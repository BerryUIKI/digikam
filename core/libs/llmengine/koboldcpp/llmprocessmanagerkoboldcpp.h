/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-09
 * Description : Manager for KoboldCpp process operations
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QNetworkReply>

// Local includes

#include "digikam_export.h"
#include "llmprocessmanager.h"

namespace Digikam
{

/**
 * @class LLMProcessManagerKoboldCpp
 * @brief Manager for KoboldCpp process operations
 *
 * This class extends LLMProcessManager to provide specific functionality
 * for controlling and interacting with the KoboldCpp engine.
 */
class DIGIKAM_EXPORT LLMProcessManagerKoboldCpp : public LLMProcessManager
{
    Q_OBJECT

public:

    /**
     * @brief Constructor
     * @param settings The LLM process settings
     * @param parent The parent QObject
     */
    explicit LLMProcessManagerKoboldCpp(const LLMProcessSettings& settings, QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~LLMProcessManagerKoboldCpp() override;

    /**
     * @brief Start the KoboldCpp engine asynchronously
     * @return True if the engine launch was initiated successfully
     * @note Actual startup result will be communicated via engineStarted signal
     */
    bool startEngine() override;
    
    /**
     * @brief Check if the KoboldCpp LLM is ready for inference
     * @return True if the LLM is ready to accept inference requests
     */
    bool checkEngineReady() override;
    
    /**
     * @brief Submit a request to the KoboldCpp API
     * @param function The function name or API endpoint to call
     * @param payload The JSON payload for the request
     * @param method The HTTP method to use (GET or POST)
     * @param force If true, skip the engine ready check
     * @return A QByteArray containing the raw response data from the LLM
     */
    QByteArray submitRequest(const QString& function, const QJsonObject& payload,
                            LLMRequestMethod method, bool force = false) override;

    /**
     * @brief Get the base path for the Open AI engine
     */
    static const QString getEnginePath()
    {
        return LLMProcessManager::getBaseEnginePath() + QLatin1String("/llamacpp");
    }

    /**
     * @brief the type of LLM engine
     */
    LLMProcessSettings::LLMEngineType m_engineType  = LLMProcessSettings::LLMEngineType::LlamaCpp;
        
public Q_SLOTS:

    /**
     * @brief Abort a running inference
     * Implementation for KoboldCpp specific abort mechanism
     */
    void abortInference() override;

protected:

    /**
     * @brief network reply object for handling API responses
     * @note this is NOT set if force=true for the request
     */
    QNetworkReply** m_replyPtr                      = nullptr;

    /**
     * @brief Create the base URL for API requests
     * @return The base URL for the KoboldCpp API
     * @note This method constructs the base URL using the host and port settings
     */
    QString createBaseUrl() const;
    
};

} // namespace Digikam
