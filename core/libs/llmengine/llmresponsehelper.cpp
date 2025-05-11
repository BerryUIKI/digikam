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

// Qt includes

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// Local includes

#include "digikam_debug.h"
#include "llmresponsehelper.h"

namespace Digikam
{

LLMResponseHelper::LLMResponseHelper()
{
    // Nothing to do
}

LLMResponseHelper::~LLMResponseHelper()
{
    // Nothing to do
}

QJsonObject LLMResponseHelper::processResult(const QByteArray& responseData)
{
    QJsonObject resultObj;
    
    // Check if the response contains valid JSON
    QJsonParseError parseError;
    QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData, &parseError);
    
    if (parseError.error == QJsonParseError::NoError)
    {
        qCDebug(DIGIKAM_LLMENGINE_LOG) << "Response contains valid JSON data";
        
        // Extract content from the response
        QJsonObject responseObj = jsonResponse.object();
        
        // Try to get the content from various possible paths in the response JSON structure
        QString contentText;
        
        // Check for content in choices[0].message.content path (typical for OpenAI API format)
        if (responseObj.contains(QStringLiteral("choices")) && 
            responseObj[QStringLiteral("choices")].isArray() && 
            !responseObj[QStringLiteral("choices")].toArray().isEmpty())
        {
            QJsonObject messageObj = responseObj[QStringLiteral("choices")].toArray()[0].toObject()
                                    [QStringLiteral("message")].toObject();
            
            if (messageObj.contains(QStringLiteral("content")))
            {
                contentText = messageObj[QStringLiteral("content")].toString();
            }
        }
        // Also try direct content field for other possible API formats
        else if (responseObj.contains(QStringLiteral("content")))
        {
            contentText = responseObj[QStringLiteral("content")].toString();
        }
        
        // If we found content text, process it
        if (!contentText.isEmpty())
        {
            qCDebug(DIGIKAM_LLMENGINE_LOG) << "Found content in response:" << contentText;
            
            // Ensure the JSON string is properly formatted
            QString jsonStr = LLMResponseHelper::ensureValidJsonFormat(contentText);
            
            qCDebug(DIGIKAM_LLMENGINE_LOG) << "Cleaned JSON string:" << jsonStr;
            
            // Parse the extracted JSON
            QJsonParseError contentParseError;
            QJsonDocument contentJson = QJsonDocument::fromJson(jsonStr.toUtf8(), &contentParseError);
            
            if (contentParseError.error == QJsonParseError::NoError)
            {
                qCDebug(DIGIKAM_LLMENGINE_LOG) << "Successfully parsed JSON from content";
                
                // Extract the content object
                resultObj = contentJson.object();
            }
            else
            {
                qCWarning(DIGIKAM_LLMENGINE_LOG) << "Failed to parse JSON content:" 
                                                << contentParseError.errorString();
            }
        }
        else
        {
            qCWarning(DIGIKAM_LLMENGINE_LOG) << "No content found in the JSON response";
        }
    }
    else
    {
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Response does not contain valid JSON:"
                                            << parseError.errorString()
                                            << "at offset:" << parseError.offset;
        
        // Log a sample of the response for debugging
        qCDebug(DIGIKAM_LLMENGINE_LOG) << "Response sample:" << responseData.left(200) << "...";
    }
    
    // Return empty object if processing failed
    return resultObj;
}

QString LLMResponseHelper::ensureValidJsonFormat(const QString& jsonStr)
{
    QString result = jsonStr.trimmed();
    
    // Check and fix beginning of JSON
    if (!result.startsWith(QLatin1Char('{')))
    {
        // Find the first opening brace
        int startPos = result.indexOf(QLatin1Char('{'));
        if (startPos >= 0)
        {
            // Remove everything before the opening brace
            result = result.mid(startPos);
        }
        else
        {
            // No opening brace found, add one
            result.prepend(QLatin1String("{"));
        }
    }
    
    // Check and fix end of JSON
    if (!result.endsWith(QLatin1Char('}')) && result.contains(QLatin1Char('}')))
    {
        // Find the last closing brace
        int endPos = result.lastIndexOf(QLatin1Char('}'));
        if (endPos >= 0)
        {
            // Remove everything after the closing brace
            result.truncate(endPos + 1);
        }
    }
    else if (!result.endsWith(QLatin1Char('}')))
    {
        // No proper JSON closing - check for the last quote preceded by alphanumeric character
        // which likely indicates the end of the last keyword in the array
        for (int i = result.length() - 1; i >= 0; --i)
        {
            if (result.at(i) == QLatin1Char('"') && i > 0)
            {
                QChar precedingChar = result.at(i - 1);
                if (precedingChar.isLetterOrNumber())
                {
                    // Found the last quote preceded by an alphanumeric character
                    // Truncate and close the JSON properly
                    result.truncate(i + 1);
                    result.append(QLatin1String("]}"));
                    break;
                }
            }
        }
        
        // If we still don't have a proper JSON closing, add it
        if (!result.endsWith(QLatin1Char('}')))
        {
            result.append(QLatin1String("]}"));
        }
    }
    
    qCDebug(DIGIKAM_LLMENGINE_LOG) << "JSON formatted string:" << result;
    return result;
}

} // namespace Digikam
