/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-06
 * Description : Performs autotags using OpenAI models
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes
#include <QNetworkAccessManager>

// Local includes

#include "digikam_export.h"
#include "applicationsettings.h"
#include "dnnmodelbase.h"
#include "dnnmodelmanager.h"
#include "autotagspipelinebase.h"
#include "llmresponsehelper.h"
#include "llmprocessmanager.h"
#include "llmprocessmanagerkoboldcpp.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT AnnotatePipelineOpenAI : public AutotagsPipelineBase
{
    Q_OBJECT

public:

    explicit AnnotatePipelineOpenAI(const AutotagsScanSettings& settings);
    ~AnnotatePipelineOpenAI() override;

    bool start() override;
Q_SIGNALS:

    void signalStartupMessage(const QString& message);


protected:
    enum LLMResponses
    {
        LLMDescription                      = 1 << 0,
        LLMKeywords                         = 1 << 1,
        LLMdescriptionAndKeywords           = LLMDescription | LLMKeywords
    };

    const int       minImageSize            = 312;
    const int       jpegQuality             = 95;
    const char*     base64ImageType         = "JPEG";
    QString         llmPrompt               = AIAnnotate::DEFAULT_PROMPT;
    QString         apiKey;
    const int       llmStartupTimeout       = 300;          /// seconds
    bool            llmReady                = false;
    bool            running                 = false;
    LLMResponses    llmResponseType         = LLMResponses::LLMdescriptionAndKeywords;

    // LLM model parameters
    int             maxTokens               = AIAnnotate::DEFAULT_MAX_TOKENS;
    float           temperature             = AIAnnotate::DEFAULT_TEMPERATURE;
    const float     topP                    = 0.95F;
    const int       topK                    = 40;
    const float     minP                    = 0.05F;

    // LLM Process Manager and settings
    LLMProcessSettings                          m_llmSettings;
    LLMProcessManagerKoboldCpp*                 m_llmProcessManager;

    QList<qlonglong>                            m_imageIdsInProgress;
    QMutex                                      m_progressIdsMutex;

    bool finder() override;
    bool loader() override;
    bool extractor() override;
    bool classifier() override;
    bool trainer() override { return false; }
    bool writer() override;
    
    void cancel() override;
    void addMoreWorkers() override;

    /**
     * @brief Prepares the payload for the OpenAI API request
     * @param prompt The LLM prompt to include in the request
     * @param base64Image The base64 encoded image to include in the request
     * @return The JSON object containing the prepared payload
     */
    QJsonObject createPayload(const QString& prompt, const QString& base64Image) const;

    /**
     * @brief Creates the LLM prompt based on SystemSettings
     * @return The generated prompt string
     */
    QString createPrompt() const;

private Q_SLOTS:

    /**
     * @brief Handles engineStarted signal from LLMProcessManager
     * @param success Whether the engine started successfully
     */
    void slotEngineStarted(bool success);

    /**
     * @brief Handles engineReadyChanged signal from LLMProcessManager
     * @param ready Whether the engine is ready for inference
     */
    void slotEngineReady();

    /**
     * @brief Checks if the LLM engine is ready for inference
     * This slot can be connected to a timer to periodically check engine readiness
     */
    void checkLLMEngineReady();

private:

    // Disable
    AnnotatePipelineOpenAI() = delete;
    AnnotatePipelineOpenAI(QObject*) = delete;
    AnnotatePipelineOpenAI(const AnnotatePipelineOpenAI&) = delete;
};

} // namespace Digikam