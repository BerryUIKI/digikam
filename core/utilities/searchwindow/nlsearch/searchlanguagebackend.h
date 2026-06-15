/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Abstract inference interface for the natural-language
 *               search feature. Hides the actual model runtime
 *               (llama.cpp / ONNX / mock) behind a Qt async API so the
 *               rest of the pipeline stays backend-independent.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

#include "digikam_export.h"

// Qt includes

#include <QObject>
#include <QString>

namespace Digikam
{

class DIGIKAM_GUI_EXPORT SearchLanguageBackend : public QObject
{
    Q_OBJECT

public:

    explicit SearchLanguageBackend(QObject* const parent = nullptr);
    ~SearchLanguageBackend() override;

    /**
     * Load the model from a local path (resolved by SearchNlModelManager).
     * Implementations should perform heavy loading off the GUI thread and
     * report completion through modelLoaded().
     * Returns false immediately if the path is invalid.
     */
    virtual bool loadModel(const QString& modelPath) = 0;

    virtual void unloadModel()                       = 0;
    virtual bool isModelLoaded()               const = 0;
    virtual QString modelPath()                const = 0;

    // Human-readable backend identifier ("llama.cpp", "onnx", "mock").
    virtual QString backendName()              const = 0;

public Q_SLOTS:

    /**
     * Run inference asynchronously. Result is delivered via
     * rawOutputReady() or inferenceError(). Implementations must be
     * safe to call from the GUI thread and must not block it.
     */
    virtual void runInference(const QString& prompt) = 0;

    virtual void cancel();

Q_SIGNALS:

    void rawOutputReady(const QString& output);
    void inferenceError(const QString& errorMessage);
    void modelLoaded(bool success);
    void inferenceProgress(int step);
};

} // namespace Digikam
