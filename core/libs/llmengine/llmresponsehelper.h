/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-09
 * Description : Helper class for LLM model response operations
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QJsonObject>
#include <QByteArray>
#include <QString>

// Local includes

#include "digikam_export.h"

namespace Digikam
{

/**
 * @class LLMResponseHelper
 * @brief Helper class for LLM model response operations
 *
 * This class provides utility functions for processing LLM responses
 * and handling JSON formatting for LLM-related operations.
 */
class DIGIKAM_EXPORT LLMResponseHelper
{
public:

    /**
     * Constructor
     */
    LLMResponseHelper();

    /**
     * Destructor
     */
    ~LLMResponseHelper();
    
    /**
     * Process an LLM response and extract the JSON content
     * 
     * @param responseData The raw response data from the LLM API
     * @return A QJsonObject containing the processed result
     */
    static QJsonObject processResult(const QByteArray& responseData);

    /**
     * Ensure the JSON string is properly formatted
     * 
     * @param jsonStr The JSON string to format
     * @return A properly formatted JSON string
     */
    static QString ensureValidJsonFormat(const QString& jsonStr);

private:

    // Disable copying
    LLMResponseHelper(const LLMResponseHelper&)            = delete;
    LLMResponseHelper& operator=(const LLMResponseHelper&) = delete;
};

} // namespace Digikam