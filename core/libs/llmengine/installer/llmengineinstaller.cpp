/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-19
 * Description : Class to manage LLM engine installation across platforms
 *
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

// Local includes

#include "llmengineinstaller.h"

// Qt includes

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_globals.h"
#include "digikam_debug.h"

namespace Digikam
{

LLMEngineInstaller::LLMEngineInstaller(QObject* const parent)
    : QObject(parent),
      m_process(nullptr)
{
}

LLMEngineInstaller::~LLMEngineInstaller()
{
    if (m_process)
    {
        if (m_process->state() != QProcess::NotRunning)
        {
            m_process->terminate();
            m_process->waitForFinished(3000); // Wait up to 3 seconds
            
            if (m_process->state() != QProcess::NotRunning)
            {
                m_process->kill();
            }
        }
        
        delete m_process;
    }
}

bool LLMEngineInstaller::start(const QString& destinationPath, 
                              const QString& version,
                              const QString& gpuType)
{
    if (destinationPath.isEmpty())
    {
        m_errorOutput = i18n("Error: Destination path is empty");
        displayOutputDialog(false);
        return false;
    }
    
    QString installerScript = getInstallerScript();
    
    if (installerScript.isEmpty())
    {
        m_errorOutput = i18n("Error: Could not determine the appropriate installer script for this platform");
        displayOutputDialog(false);
        return false;
    }
    
    // Create the destination directory if it doesn't exist
    QDir destDir(destinationPath);
    
    if (!destDir.exists())
    {
        if (!destDir.mkpath(destinationPath))
        {
            m_errorOutput = i18n("Error: Failed to create destination directory: %1").arg(destinationPath);
            displayOutputDialog(false);
            return false;
        }
    }
    
    // Clear previous output
    m_output.clear();
    m_errorOutput.clear();
    
    // Create process if needed
    if (!m_process)
    {
        m_process = new QProcess(this);
        
        connect(m_process, &QProcess::readyReadStandardOutput,
                this, &LLMEngineInstaller::slotReadStandardOutput);
                
        connect(m_process, &QProcess::readyReadStandardError,
                this, &LLMEngineInstaller::slotReadStandardError);
                
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &LLMEngineInstaller::slotProcessFinished);
    }
    
    // Prepare arguments
    QStringList arguments;
    arguments << destinationPath;
    
    if (!version.isEmpty())
    {
        arguments << version;
        
        if (!gpuType.isEmpty())
        {
            arguments << gpuType;
        }
    }
    else if (!gpuType.isEmpty())
    {
        arguments << QLatin1String("b5432"); // Default version
        arguments << gpuType;
    }

#ifdef Q_OS_WIN
    // For Windows, we need to use cmd.exe to run the batch file
    m_process->setProgram(QLatin1String("cmd.exe"));
    QStringList cmdArgs;
    cmdArgs << QLatin1String("/c") << installerScript;
    cmdArgs.append(arguments);
    m_process->setArguments(cmdArgs);
#else
    // For Unix-like systems, run the script directly
    // Make sure the script is executable
    QFile script(installerScript);
    if (script.exists())
    {
        QProcess chmodProcess;
        chmodProcess.start(QLatin1String("chmod"), QStringList() << QLatin1String("+x") << installerScript);
        chmodProcess.waitForFinished();
    }
    
    m_process->setProgram(installerScript);
    m_process->setArguments(arguments);
#endif
    
    // Start the process
    m_process->start();
    
    if (!m_process->waitForStarted())
    {
        m_errorOutput = i18n("Error: Failed to start the installation process.");
        displayOutputDialog(false);
        return false;
    }
    
    return true;
}

void LLMEngineInstaller::slotProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool success = (exitCode == 0) && (exitStatus == QProcess::NormalExit);
    
    if (!success)
    {
        if (m_errorOutput.isEmpty())
        {
            m_errorOutput = i18n("Installation process failed with exit code %1").arg(exitCode);
        }
    }
    
    displayOutputDialog(success);
    
    Q_EMIT installationFinished(success, m_output);
}

