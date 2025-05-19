/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-19
 * Description : Dialog for installing LLM engine
 *
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

// Local includes

#include "llmengineinstalldialog.h"

// Qt includes

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QDialogButtonBox>

// KDE includes

#include <klocalizedstring.h>

namespace Digikam
{

LLMEngineInstallDialog::LLMEngineInstallDialog(QWidget* const parent)
    : QDialog(parent),
      m_installer(nullptr)
{
    setWindowTitle(i18n("Install LLM Engine"));
    setMinimumSize(500, 300);
    
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Destination path selector
    QHBoxLayout* pathLayout = new QHBoxLayout();
    QLabel* pathLabel = new QLabel(i18n("Destination:"), this);
    m_pathEdit = new QLineEdit(this);
    
    // Set default path in user's application data directory
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    defaultPath += QDir::separator() + QStringLiteral("llamacpp");
    m_pathEdit->setText(defaultPath);
    
    QPushButton* browseButton = new QPushButton(i18n("Browse..."), this);
    
    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_pathEdit, 1);
    pathLayout->addWidget(browseButton);
    
    // Version input
    QHBoxLayout* versionLayout = new QHBoxLayout();
    QLabel* versionLabel = new QLabel(i18n("Version:"), this);
    m_versionEdit = new QLineEdit(this);
    m_versionEdit->setText(QStringLiteral("b5425"));
    m_versionEdit->setToolTip(i18n("Specify a version tag from the llama.cpp GitHub releases. Default is b5425."));
    
    versionLayout->addWidget(versionLabel);
    versionLayout->addWidget(m_versionEdit, 1);
    
    // GPU type selector
    QHBoxLayout* gpuLayout = new QHBoxLayout();
    QLabel* gpuLabel = new QLabel(i18n("GPU Type:"), this);
    m_gpuCombo = new QComboBox(this);
    m_gpuCombo->addItem(i18n("Auto-detect"), QStringLiteral("auto"));
    
    // Add platform-specific GPU options
#ifdef Q_OS_WIN
    m_gpuCombo->addItem(i18n("CUDA (NVIDIA)"), QStringLiteral("cuda"));
    m_gpuCombo->addItem(i18n("HIP (AMD)"), QStringLiteral("hip"));
    m_gpuCombo->addItem(i18n("Vulkan"), QStringLiteral("vulkan"));
    m_gpuCombo->addItem(i18n("CPU Only"), QStringLiteral("none"));
#elif defined(Q_OS_MAC)
    m_gpuCombo->addItem(i18n("Metal"), QStringLiteral("metal"));
    m_gpuCombo->addItem(i18n("Vulkan"), QStringLiteral("vulkan"));
    m_gpuCombo->addItem(i18n("CPU Only"), QStringLiteral("none"));
#else
    m_gpuCombo->addItem(i18n("CUDA (NVIDIA)"), QStringLiteral("cuda"));
    m_gpuCombo->addItem(i18n("ROCm (AMD)"), QStringLiteral("rocm"));
    m_gpuCombo->addItem(i18n("Vulkan"), QStringLiteral("vulkan"));
    m_gpuCombo->addItem(i18n("CPU Only"), QStringLiteral("none"));
#endif
    
    gpuLayout->addWidget(gpuLabel);
    gpuLayout->addWidget(m_gpuCombo, 1);
    
    // Status label
    m_statusLabel = new QLabel(i18n("Ready to install llama.cpp binaries"), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    
    // Dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_installButton = new QPushButton(i18n("Install"), this);
    m_installButton->setDefault(true);
    buttonBox->addButton(m_installButton, QDialogButtonBox::ActionRole);
    
    // Add all widgets to main layout
    mainLayout->addLayout(pathLayout);
    mainLayout->addLayout(versionLayout);
    mainLayout->addLayout(gpuLayout);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonBox);
    
    // Connect signals
    connect(browseButton, &QPushButton::clicked, this, &LLMEngineInstallDialog::slotBrowsePath);
    connect(m_installButton, &QPushButton::clicked, this, &LLMEngineInstallDialog::slotInstall);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

LLMEngineInstallDialog::~LLMEngineInstallDialog()
{
    // The installer will be deleted by the parent-child relationship with QObject
}

void LLMEngineInstallDialog::slotBrowsePath()
{
    QString dir = QFileDialog::getExistingDirectory(this, 
                                                  i18n("Select Destination Directory"),
                                                  m_pathEdit->text());
                                                  
    if (!dir.isEmpty())
    {
        m_pathEdit->setText(dir);
    }
}

void LLMEngineInstallDialog::slotInstall()
{
    QString destDir  = m_pathEdit->text();
    QString version  = m_versionEdit->text();
    QString gpuType  = m_gpuCombo->currentData().toString();
    
    if (destDir.isEmpty())
    {
        m_statusLabel->setText(i18n("Error: Please specify a destination directory"));
        return;
    }
    
    // Create installer if it doesn't exist
    if (!m_installer)
    {
        m_installer = new LLMEngineInstaller(this);
        connect(m_installer, &LLMEngineInstaller::installationFinished,
                this, &LLMEngineInstallDialog::slotInstallationFinished);
    }
    
    // Disable UI elements during installation
    m_installButton->setEnabled(false);
    m_pathEdit->setEnabled(false);
    m_versionEdit->setEnabled(false);
    m_gpuCombo->setEnabled(false);
    
    m_statusLabel->setText(i18n("Installing llama.cpp binaries, please wait..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // Start the installation
    m_installer->start(destDir, version, gpuType);
}

void LLMEngineInstallDialog::slotInstallationFinished(bool success, const QString& output)
{
    Q_UNUSED(output); // The output is already displayed in a dialog by the installer
    
    QApplication::restoreOverrideCursor();
    
    // Re-enable UI elements
    m_installButton->setEnabled(true);
    m_pathEdit->setEnabled(true);
    m_versionEdit->setEnabled(true);
    m_gpuCombo->setEnabled(true);
    
    if (success)
    {
        m_statusLabel->setText(i18n("Installation completed successfully"));
    }
    else
    {
        m_statusLabel->setText(i18n("Installation failed. See log for details."));
    }
}

} // namespace Digikam
