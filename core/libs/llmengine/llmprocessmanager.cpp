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

// System includes
#if defined(Q_OS_MAC) || defined(Q_OS_UNIX)
#include <sys/resource.h>  // For setpriority
#endif

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
    if (m_settings.external)
    {
        // External engine, no need to start a process
        return;
    }

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
            this, SLOT(slotProcessStateChanged(QProcess::ProcessState)));
    
    connect(m_process, SIGNAL(errorOccurred(QProcess::ProcessError)),
            this, SLOT(slotProcessError(QProcess::ProcessError)));

    connect(m_process, SIGNAL(started()),
            this, SLOT(slotProcessStarted()));

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, [this]()
            {
                slotProcessEngineOutput(m_process->readAllStandardOutput());
            });
            
    connect(m_process, &QProcess::readyReadStandardError,
            this, [this]()
            {
                slotProcessEngineError(m_process->readAllStandardError());
            });

    connect(m_process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus)
            {
                m_exiting = true;
                Q_EMIT signalEngineStopped(exitCode);
            });
}

LLMProcessManager::~LLMProcessManager()
{
    if (isEngineRunning())
    {
        shutdownEngine();
    }

    if (m_process)
    {
        m_process->disconnect();
        delete m_process;
        m_process = nullptr;
    }
    
}

bool LLMProcessManager::startEngine(const QStringList& args)
{
    if (m_settings.external)
    {
        // External engine, no need to start a process
        QTimer::singleShot(100, this, &LLMProcessManager::slotProcessStarted);
        return true;
    }

    if (isEngineRunning())
    {
        m_lastErrorMessage = QLatin1String("Engine is already running");
        qCWarning(DIGIKAM_LLMENGINE_LOG) << "Cannot start engine. " << m_lastErrorMessage;
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

    startProcessWithPriority(m_process, m_executablePath, args);
    // m_process->start(m_executablePath, args);

    return true; // Return true if process start was initiated (not waiting for actual startup)
}

void LLMProcessManager::startProcessWithPriority(QProcess* process, const QString& program, const QStringList& args)
{
    // Start process with idle priority to minimize impact on system performance
#ifdef Q_OS_WIN
    // On Windows, we use the CREATE_PROCESS_BELOW_NORMAL_PRIORITY_CLASS flag
    QProcess::CreateProcessArgumentModifier modifier = [](QProcess::CreateProcessArguments* args) {
        // Set the process priority to below normal
        args->flags |= 0x00004000; // CREATE_BELOW_NORMAL_PRIORITY_CLASS
    };
    
    process->setCreateProcessArgumentsModifier(modifier);
    process->start(program, args);
#elif defined(Q_OS_MAC)
    // On macOS, we start the process and then use setpriority() to set the priority
    process->start(program, args);
    
    // Give the process a moment to start up
    if (process->waitForStarted(500)) {
        qint64 pid = process->processId();
        if (pid > 0) {
            // Use setpriority to set the process priority to idle (lowest)
            // PRIO_PROCESS = 0, 20 = lowest priority
            if (setpriority(0, pid, 20) != 0) {
                qCWarning(DIGIKAM_LLMENGINE_LOG) << "Failed to set process priority for PID" << pid;
            }
            else {
                qCDebug(DIGIKAM_LLMENGINE_LOG) << "Successfully set low priority for process" << pid;
            }
        }
    }
#else
    // On Linux and other Unix variants, we use nice to start with lower priority
    QStringList nicedArgs;
    nicedArgs << QLatin1String("nice");   // Run with modified scheduling priority
    nicedArgs << QLatin1String("-n");     // Specify the nice increment
    nicedArgs << QLatin1String("19");     // Highest nice value (lowest priority)
    nicedArgs << program;                 // The actual program to run
    nicedArgs << args;                    // Add the original arguments
    
    process->start(QLatin1String("/usr/bin/env"), nicedArgs);
#endif
}

void LLMProcessManager::slotProcessStarted()
{
    qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLM Engine process started";

    // begin checking for readiness

    if ((m_process && (QProcess::Running == m_process->state())) ||
        m_settings.external)
    {
        // check if the engine is ready for inference by sending a test request
        
        if (checkEngineReady())
        {
            m_engineReady = true;
            Q_EMIT signalEngineReady();
            qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLM Engine is ready for inference";
        }
        else
        {
            // try again in 2 seconds

            QTimer::singleShot(2000, this, &LLMProcessManager::slotProcessStarted);
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
    return ((m_process && m_process->state() != QProcess::NotRunning) || 
            m_settings.external);
}

QProcess::ProcessState LLMProcessManager::engineState() const
{
    return m_process ? m_process->state() : QProcess::NotRunning;
}

qint64 LLMProcessManager::engineProcessId() const
{
    return m_process ? m_process->processId() : -1;
}

const QString LLMProcessManager::getBaseEnginePath()
{
    // Get the base path for the LLM engine
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    basePath        += QDir::separator() + QLatin1String("digikam") + QDir::separator() + QLatin1String("llmengine");
    
    return basePath;
}

QString LLMProcessManager::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

const LLMProcessSettings& LLMProcessManager::settings() const
{
    return m_settings;
}

void LLMProcessManager::slotProcessEngineOutput(const QByteArray& data)
{
    if (!data.isEmpty())
    {
        QString output = QString::fromUtf8(data);
        qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLM Engine (1):" << output;
    }
}

void LLMProcessManager::slotProcessEngineError(const QByteArray& data)
{
    if (!data.isEmpty())
    {
        QString output = QString::fromUtf8(data);
        qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLM Engine (2):" << output;
        m_lastErrorMessage = output;
        Q_EMIT signalEngineError(output);
    }
}

void LLMProcessManager::slotProcessStateChanged(QProcess::ProcessState newState)
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

void LLMProcessManager::slotProcessError(QProcess::ProcessError error)
{
    QString errorStr;
    
    switch (error)
    {
        case QProcess::FailedToStart:
        {
            errorStr = QLatin1String("Failed to start");
            m_exiting = true;
            break;
        }
            
        case QProcess::Crashed:
        {
            if (m_exiting)
            {
                errorStr = QLatin1String("Exiting normally");
            }
            else
            {
                errorStr = QLatin1String("Crashed");
                qCDebug(DIGIKAM_LLMENGINE_LOG) << "Emitting engineCrashed signal";
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
    Q_EMIT signalEngineError(m_lastErrorMessage);
}

void LLMProcessManager::shutdownEngine()
{
    qCDebug(DIGIKAM_LLMENGINE_LOG) << "LLMProcessManager::shutdownEngine: Shutting down LLM engine";

    m_exiting = true;

    if (m_settings.external)
    {
        // External engine, no need to shut down
        // fake the shutdown signal
        Q_EMIT signalEngineStopped(0);
        return;
    }

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
