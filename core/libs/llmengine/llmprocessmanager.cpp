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

// Qt includes

#include <QDebug>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QAbstractSocket>

// Local includes

#include "digikam_debug.h"
#include "llmprocessmanager.h"

namespace Digikam
{

LLMProcessManager::LLMProcessManager(const LLMProcessSettings& settings, QObject* parent)
    : QObject(parent),
      m_process(nullptr),
      m_lastErrorMessage(QString()),
      m_settings(settings)
{
    // get the working directory for the process

    QString m_workingDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    m_workingDir        += QLatin1String("/digikam/llmengine");

    if (!QFileInfo::exists(m_workingDir))
    {
        QDir().mkpath(m_workingDir);
    }

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_workingDir);
    
    connect(m_process, SIGNAL(stateChanged(QProcess::ProcessState)),
            this, SLOT(handleProcessStateChanged(QProcess::ProcessState)));
    
    connect(m_process, SIGNAL(errorOccurred(QProcess::ProcessError)),
            this, SLOT(handleProcessError(QProcess::ProcessError)));

    connect(m_process, SIGNAL(started()),
            this, SLOT(handleProcessStarted()));

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, [this]()
            {
                processEngineOutput(m_process->readAllStandardOutput());
            });
            
    connect(m_process, &QProcess::readyReadStandardError,
            this, [this]()
            {
                processEngineError(m_process->readAllStandardError());
            });

    connect(m_process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus /*exitStatus*/)
            {
                Q_EMIT engineStopped(exitCode);
            });
}

LLMProcessManager::~LLMProcessManager()
{
    if (isEngineRunning())
    {
        shutdownEngine();
    }
    
    delete m_process;
    m_process = nullptr;
}

bool LLMProcessManager::startEngine(const QStringList& args)
{
    if (isEngineRunning())
    {
        m_lastErrorMessage = QLatin1String("Engine is already running");
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Cannot start engine. " << m_lastErrorMessage;
        Q_EMIT engineStarted(false);
        return false;
    }
    
    QFileInfo engineFile(m_executablePath);
    if (!engineFile.exists() || !engineFile.isExecutable())
    {
        m_lastErrorMessage = QString::fromLatin1("Engine binary does not exist or is not executable: %1").arg(m_executablePath);
        qCWarning(DIGIKAM_LLMENGINE_LOG) << m_lastErrorMessage;
        return false;
    }

    qCDebug(DIGIKAM_LLMENGINE_LOG) << "Starting LLM engine:" << m_executablePath << "with args:" << args;

    // Start the process

    m_process->start(m_executablePath, args);

    return true; // Return true if process start was initiated (not waiting for actual startup)
}

void LLMProcessManager::handleProcessStarted()
{
    qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLM Engine process started";

    // begin checking for readiness

    if (m_process)
    {
        if (checkEngineReady())
        {
            m_engineReady = true;
            Q_EMIT engineReadyChanged(m_engineReady);
            qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLM Engine is ready for inference";
        }
        else
        {
            // try again in 2 seconds

            QTimer::singleShot(2000, this, &LLMProcessManager::handleProcessStarted);
        }
    }
}

void LLMProcessManager::applySettings(const LLMProcessSettings& settings)
{
    // Store the settings in our member variable
    m_settings = settings;
    
    // Derived classes should override this to apply settings to their specific implementation
}

bool LLMProcessManager::isEngineRunning() const
{
    return (m_process && m_process->state() != QProcess::NotRunning);
}

QProcess::ProcessState LLMProcessManager::engineState() const
{
    return m_process ? m_process->state() : QProcess::NotRunning;
}

qint64 LLMProcessManager::engineProcessId() const
{
    return m_process ? m_process->processId() : -1;
}

QString LLMProcessManager::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

const LLMProcessSettings& LLMProcessManager::settings() const
{
    return m_settings;
}

void LLMProcessManager::processEngineOutput(const QByteArray& data)
{
    if (!data.isEmpty())
    {
        QString output = QString::fromUtf8(data);
        qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLM Engine output:" << output;
    }
}

void LLMProcessManager::processEngineError(const QByteArray& data)
{
    if (!data.isEmpty())
    {
        QString error = QString::fromUtf8(data);
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "LLM Engine error:" << error;
        m_lastErrorMessage = error;
        Q_EMIT engineError(error);
    }
}

void LLMProcessManager::handleProcessStateChanged(QProcess::ProcessState newState)
{
    QString stateStr;
    
    switch (newState)
    {
        case QProcess::NotRunning:
        {
            stateStr = QLatin1String("Not Running");
            break;
        }
            
        case QProcess::Starting:
        {
            stateStr = QLatin1String("Starting");
            break;
        }
            
        case QProcess::Running:
        {
            stateStr = QLatin1String("Running");
            break;
        }
    }
    
    qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLM Engine process state changed to:" << stateStr;
}

void LLMProcessManager::handleProcessError(QProcess::ProcessError error)
{
    QString errorStr;
    
    switch (error)
    {
        case QProcess::FailedToStart:
        {
            errorStr = QLatin1String("Failed to start");
            break;
        }
            
        case QProcess::Crashed:
        {
            if (m_exiting)
            {
                errorStr = QLatin1String("Exiting");
            }
            else
            {
                errorStr = QLatin1String("Crashed");
            }
            break;
        }
            
        case QProcess::Timedout:
        {
            errorStr = QLatin1String("Timed out");
            break;
        }
            
        case QProcess::ReadError:
        {
            errorStr = QLatin1String("Read error");
            break;
        }
            
        case QProcess::WriteError:
        {
            errorStr = QLatin1String("Write error");
            break;
        }
            
        case QProcess::UnknownError:
        {
            errorStr = QLatin1String("Unknown error");
            break;
        }
    }
    
    m_lastErrorMessage = QString::fromLatin1("Process error: %1 - %2").arg(errorStr).arg(m_process->errorString());
    qCWarning(DIGIKAM_LLMENGINE_LOG) << m_lastErrorMessage;
    Q_EMIT engineError(m_lastErrorMessage);
}

void LLMProcessManager::shutdownEngine()
{
    qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLMProcessManager::shutdownEngine: Shutting down LLM engine";

    m_exiting = true;

    if (m_process && (m_process->state() != QProcess::NotRunning))
    {
        // Use QProcess::terminate() which sends SIGTERM on Unix/Linux/Mac
        // and gracefully terminates the process on Windows
        m_process->terminate();
        
        // Give it a few seconds to finish
        if (!m_process->waitForFinished(3000))
        {
            qCWarning(DIGIKAM_LLMENGINE_LOG) << "LLMProcessManager::shutdownEngine: Process did not terminate within timeout, forcing kill";
            m_process->kill();
        }
    }
}

int LLMProcessManager::findUnusedTcpPort() const
{
    int freePort = -1;

    QTcpServer server;
    if (server.listen(QHostAddress::Any, 0))
    { // Port 0 lets the OS choose an available port
        freePort = server.serverPort();
        qCDebug(DIGIKAM_LLMENGINE_LOG) << "Unused TCP port found:" << freePort;
        server.close(); // Close the server after retrieving the port
    }

    return freePort;
}

} // namespace Digikam

#include "moc_llmprocessmanager.cpp"
