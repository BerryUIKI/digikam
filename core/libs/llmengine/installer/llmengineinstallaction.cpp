/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-19
 * Description : Action to launch the llama.cpp installer
 *
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

// Local includes

#include "llmengineinstallaction.h"
#include "llmengineinstalldialog.h"

// KDE includes

#include <klocalizedstring.h>

// Qt includes

#include <QApplication>
#include <QMainWindow>

namespace Digikam
{

LLMEngineInstallAction::LLMEngineInstallAction(QObject* const parent)
    : QObject(parent),
      m_action(new QAction(this))
{
    m_action->setText(i18n("Install llama.cpp Binaries..."));
    m_action->setToolTip(i18n("Download and install llama.cpp binaries for local AI inference"));
    m_action->setWhatsThis(i18n("Launch the llama.cpp installer to download the appropriate binaries for your system"));
    
    // We can set an icon for the action
    // m_action->setIcon(QIcon::fromTheme("download"));
    
    connect(m_action, &QAction::triggered, this, &LLMEngineInstallAction::slotInstallLlamaCpp);
}

LLMEngineInstallAction::~LLMEngineInstallAction()
{
    // QAction will be deleted by parent-child relationship with QObject
}

QAction* LLMEngineInstallAction::action() const
{
    return m_action;
}

void LLMEngineInstallAction::slotInstallLlamaCpp()
{
    // Find parent widget (can be null)
    QWidget* parentWidget = qobject_cast<QWidget*>(parent());
    
    if (!parentWidget)
    {
        // If no parent widget is available, try to use the active window
        parentWidget = QApplication::activeWindow();
    }
    
    // Create and show the dialog
    LLMEngineInstallDialog* const dialog = new LLMEngineInstallDialog(parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

} // namespace Digikam
