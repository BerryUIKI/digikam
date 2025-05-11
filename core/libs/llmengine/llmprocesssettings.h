/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-09
 * Description : Settings for LLM process operations
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QString>
#include <QStringList>

// Local includes

#include "digikam_export.h"

namespace Digikam
{

/**
 * @class LLMProcessSettings
 * @brief Settings for LLM process operations
 *
 * This class centralizes all settings that can be applied to LLM process managers.
 * It provides a unified way to configure different LLM engines with appropriate
 * parameters.
 */
class DIGIKAM_EXPORT LLMProcessSettings
{
public:

    /**
     * @brief Constructor with default settings
     */
    LLMProcessSettings()            = default;

    /**
     * @brief Destructor
     */
    ~LLMProcessSettings()           = default;

    /**
     * The path to the model file
     */
    QString modelPath;
    
    /**
     * The path to the mmproj file for multimodal models
     */
    QString mmprojPath;
    
    /**
     * The hostname or IP address for the API server
     */
    QString apiHost                 = QStringLiteral("127.0.0.1");
    
    /**
     * The port number for the API server
     */
    int apiPort                     = -1;
    
    /**
     * The context size in tokens
     */
    int contextSize                 = 4096;
    
    /**
     * The number of threads to use for inference
     */
    int threads                     = QThread::idealThreadCount();
    
    /**
     * The chat completions adapter to use with this LLM
     * For example: "chatml", "llama2", "openai", etc.
     */
    QString chatCompletionsAdapter  = QStringLiteral("chatml");
    
    /**
     * Additional command line arguments for the engine
     */
    QStringList additionalArgs;
    
    /**
     * @brief Check if settings are valid for starting an LLM process
     * @return True if the settings are valid
     */
    bool isValid() const;
    
    /**
     * @brief Get the reason why settings are not valid
     * @return Error message or empty string if settings are valid
     */
    QString validationError() const;
    
    /**
     * @brief Copy constructor
     * @param other The settings to copy from
     */
    LLMProcessSettings(const LLMProcessSettings& other);
    
    /**
     * @brief Assignment operator
     * @param other The settings to assign from
     * @return Reference to this object after assignment
     */
    LLMProcessSettings& operator=(const LLMProcessSettings& other);
};

} // namespace Digikam
