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

#pragma once

// Qt includes

#include <QDialog>

// Local includes

#include "digikam_export.h"
#include "llmengineinstaller.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

namespace Digikam
{

class DIGIKAM_EXPORT LLMEngineInstallDialog : public QDialog
{
    Q_OBJECT

public:
    
    /**
     * Constructor
     */
    explicit LLMEngineInstallDialog(QWidget* const parent);
    
    /**
     * Destructor
     */
    ~LLMEngineInstallDialog() override;

private Q_SLOTS:
    
    void slotBrowsePath();
    void slotInstall();
    void slotInstallationFinished(bool success, const QString& output);

private:
    
    QLineEdit*           m_pathEdit;
    QLineEdit*           m_versionEdit;
    QComboBox*           m_gpuCombo;
    QLabel*              m_statusLabel;
    QPushButton*         m_installButton;
    LLMEngineInstaller*  m_installer;
};

} // namespace Digikam
