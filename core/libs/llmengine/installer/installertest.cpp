/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-19
 * Description : Test application for LLMEngineInstaller
 *
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

// This is a standalone test application that shows how to use the LLMEngineInstaller

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QComboBox>
#include <QDir>
#include <QDebug>

#include "llmengineinstaller.h"

using namespace Digikam;

class InstallerTestWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    
    InstallerTestWindow(QWidget* parent = nullptr) 
        : QMainWindow(parent),
          m_installer(nullptr)
    {
        setWindowTitle("Llama.cpp Installer Test");
        setMinimumSize(600, 300);
        
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        
        // Destination path selector
        QHBoxLayout* pathLayout = new QHBoxLayout();
        QLabel* pathLabel = new QLabel("Destination:", this);
        m_pathEdit = new QLineEdit(this);
        m_pathEdit->setText(QDir::homePath() + QDir::separator() + "llamacpp");
        QPushButton* browseButton = new QPushButton("Browse...", this);
        
        pathLayout->addWidget(pathLabel);
        pathLayout->addWidget(m_pathEdit, 1);
        pathLayout->addWidget(browseButton);
        
        // Version input
        QHBoxLayout* versionLayout = new QHBoxLayout();
        QLabel* versionLabel = new QLabel("Version:", this);
        m_versionEdit = new QLineEdit(this);
        m_versionEdit->setText("b5519");
        m_versionEdit->setPlaceholderText("Leave empty for default");
        
        versionLayout->addWidget(versionLabel);
        versionLayout->addWidget(m_versionEdit, 1);
        
        // GPU type selector
        QHBoxLayout* gpuLayout = new QHBoxLayout();
        QLabel* gpuLabel = new QLabel("GPU Type:", this);
        m_gpuCombo = new QComboBox(this);
        m_gpuCombo->addItem("Auto-detect", "auto");
        m_gpuCombo->addItem("CUDA (NVIDIA)", "cuda");
        m_gpuCombo->addItem("Vulkan", "vulkan");
        m_gpuCombo->addItem("ROCm (AMD Linux)", "rocm");
        m_gpuCombo->addItem("Metal (macOS)", "metal");
        m_gpuCombo->addItem("HIP (AMD Windows)", "hip");
        m_gpuCombo->addItem("CPU Only", "none");
        
        gpuLayout->addWidget(gpuLabel);
        gpuLayout->addWidget(m_gpuCombo, 1);
        
        // Install button
        QPushButton* installButton = new QPushButton("Install Llama.cpp", this);
        installButton->setMinimumHeight(40);
        
        // Status label
        m_statusLabel = new QLabel("Ready", this);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setWordWrap(true);
        
        // Add all layouts to main layout
        mainLayout->addLayout(pathLayout);
        mainLayout->addLayout(versionLayout);
        mainLayout->addLayout(gpuLayout);
        mainLayout->addWidget(installButton);
        mainLayout->addWidget(m_statusLabel);
        mainLayout->addStretch();
        
        // Connect signals
        connect(browseButton, &QPushButton::clicked, this, &InstallerTestWindow::browsePath);
        connect(installButton, &QPushButton::clicked, this, &InstallerTestWindow::startInstallation);
    }
    
private slots:
    
    void browsePath()
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Directory",
                                                      m_pathEdit->text());
        if (!dir.isEmpty())
        {
            m_pathEdit->setText(dir);
        }
    }
    
    void startInstallation()
    {
        QString destDir = m_pathEdit->text();
        QString version = m_versionEdit->text();
        QString gpuType = m_gpuCombo->currentData().toString();
        
        if (destDir.isEmpty())
        {
            m_statusLabel->setText("Error: Please specify a destination directory");
            return;
        }
        
        m_statusLabel->setText("Starting installation...");
        
        // Create installer if it doesn't exist
        if (!m_installer)
        {
            m_installer = new LLMEngineInstaller(this);
            connect(m_installer, &LLMEngineInstaller::installationFinished,
                    this, &InstallerTestWindow::installationFinished);
        }
        
        // Start the installation
        if (!m_installer->start(destDir, version, gpuType))
        {
            m_statusLabel->setText("Failed to start installation");
        }
    }
    
    void installationFinished(bool success, const QString& output)
    {
        if (success)
        {
            m_statusLabel->setText("Installation completed successfully");
        }
        else
        {
            m_statusLabel->setText("Installation failed");
        }
        
        // Clean up installer
        m_installer->deleteLater();
        m_installer = nullptr;
    }
    
private:
    QLineEdit* m_pathEdit;
    QLineEdit* m_versionEdit;
    QComboBox* m_gpuCombo;
    QLabel* m_statusLabel;
    LLMEngineInstaller* m_installer;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    
    InstallerTestWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}

#include "installertest.moc"
