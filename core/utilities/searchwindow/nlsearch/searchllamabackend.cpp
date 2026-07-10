/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchllamabackend.h"

// Qt includes

#include <QFileInfo>
#include <vector>
#include <QElapsedTimer>

// Local includes

#include "digikam_debug.h"

#ifdef HAVE_LLAMACPP
#   include <llama.h>
#endif

namespace Digikam
{

SearchLlamaBackend::SearchLlamaBackend(QObject* const parent)
    : SearchLanguageBackend(parent)
{
    m_worker = new SearchLlamaWorker;
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    connect(this, &SearchLlamaBackend::signalRequestLoad,
            m_worker, &SearchLlamaWorker::slotDoLoad);

    connect(this, &SearchLlamaBackend::signalRequestInference,
            m_worker, &SearchLlamaWorker::slotDoInference);

    connect(m_worker, &SearchLlamaWorker::signalLoaded,
            this, [this](bool ok)
        {
            m_modelLoaded = ok;
            Q_EMIT signalModelLoaded(ok);
        });

    connect(m_worker, &SearchLlamaWorker::signalOutputReady,
            this, &SearchLlamaBackend::signalRawOutputReady);

    connect(m_worker, &SearchLlamaWorker::signalError,
            this, &SearchLlamaBackend::signalInferenceError);

    connect(m_worker, &SearchLlamaWorker::signalProgress,
            this, &SearchLlamaBackend::signalInferenceProgress);

    m_workerThread.start();
}

SearchLlamaBackend::~SearchLlamaBackend()
{
    m_worker->requestCancel();
    m_workerThread.quit();
    m_workerThread.wait();
}

bool SearchLlamaBackend::loadModel(const QString& modelPath)
{
    if (!QFileInfo::exists(modelPath))
    {
        qCWarning(DIGIKAM_NLSEARCH_LOG) << "NL search: model file not found:" << modelPath;

        return false;
    }

    m_modelPath = modelPath;

    Q_EMIT signalRequestLoad(modelPath);

    return true;
}

void SearchLlamaBackend::unloadModel()
{
    QMetaObject::invokeMethod(m_worker, "slotDoUnload", Qt::QueuedConnection);
    m_modelLoaded = false;
}

bool SearchLlamaBackend::isModelLoaded() const
{
    return m_modelLoaded;
}

QString SearchLlamaBackend::modelPath() const
{
    return m_modelPath;
}

QString SearchLlamaBackend::backendName() const
{
    return QLatin1String("llama.cpp");
}

void SearchLlamaBackend::setMaxTokens(int maxTokens)
{
    m_maxTokens = maxTokens;
}

void SearchLlamaBackend::setTemperature(float temperature)
{
    m_temperature = temperature;
}

void SearchLlamaBackend::slotRunInference(const QString& prompt)
{
    if (!m_modelLoaded)
    {
        Q_EMIT signalInferenceError(QLatin1String("Model is not loaded."));

        return;
    }

    Q_EMIT signalRequestInference(prompt, m_maxTokens, m_temperature);
}

void SearchLlamaBackend::slotCancel()
{
    m_worker->requestCancel();
}

SearchLlamaWorker::SearchLlamaWorker(QObject* const parent)
    : QObject(parent)
{
}

SearchLlamaWorker::~SearchLlamaWorker()
{
    slotDoUnload();
}

void SearchLlamaWorker::requestCancel()
{
    m_cancelRequested.storeRelaxed(1);
}

void SearchLlamaWorker::slotDoLoad(const QString& modelPath)
{

#ifdef HAVE_LLAMACPP

    slotDoUnload();  // clear any previously loaded model

    // Initialize the llama.cpp backend
    llama_backend_init();

    // Model parameters: CPU-only
    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers       = 0;

    const QByteArray pathUtf8  = modelPath.toUtf8();
    llama_model* const model   = llama_model_load_from_file(pathUtf8.constData(), mparams);

    if (!model)
    {
        qCWarning(DIGIKAM_NLSEARCH_LOG) << "NL search: failed to load model:" << modelPath;
        Q_EMIT signalError(QLatin1String("Failed to load the language model."));
        Q_EMIT signalLoaded(false);

        return;
    }

    // Context window large enough for the prompt and the structured output, and thread counts sized to the machine.
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx                = 4096;
    cparams.n_threads            = QThread::idealThreadCount();
    cparams.n_threads_batch      = QThread::idealThreadCount();

    llama_context* const ctx     = llama_init_from_model(model, cparams);

    if (!ctx)
    {
        qCWarning(DIGIKAM_NLSEARCH_LOG) << "NL search: failed to create llama context";
        llama_model_free(model);
        Q_EMIT signalError(QLatin1String("Failed to initialize the model context."));
        Q_EMIT signalLoaded(false);

        return;
    }

    m_model   = model;
    m_context = ctx;

    Q_EMIT signalLoaded(true);

#else

    Q_UNUSED(modelPath);
    Q_EMIT signalError(QLatin1String("digiKam was built without llama.cpp support."));
    Q_EMIT signalLoaded(false);

#endif

}

void SearchLlamaWorker::slotDoInference(const QString& prompt, int maxTokens, float temperature)
{
    m_cancelRequested.storeRelaxed(0);

#ifdef HAVE_LLAMACPP

    Q_UNUSED(temperature);  // greedy decoding, determinism preferred for structured output

    if (!m_model || !m_context)
    {
        Q_EMIT signalError(QLatin1String("Model is not loaded."));

        return;
    }

    llama_model*       const model = static_cast<llama_model*>(m_model);
    llama_context*     const ctx   = static_cast<llama_context*>(m_context);
    const llama_vocab* const vocab = llama_model_get_vocab(model);

    // 1. Tokenize the prompt
    const QByteArray promptUtf8 = prompt.toUtf8();

    const int n_prompt = -llama_tokenize(vocab, promptUtf8.constData(), promptUtf8.size(),
                                         nullptr, 0, true, true);

    if (n_prompt <= 0)
    {
        Q_EMIT signalError(QLatin1String("Failed to tokenize the prompt."));

        return;
    }

    std::vector<llama_token> tokens(n_prompt);

    if (llama_tokenize(vocab, promptUtf8.constData(), promptUtf8.size(),
                       tokens.data(), tokens.size(), true, true) < 0)
    {
        Q_EMIT signalError(QLatin1String("Failed to tokenize the prompt."));

        return;
    }

    // 2. Greedy sampler (deterministic)
    llama_sampler* const smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    // 3. Decode the prompt, then generate token by token
    QString     result;
    llama_batch batch     = llama_batch_get_one(tokens.data(), tokens.size());
    int         generated = 0;

    while (generated < maxTokens)
    {
        if (m_cancelRequested.loadRelaxed())
        {
            break;
        }

        if (llama_decode(ctx, batch) != 0)
        {
            llama_sampler_free(smpl);
            Q_EMIT signalError(QLatin1String("Model decode failed."));

            return;
        }

        const llama_token newToken = llama_sampler_sample(smpl, ctx, -1);

        if (llama_vocab_is_eog(vocab, newToken))
        {
            break;
        }

        char      piece[256] = { 0 };
        const int n = llama_token_to_piece(vocab, newToken, piece, sizeof(piece), 0, true);

        if (n > 0)
        {
            result += QString::fromUtf8(piece, n);
        }

        Q_EMIT signalProgress(++generated);

        
        const QString trimmed = result.trimmed();
        int depth = 0;
        bool sawOpen = false; 
        bool balanced = false;

        for (const QChar ch : trimmed)
        {
            if      (ch == QLatin1Char('{')) { ++depth; sawOpen = true; }
            else if (ch == QLatin1Char('}')) { --depth; }
        }

        balanced = (sawOpen && depth == 0);

        if (balanced)
        {
            break;
        }
        
        m_singleToken = newToken;
        batch         = llama_batch_get_one(&m_singleToken, 1);
    }

    llama_sampler_free(smpl);

    Q_EMIT signalOutputReady(result);

#else

    Q_UNUSED(prompt);
    Q_UNUSED(maxTokens);
    Q_UNUSED(temperature);
    Q_EMIT signalError(QLatin1String("digiKam was built without llama.cpp support."));

#endif
}

void SearchLlamaWorker::slotDoUnload()
{

#ifdef HAVE_LLAMACPP

    if (m_context)
    {
        llama_free(static_cast<llama_context*>(m_context));
        m_context = nullptr;
    }

    if (m_model)
    {
        llama_model_free(static_cast<llama_model*>(m_model));
        m_model = nullptr;
    }

#endif

}

} // namespace Digikam

#include "moc_searchllamabackend.cpp"
