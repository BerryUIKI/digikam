/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-19
 * Description : Class to manage llama.cpp installation across platforms
 *
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
#include <QMessageBox>

// Local includes

#include "digikam_export.h"

namespace Digikam
{

class DIGIKAM_EXPORT LLMEngineInstaller : public QObject
{
    Q_OBJECT

public:

    /**
     * Constructor
     */
    explicit LLMEngineInstaller(QObject* const parent = nullptr);
    
    /**
     * Destructor
     */
    ~LLMEngineInstaller() override;

    /**
     * Start the installation process.
     * @param destinationPath The path where llama.cpp binaries will be installed
     * @param version Optional version string (e.g., 'b5519')
     * @param gpuType Optional GPU type ('cuda', 'vulkan', 'rocm', 'metal', 'hip', 'none', or 'auto')
     * @return True if installation was started successfully, false otherwise
     */
    bool start(const QString& destinationPath, 
               const QString& version = QString(),
               const QString& gpuType = QString());

Q_SIGNALS:

    /**
     * Signal emitted when installation completes
     * @param success True if installation was successful
     * @param output The output from the installation process
     */
    void installationFinished(bool success, const QString& output);

private Q_SLOTS:

    void slotProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotReadStandardOutput();
    void slotReadStandardError();

private:

    /**
     * Get the appropriate script path based on the current platform
     */
    QString getInstallerScript() const;

    /**
     * Display a dialog with the installation output
     */
    void displayOutputDialog(bool success);

private:

    QProcess*      m_process;
    QString        m_output;
    QString        m_errorOutput;
};

} // namespace Digikam
