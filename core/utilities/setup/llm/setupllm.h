/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-15
 * Description : AI configuration setup tab
 *
 * SPDX-FileCopyrightText: 2025 by Michael Miller <michael underscore miller at msn dot com>
 * SPDX-FileCopyrightText: 2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QScrollArea>

namespace Digikam
{

class SetupLLM : public QScrollArea
{
    Q_OBJECT

public:

    enum LLMTab
    {
        Annotate = 0,
        // Future tabs will be added here
    };

public:

    explicit SetupLLM(QWidget* const parent = nullptr);
    ~SetupLLM() override;

    void setActiveTab(LLMTab tab);
    LLMTab activeTab() const;

    bool checkSettings();
    void applySettings();

private:

    void readSettings();

private Q_SLOTS:
    void slotEnableAIAnnotateSettings(bool enable);
    void slotEnableAIAnnotateMaxTags(bool enable);
    void slotEnableAIAnnotateAdvancedSettings(bool enable);
    void slotEnableAIAnnotateCustomPrompt(bool enable);

private:

    class Private;
    Private* const d = nullptr;
};

} // namespace Digikam
