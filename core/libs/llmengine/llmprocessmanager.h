/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-09
 * Description : Manager for LLM process operations
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QObject>
#include <QString>
#include <QProcess>
#include <QThread>
#include <QJsonObject>
#include <QTimer>

// Local includes

#include "digikam_export.h"
#include "llmprocesssettings.h"

namespace Digikam
{

/**
 * @class LLMProcessManager
 * @brief Manager for LLM engine processes
 *
 * This class provides utility functions for launching and managing
 * LLM engine processes such as llamacpp and koboldcpp.
 * It handles starting and stopping the processes, checking their status,
 * and aborting running inferences.
 */
class DIGIKAM_EXPORT LLMProcessManager : public QObject
{
    Q_OBJECT

public:

    enum LLMRequestMethod
    {
        GET,
        POST
    };

    /**
     * @brief Constructor
     * @param settings The LLM process settings
     * @param parent The parent QObject
     */
    explicit LLMProcessManager(const LLMProcessSettings& settings = LLMProcessSettings(), QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~LLMProcessManager() override;
    
    /**
     * @brief Get the current settings
     * @return The LLM process settings
     */
    const LLMProcessSettings& settings() const;
    
    /**
     * @brief Start the LLM engine process asynchronously
     * @return True if the process launch was initiated successfully
     * @note The actual engine startup result will be communicated via the engineStarted signal
     */
    virtual bool startEngine() = 0;
    
    /**
     * @brief Apply settings to the manager
     * @param settings The settings to apply
     */
    virtual void applySettings(const LLMProcessSettings& settings);
    
    /**
     * @brief Check if the LLM engine is running
     * @return True if the engine is currently running
     */
    virtual bool isEngineRunning() const;
    
    /**
     * @brief Check if the LLM engine is ready to perform inference
     * @return True if the LLM engine is ready for inference operations
     */
    virtual bool engineReady() const        { return m_engineReady; }
    
    /**
     * @brief Check if the LLM engine is ready to perform inference
     * @return True if the LLM engine is ready for inference operations
     */
    virtual bool checkEngineReady() = 0;
    
    /**
     * @brief Submit a request to the LLM API
     * @param function The function name or API endpoint to call
     * @param payload The JSON payload for the request
     * @param method The HTTP method to use (GET or POST)
     * @param force If true, skip the engine ready check
     * @return A QByteArray containing the raw response data from the LLM
     * @note This is a blocking call and will wait for the response from the LLM
     */
    virtual QByteArray submitRequest(const QString& function, const QJsonObject& payload, 
                                    LLMRequestMethod method = POST, bool force = false) = 0;
    
    /**
     * @brief Get the process state
     * @return The current state of the process
     */
    QProcess::ProcessState engineState() const;
    
    /**
     * @brief Get the LLM engine process ID
     * @return The process ID of the running engine or -1 if not running
     */
    qint64 engineProcessId() const;
    
    /**
     * @brief Get the last error message
     * @return The last error message from the process
     */
    QString lastErrorMessage() const;

public Q_SLOTS:

    /**
     * @brief Abort a running inference
     * This method should be overridden in derived classes to stop a currently running inference
     * without shutting down the engine.
     */
    virtual void abortInference() = 0;
    
    /**
     * @brief Shutdown the LLM engine
     * This method gracefully shuts down the LLM engine.
     * Can be overridden in derived classes for specific shutdown behavior.
     */
    virtual void shutdownEngine();

Q_SIGNALS:

    /**
     * @brief Signal emitted when the engine has started
     * @param success True if the engine started successfully
     */
    void engineStarted(bool success);
    
    /**
     * @brief Signal emitted when the engine has stopped
     * @param exitCode The exit code of the process
     */
    void engineStopped(int exitCode);
    
    /**
     * @brief Signal emitted when there is an error from the engine
     * @param error The error text from the engine
     */
    void engineError(const QString& error);
    
    /**
     * @brief Signal emitted when the engine becomes ready for inference
     * @param ready True if the engine is ready, false otherwise
     */
    void engineReadyChanged(bool ready);


protected Q_SLOTS:

    /**
     * @brief Handle process state changes
     * @param newState The new process state
     */
    virtual void handleProcessStateChanged(QProcess::ProcessState newState);
    
    /**
     * @brief Handle process errors
     * @param error The process error
     */
    virtual void handleProcessError(QProcess::ProcessError error);

    /**
     * @brief Handle process started signal
     * @param pid The process ID of the started engine
     */
    virtual void handleProcessStarted();

protected:

    /**
     * @brief Start the LLM engine process asynchronously
     * @return True if the process launch was initiated successfully
     * @note The actual engine startup result will be communicated via the engineStarted signal
     */
    virtual bool startEngine(const QStringList& args);

    /**
     * @brief Process engine output
     * @param data The output data from the engine
     */
    virtual void processEngineOutput(const QByteArray& data);

    /**
     * @brief Process engine error output
     * @param data The error output data from the engine
     */
    virtual void processEngineError(const QByteArray& data);
    
    /**
     * @brief Find an unused TCP port that can be used for services
     * @param startPort The port number to start searching from (default: 5000)
     * @param endPort The maximum port number to search (default: 65535)
     * @return An available TCP port number, or -1 if none found
     */
    int findUnusedTcpPort() const;

    /**
     * @brief The QProcess instance that manages the LLM engine process
     */
    QProcess* m_process;

    /**
     * @brief The path to the LLM engine executable
     */
    QString m_executablePath;

    /**
     * @brief The last error message from the process
     */
    QString m_lastErrorMessage;

    /**
     * @brief The LLM process settings
     */
    LLMProcessSettings m_settings;

    /**
     * @brief The process launcher thread
     */
    QThread* m_launcherThread;

    /**
     * @brief The process launcher timeout
     * This is the maximum time to wait for the process to start
     * and be able to accept requests.
     */
    qint64 m_launcherTimeout        = 50000; // 50 seconds

    /**
     * @brief Timeout for API requests
     */
    qint64 m_apiTimeout             = 300000;  // 5 minutes
    
    /**
     * @brief The working directory for the LLM engine process
     */
    QString m_workingDir;

    /**
     * @brief Flag to indicate if the engine is ready for inference
     */
    bool m_engineReady              = false;

    /**
     * @brief Flag to indicate if the engine is shutting down
     */
    bool m_exiting                  = false;

private:
    // Disable copying
    LLMProcessManager(const LLMProcessManager&)            = delete;
    LLMProcessManager& operator=(const LLMProcessManager&) = delete;
};

} // namespace Digikam
