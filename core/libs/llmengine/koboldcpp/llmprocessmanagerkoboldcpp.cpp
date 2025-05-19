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

// Qt includes

#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslConfiguration>
#include <QThread>
#include <QTimer>
#include <QEventLoop>

// Local includes

#include "digikam_debug.h"
#include "llmprocessmanagerkoboldcpp.h"
#include "llmresponsehelper.h"

namespace Digikam
{

LLMProcessManagerKoboldCpp::LLMProcessManagerKoboldCpp(const LLMProcessSettings& settings, QObject* parent)
    : LLMProcessManager(settings, parent)
{
    if (!m_settings.external)
    {
        // local engine settings

        m_settings.apiPort = findUnusedTcpPort();
        m_settings.apiHost = QStringLiteral("127.0.0.1");
        m_engineType = LLMProcessSettings::LLMEngineType::LlamaCpp;
    }
    else
    {
        // external engine settings

        m_engineType = m_settings.engineType;
    }
}

LLMProcessManagerKoboldCpp::~LLMProcessManagerKoboldCpp()
{
}

bool LLMProcessManagerKoboldCpp::startEngine()
{
    QStringList args;

    // skip creating the process if it is external

    if (!m_settings.external)
    {

        if (!m_settings.isValid() || m_settings.mmprojPath.isEmpty())
        {
            qCWarning(DIGIKAM_LLMENGINE_LOG) << "Cannot start KoboldCpp engine: invalid settings";
            return false;
        }

        // add normal arguments

        if (LLMProcessSettings::LLMEngineType::KoboldCpp == m_engineType)
        {
            // add the model path

            args << m_settings.modelPath;

            // add the mmproj path if it exists

            if (!m_settings.mmprojPath.isEmpty())
            {
                args << QStringLiteral("--mmproj")              << m_settings.mmprojPath;
            }
            args << QStringLiteral("--contextsize")             << QString::number(m_settings.contextSize)
                 << QStringLiteral("--chatcompletionsadapter")  << m_settings.chatCompletionsAdapter
                 << QStringLiteral("--visionmaxres")            << QStringLiteral("2048");
        }
        else if (LLMProcessSettings::LLMEngineType::LlamaCpp == m_engineType)
        {
            args << QStringLiteral("--model")                   << QStringLiteral("/Users/michmill/Projects/ImageIndexer/resources/google_gemma-3-4b-it-Q6_K.gguf")
                 << QStringLiteral("--mmproj")                  << QStringLiteral("/Users/michmill/Projects/ImageIndexer/resources/mmproj-google_gemma-3-4b-it-f16.gguf") 
                 << QStringLiteral("--ctx-size")                << QString::number(m_settings.contextSize);
        }

        args << QStringLiteral("--host")                        << m_settings.apiHost    
             << QStringLiteral("--port")                        << QString::number(m_settings.apiPort);

        // used for testing engine startup failure
        // args << QStringLiteral("--badparam");

        // add additional arguments if any
        
        if (m_settings.additionalArgs.size() > 0)
        {
            args << m_settings.additionalArgs;
        }

        if (LLMProcessSettings::LLMEngineType::KoboldCpp == m_engineType)
        {
            // koboldcpp path

            m_executablePath = QStringLiteral("/Users/michmill/Projects/ImageIndexer/resources/koboldcpp-1_90_2");
        }
        else if (LLMProcessSettings::LLMEngineType::LlamaCpp == m_engineType)
        {
            // llamacpp path

            m_executablePath = QStringLiteral("/Users/michmill/Projects/llamacpp/bin/llama-server");
        }
    }
    
    return LLMProcessManager::startEngine(args);
}

void LLMProcessManagerKoboldCpp::abortInference()
{
    if (!isEngineRunning())
    {
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Cannot abort inference: KoboldCpp engine is not running";
        return;
    }
    
    // Prepare the payload for the abort request

    QJsonObject jsonPayload;
    jsonPayload[QStringLiteral("abort")] = true;
    
    // Submit the request to the abort endpoint
    // Use force=true to skip the engine ready check, as we want to be able to abort
    // even if the engine is busy or in a non-ready state

    QByteArray response = submitRequest(QStringLiteral("/api/extra/abort"), jsonPayload, POST, true);
    
    // Log the result

    if (!response.isEmpty())
    {
        qCDebug(DIGIKAM_LLMENGINE_LOG) << "Successfully aborted KoboldCpp inference";
    }
    else
    {
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Failed to abort KoboldCpp inference";
    }
}

bool LLMProcessManagerKoboldCpp::checkEngineReady()
{
    if (!isEngineRunning())
    {
        // Not running, so definitely not ready

        return false;
    }

    // Create an empty payload for the GET request

    QJsonObject emptyPayload;

    // Submit the request to the version endpoint with force=true to avoid circular dependency
    // (since submitRequest itself calls engineReady when force is false)

    QString healthUrl;

    if (LLMProcessSettings::LLMEngineType::KoboldCpp == m_engineType)
    {
        // koboldcpp check

        healthUrl = QStringLiteral("/api/extra/version");
    }
    else if (LLMProcessSettings::LLMEngineType::LlamaCpp == m_engineType)
    {
        // llamacpp check

        healthUrl = QStringLiteral("/health");
    }

    QByteArray response = submitRequest(healthUrl, emptyPayload, GET, true);


    // Check if the response indicates the model is ready

    bool ready = false;

    if (!response.isEmpty())
    {
        // Parse the response as JSON

        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject jsonResponse = doc.object();
        
        // Check if the model is loaded and ready

        if (jsonResponse.contains(QStringLiteral("status")))
        {
            // llamacpp check

            QString status = jsonResponse[QStringLiteral("status")].toString();
            ready = (status.toLower() == QStringLiteral("ok"));
            
            qCDebug(DIGIKAM_LLMENGINE_LOG) << "LlamaCpp LLM status:" << jsonResponse;
        }
        else if (jsonResponse.contains(QStringLiteral("result")))
        {
            // koboldcpp check

            QString status = jsonResponse[QStringLiteral("result")].toString();
            ready = (status.toLower() == QStringLiteral("koboldcpp"));
            
            qCDebug(DIGIKAM_LLMENGINE_LOG) << "KoboldCpp LLM status:" << jsonResponse;
        }
    }
    else
    {
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "LLM status check failed: empty response";
    }

    return ready;
}

QByteArray LLMProcessManagerKoboldCpp::submitRequest(const QString& function, const QJsonObject& payload, 
                                           LLMRequestMethod method, bool force)
{
    if (!isEngineRunning())
    {
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Cannot submit request: KoboldCpp engine is not running";
        return QByteArray();
    }

    if (!force && !engineReady())
    {
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Cannot submit request: KoboldCpp engine is not ready for inference";
        return QByteArray();
    }

    // Create a local network manager for this request

    QNetworkAccessManager networkManager;
    QEventLoop loop;
    QByteArray responseData;
    bool requestError = false;

    QString baseUrl = createBaseUrl();

    if (QLatin1Char('/') != function.at(0))
    {
        baseUrl.append(QLatin1Char('/'));
    }

    // Prepare the URL for the API endpoint
    QUrl url(baseUrl + function);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    // Convert payload to JSON
    QJsonDocument doc(payload);
    QByteArray data = doc.toJson();

    // Submit the request
    qCDebug(DIGIKAM_LLMENGINE_LOG) << "Submitting request to" << url.toString() 
                                   << "using" << (method == GET ? "GET" : "POST") << "method";
    QNetworkReply* reply = nullptr;

    if (GET == method)
    {
        reply = networkManager.get(request);
    }
    else // POST is default
    {
        reply = networkManager.post(request, data);
    }

    // Connect to handle the response
    QObject::connect(reply, &QNetworkReply::finished, [&]()
    {
        if (reply->error() == QNetworkReply::NoError)
        {
            responseData = reply->readAll();
            qCDebug(DIGIKAM_LLMENGINE_LOG) << "Received response (" << responseData.size() << " bytes)";
        }
        else
        {
            requestError = true;
            qCWarning(DIGIKAM_LLMENGINE_LOG) << "Request failed:" << reply->errorString();
        }

        reply->deleteLater();
        loop.quit();
    });

    // Wait for the request to complete with a timeout

    QTimer::singleShot(m_apiTimeout, &loop, &QEventLoop::quit);
    loop.exec();

    // If there was an error or timeout, return an empty byte array
    if (requestError || responseData.isEmpty())
    {
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Failed to get valid response from LLM";
        return QByteArray();
    }

    // Return the raw response data directly
    return responseData;
}

QString LLMProcessManagerKoboldCpp::createBaseUrl() const
{
    QString baseUrl;
    if (m_settings.external)
    {        
        baseUrl = m_settings.apiHost;

        // check if the host ends with a slash

        if (baseUrl.endsWith(QLatin1Char('/')))
        {
            baseUrl.chop(1);
        }

        // add the port if it is not the default

        if (0 < m_settings.apiPort)
        {
            baseUrl += QLatin1String(":%1").arg(QString::number(m_settings.apiPort));
        }
    }
    else
    {
        baseUrl = QString::fromLatin1("http://%1:%2").arg(m_settings.apiHost).arg(m_settings.apiPort);
    }

    return baseUrl;
}

} // namespace Digikam

#include "moc_llmprocessmanagerkoboldcpp.cpp"
