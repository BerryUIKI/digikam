/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-06
 * Description : Performs autotags using KoboldLLM models
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Local includes

#include "digikam_export.h"
#include "dnnmodelbase.h"
#include "dnnmodelmanager.h"
#include "autotagspipelinebase.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT AutotagsPipelineKoboldLLM : public AutotagsPipelineBase
{
    Q_OBJECT

public:

    explicit AutotagsPipelineKoboldLLM(const AutotagsScanSettings& settings);
    ~AutotagsPipelineKoboldLLM() override;

    bool start() override;

protected:

    const int   minImageSize                    = 312;
    const int   jpegQuality                     = 95;
    const char* base64ImageType                 = "JPEG";
    QString     llmPrompt;

    // LLM model parameters
    const int   maxTokens                       = 250;
    const float temperature                     = 0.1f;
    const int   topP                            = 1;
    const int   topK                            = 0;
    const float minP                            = 1.05f;

    const QString apiURL                      = QLatin1String("http://localhost:5001");
    const QString apiPassword                 = QLatin1String("");

    bool finder() override;
    bool loader() override;
    bool extractor() override;
    bool classifier() override;
    bool trainer() override { return false; }
    bool writer() override;

    void addMoreWorkers() override;

    /**
     * @brief Prepares the payload for the KoboldLLM API request
     * @param prompt The LLM prompt to include in the request
     * @param base64Image The base64 encoded image to include in the request
     * @return The JSON object containing the prepared payload
     */
    QJsonObject createPayload(const QString& prompt, const QString& base64Image);

    /**
     * @brief Processes the API response and extracts Description and Keywords
     * @param responseData The raw response data from the API call
     * @return QJsonObject containing Description and Keywords if successful, empty object if failed
     */
    QJsonObject processResult(const QByteArray& responseData);

    /**
     * @brief Ensures a string has valid JSON formatting at the beginning and end
     * @param jsonStr The string to check and modify
     * @return The string with valid JSON formatting
     */
    QString ensureValidJsonFormat(const QString& jsonStr);

private:

    // Disable
    AutotagsPipelineKoboldLLM() = delete;
    AutotagsPipelineKoboldLLM(QObject*) = delete;
    AutotagsPipelineKoboldLLM(const AutotagsPipelineKoboldLLM&) = delete;
};

} // namespace Digikam