void LLMEngineInstaller::slotReadStandardOutput()
{
    if (m_process)
    {
        QByteArray data = m_process->readAllStandardOutput();
        m_output.append(QString::fromLocal8Bit(data));
    }
}

void LLMEngineInstaller::slotReadStandardError()
{
    if (m_process)
    {
        QByteArray data = m_process->readAllStandardError();
        m_errorOutput.append(QString::fromLocal8Bit(data));
        m_output.append(QString::fromLocal8Bit(data)); // Also add to regular output for display
    }
}

QString LLMEngineInstaller::getInstallerScript() const
{
#ifdef Q_OS_MAC
    QString appDirPath = macOSBundlePrefix();
#else
    QString appDirPath = QCoreApplication::applicationDirPath();
#endif
    QString installerDir = appDirPath + QDir::separator() + QLatin1String("installer");
    
#ifdef Q_OS_WIN
    QString scriptName = QLatin1String("llamacpp_win.bat");
#elif defined(Q_OS_MAC)
    QString scriptName = QLatin1String("llamacpp_macos.sh");
#else
    QString scriptName = QLatin1String("llamacpp_linux.sh");
#endif
    
    QStringList searchPaths;
    
    searchPaths << (appDirPath + QDir::separator() + QLatin1String("lib") + QDir::separator() + 
                    QLatin1String("llmengine") + QDir::separator() + QLatin1String("scripts") + QDir::separator() + scriptName);
    
    
    // Loop through search paths and use the first script found
    for (const QString& path : searchPaths)
    {
        if (QFile::exists(path))
        {
            qDebug() << "Found installer script at:" << path;
            return path;
        }
    }
    
    // If we can't find the script, try to create a temporary one
    qWarning() << "Could not find installer script:" << scriptName;
    
    // As a last resort, copy our embedded script to a temporary location and use that
    QTemporaryDir tempDir;
    if (tempDir.isValid())
    {
        QString tempScriptPath = tempDir.path() + QDir::separator() + scriptName;
        QFile tempScript(tempScriptPath);
        
        // Create a script with the basic commands needed
        if (tempScript.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream stream(&tempScript);
            
#ifdef Q_OS_WIN
            stream << "@echo off\n";
            stream << "echo Error: Could not locate the llamacpp_win.bat script\n";
            stream << "echo Please ensure the script is properly installed\n";
            stream << "exit /b 1\n";
#else
            stream << "#!/bin/bash\n";
            stream << "echo \"Error: Could not locate the installer script\"\n";
            stream << "echo \"Please ensure the script is properly installed\"\n";
            stream << "exit 1\n";
#endif
            
            tempScript.close();
            
#ifndef Q_OS_WIN
            // Make the script executable on Unix
            QProcess chmodProcess;
            chmodProcess.start(QLatin1String("chmod"), QStringList() << QLatin1String("+x") << tempScriptPath);
            chmodProcess.waitForFinished();
#endif
            
            // Make the temporary directory persistent
            tempDir.setAutoRemove(false);
            
            qDebug() << "Created fallback script at:" << tempScriptPath;
            return tempScriptPath;
        }
    }
    
    return QString();
}

void LLMEngineInstaller::displayOutputDialog(bool success)
{
    QMessageBox msgBox;
    
    if (success)
    {
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setWindowTitle(tr("Installation Successful"));
        msgBox.setText(tr("The llama.cpp binaries were successfully installed."));
    }
    else
    {
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("Installation Failed"));
        msgBox.setText(tr("Failed to install the llama.cpp binaries."));
    }
    
    // Prepare the combined output text
    QString outputText = m_output;
    
    if (outputText.isEmpty())
    {
        outputText = i18n("No output received from the installation process.");
    }
    
    msgBox.setInformativeText(tr("Installation Log:"));
    
    // Set the detailed text to show the full output
    msgBox.setDetailedText(outputText);
    
    // Ensure the dialog is large enough to show a good amount of the log
    msgBox.setMinimumWidth(600);
    
    // Show the dialog
    msgBox.exec();
}

} // namespace Digikam
