/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-19
 * Description : Example of using the LLMEngineInstaller class
 *
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

// This is an example of how to use the LLMEngineInstaller class in your application

#include "llmengineinstaller.h"
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

// Sample usage in a class method:
void MyApplication::installLlamaCpp()
{
    // Create the installer
    Digikam::LLMEngineInstaller* installer = new Digikam::LLMEngineInstaller(this);
    
    // Connect to the installation finished signal
    connect(installer, &Digikam::LLMEngineInstaller::installationFinished,
            this, &MyApplication::slotInstallationFinished);
    
    // Set the destination directory (e.g., in app data location)
    QString destDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    destDir += QDir::separator() + "llamacpp";
    
    // Start the installation
    // You can specify version and GPU type or leave them empty for defaults
    // e.g., installer->start(destDir, "b5432", "cuda");
    if (!installer->start(destDir))
    {
        qWarning() << "Failed to start the llama.cpp installation";
        // Note: The installer will show an error dialog
        installer->deleteLater();
    }
}

// Slot to handle installation completion
void MyApplication::slotInstallationFinished(bool success, const QString& output)
{
    if (success)
    {
        qDebug() << "llama.cpp installation succeeded";
        // Update your application's settings or status
        // e.g., m_settings->setValue("llamacpp/installed", true);
    }
    else
    {
        qDebug() << "llama.cpp installation failed:" << output;
        // Handle failure in your application
    }
    
    // Clean up the installer
    sender()->deleteLater();
}
