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

#pragma once

// Qt includes

#include <QObject>

// KDE includes

#include <QAction>

// Local includes

#include "digikam_export.h"

class QWidget;

namespace Digikam
{

/**
 * Class to create action for installing llama.cpp
 * This action can be added to menu/toolbar in main application
 */
class DIGIKAM_EXPORT LLMEngineInstallAction : public QObject
{
    Q_OBJECT

public:

    /**
     * Constructor with optional parent widget
     */
    explicit LLMEngineInstallAction(QObject* const parent);

    /**
     * Destructor
     */
    ~LLMEngineInstallAction() override;

    /**
     * Returns the action to add to menu/toolbar
     */
    QAction* action() const;

private Q_SLOTS:

    /**
     * Slot to handle action trigger
     */
    void slotInstallLlamaCpp();

private:

    QAction* m_action;
};

} // namespace Digikam
