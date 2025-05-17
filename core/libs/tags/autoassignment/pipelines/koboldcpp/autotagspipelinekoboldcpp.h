/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-06
 * Description : Performs autotags using KoboldCpp models
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
#include "dnnmodelbase.h"
#include "dnnmodelmanager.h"
#include "autotagspipelinebase.h"
#include "llmresponsehelper.h"
#include "llmprocessmanager.h"
#include "llmprocessmanagerkoboldcpp.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT AutotagsPipelineKoboldCpp : public AutotagsPipelineBase
{
    Q_OBJECT

public:

    explicit AutotagsPipelineKoboldCpp(const AutotagsScanSettings& settings);
    ~AutotagsPipelineKoboldCpp() override;

    bool start() override;
Q_SIGNALS:

    void signalStartupMessage(const QString& message);

    // const QString defaultPrompt = i18n("Describe the image in detail. Include any relevant keywords or tags that may apply to the image. Please separate each keyword with a comma.");

protected:
    enum LLMResponses
    {
        LLMDescription                  = 1 << 0,
        LLMKeywords                     = 1 << 1,
        LLMdescriptionAndKeywords       = LLMDescription | LLMKeywords
    };

    const int       minImageSize                    = 312;
    const int       jpegQuality                     = 95;
    const char*     base64ImageType                 = "JPEG";
    QString         llmPrompt;
    QString         apiKey;
    const int       llmStartupTimeout               = 300;          /// seconds
    bool            llmReady                        = false;
    bool            running                         = false;
    LLMResponses    llmResponseType                 = LLMResponses::LLMdescriptionAndKeywords;

    // LLM model parameters
    const int   maxTokens                       = 250;
    const float temperature                     = 0.1f;
    const int   topP                            = 1;
    const int   topK                            = 0;
    const float minP                            = 1.05f;

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
     * @brief Prepares the payload for the KoboldCpp API request
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
    AutotagsPipelineKoboldCpp() = delete;
    AutotagsPipelineKoboldCpp(QObject*) = delete;
    AutotagsPipelineKoboldCpp(const AutotagsPipelineKoboldCpp&) = delete;
};

} // namespace Digikam