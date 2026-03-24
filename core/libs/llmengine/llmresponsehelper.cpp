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
#include <QStringList>

// Local includes

#include "digikam_debug.h"
#include "llmresponsehelper.h"

namespace Digikam
{

namespace
{

QList<QJsonObject> extractJsonObjectsFromText(const QByteArray& responseData)
{
    QList<QJsonObject> result;

    // First try a regular single JSON payload.
    {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

        if ((parseError.error == QJsonParseError::NoError) && doc.isObject())
        {
            result << doc.object();
            return result;
        }
    }

    const QString text = QString::fromUtf8(responseData);

    // Try SSE payload format where each event is prefixed with "data:".
    const QStringList lines = text.split(QLatin1Char('\n'));

    for (const QString& rawLine : lines)
    {
        const QString line = rawLine.trimmed();

        if (!line.startsWith(QLatin1String("data:")))
        {
            continue;
        }

        const QString payload = line.mid(5).trimmed();

        if (payload.isEmpty() || (payload == QLatin1String("[DONE]")))
        {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);

        if ((parseError.error == QJsonParseError::NoError) && doc.isObject())
        {
            result << doc.object();
        }
    }

    if (!result.isEmpty())
    {
        return result;
    }

    // Fallback: parse concatenated JSON objects by tracking brace depth.
    const QByteArray bytes = responseData;
    int start = -1;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (int i = 0 ; i < bytes.size() ; ++i)
    {
        const char c = bytes.at(i);

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (inString)
        {
            if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                inString = false;
            }

            continue;
        }

        if (c == '"')
        {
            inString = true;
            continue;
        }

        if (c == '{')
        {
            if (depth == 0)
            {
                start = i;
            }

            ++depth;
        }
        else if ((c == '}') && (depth > 0))
        {
            --depth;

            if ((depth == 0) && (start >= 0))
            {
                const QByteArray chunk = bytes.mid(start, i - start + 1);
                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson(chunk, &parseError);

                if ((parseError.error == QJsonParseError::NoError) && doc.isObject())
                {
                    result << doc.object();
                }

                start = -1;
            }
        }
    }

    return result;
}

QString extractContentFromResponseObjects(const QList<QJsonObject>& responseObjects)
{
    QString latestMessageContent;
    QString streamedContent;

    for (const QJsonObject& responseObj : responseObjects)
    {
        if (!responseObj.contains(QStringLiteral("choices")) ||
            !responseObj[QStringLiteral("choices")].isArray() ||
            responseObj[QStringLiteral("choices")].toArray().isEmpty())
        {
            if (responseObj.contains(QStringLiteral("content")) &&
                responseObj[QStringLiteral("content")].isString())
            {
                latestMessageContent = responseObj[QStringLiteral("content")].toString();
            }

            continue;
        }

        const QJsonObject choiceObj = responseObj[QStringLiteral("choices")].toArray().first().toObject();

        if (choiceObj.contains(QStringLiteral("message")) && choiceObj[QStringLiteral("message")].isObject())
        {
            const QJsonObject messageObj = choiceObj[QStringLiteral("message")].toObject();

            if (messageObj.contains(QStringLiteral("content")) && messageObj[QStringLiteral("content")].isString())
            {
                latestMessageContent = messageObj[QStringLiteral("content")].toString();
            }
        }

        if (choiceObj.contains(QStringLiteral("delta")) && choiceObj[QStringLiteral("delta")].isObject())
        {
            const QJsonObject deltaObj = choiceObj[QStringLiteral("delta")].toObject();

            if (deltaObj.contains(QStringLiteral("content")) && deltaObj[QStringLiteral("content")].isString())
            {
                streamedContent.append(deltaObj[QStringLiteral("content")].toString());
            }
        }
    }

    return latestMessageContent.isEmpty() ? streamedContent : latestMessageContent;
}

} // namespace

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

    const QList<QJsonObject> responseObjects = extractJsonObjectsFromText(responseData);

    if (!responseObjects.isEmpty())
    {
        qCDebug(DIGIKAM_LLMENGINE_LOG) << "Parsed" << responseObjects.size() << "JSON response object(s)";

        const QString contentText = extractContentFromResponseObjects(responseObjects);

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
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Response does not contain parseable JSON object(s)";
        
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
