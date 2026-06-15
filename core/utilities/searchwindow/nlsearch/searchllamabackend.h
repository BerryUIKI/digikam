/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : llama.cpp-based inference backend (GGUF models).
 *               First concrete backend.
 *               TinyLlama 1.1B Q4 as baseline, Qwen2.5-1.5B-Instruct
 *               as primary candidate. Inference runs on a worker
 *               thread; results are delivered via queued signals.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QThread>
#include <QAtomicInteger>

// Local includes

#include "searchlanguagebackend.h"
#include "digikam_config.h"     // for HAVE_LLAMACPP

namespace Digikam
{

class SearchLlamaWorker;

class SearchLlamaBackend : public SearchLanguageBackend
{
    Q_OBJECT

public:

    explicit SearchLlamaBackend(QObject* const parent = nullptr);
    ~SearchLlamaBackend() override;

    bool loadModel(const QString& modelPath)   override;
    void unloadModel()                         override;
    bool isModelLoaded()                 const override;
    QString modelPath()                  const override;
    QString backendName()                const override;

    void setMaxTokens(int maxTokens);
    void setTemperature(float temperature);

public Q_SLOTS:

    void runInference(const QString& prompt) override;
    void cancel()                            override;

Q_SIGNALS:

    void requestLoad(const QString& path);
    void requestInference(const QString& prompt, int maxTokens, float temperature);

private:

    QThread             m_workerThread;
    SearchLlamaWorker*  m_worker      = nullptr;
    QString             m_modelPath;
    bool                m_modelLoaded = false;
    int                 m_maxTokens   = 256;

    // Sampling temperature for token generation. 0.0 = greedy/deterministic
    // decoding which always pick the most-probable token, which is what we want:
    // the model must emit structured, parseable JSON, so reproducible output
    // matters more than creative variation. Higher values would add randomness.
    float               m_temperature = 0.0F;  
};


/**
 * Worker object living on the worker thread. Owns the llama.cpp model
 * and context. All llama_* calls happen here, never on the GUI thread.
 */
class SearchLlamaWorker : public QObject
{
    Q_OBJECT

public:

    explicit SearchLlamaWorker(QObject* const parent = nullptr);
    ~SearchLlamaWorker() override;

    void requestCancel();

public Q_SLOTS:

    void doLoad(const QString& modelPath);
    void doInference(const QString& prompt, int maxTokens, float temperature);
    void doUnload();

Q_SIGNALS:

    void loaded(bool success);
    void outputReady(const QString& output);
    void error(const QString& message);
    void progress(int step);

private:

#ifdef HAVE_LLAMACPP

    void*                  m_model   = nullptr;
    void*                  m_context = nullptr;

#endif

    QAtomicInteger<int>    m_cancelRequested;
};

} // namespace Digikam
