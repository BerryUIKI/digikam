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

    slotDoUnload();

    // TODO: expose n_ctx / n_threads as settings.

    Q_UNUSED(modelPath);

    const bool ok = (m_model != nullptr) && (m_context != nullptr);
    Q_EMIT signalLoaded(ok);

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

    // TODO: implement the decode loop.

    Q_UNUSED(prompt);
    Q_UNUSED(maxTokens);
    Q_UNUSED(temperature);

    Q_EMIT signalError(QLatin1String("llama.cpp decode loop not implemented yet."));

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

    // if (m_context) { llama_free((llama_context*)m_context);       m_context = nullptr; }
    // if (m_model)   { llama_model_free((llama_model*)m_model);     m_model   = nullptr; }

#endif

}

} // namespace Digikam

#include "moc_searchllamabackend.cpp"
