/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchpromptbuilder.h"

namespace Digikam
{

SearchPromptBuilder::SearchPromptBuilder()
{
    initSystemPrompt();
}

void SearchPromptBuilder::initSystemPrompt()
{
    m_systemPrompt = QLatin1String(
        "You translate photo search requests into JSON.\n"
        "Respond with ONLY a single JSON object. No prose, no markdown.\n"
        "If a term is subjective or cannot be mapped to a supported field, "
        "do NOT guess: either omit it or use the clarification object.\n"
        "Never invent tags, places, or dates that are not implied by the request.\n");
}

QString SearchPromptBuilder::schemaDescription() const
{
    return QLatin1String(
        "JSON schema:\n"
        "{\n"
        "  \"constraints\": [ { \"field\": F, \"op\": O, \"value\": V } ],\n"
        "  \"clarification\": null | { \"message\": M, \"choices\": [C1, C2] }\n"
        "}\n"
        "Supported fields F: tag, album, person, place, daterange, rating, "
        "picklabel, colorlabel, orientation, caption\n"
        "Supported ops O: eq, contains, gte, lte, between\n"
        "Date values use ISO format: YYYY-MM-DD or YYYY-MM-DD..YYYY-MM-DD\n");
}

QString SearchPromptBuilder::buildPrompt(const QString& userQuery,
                                         const QStringList& knownTags,
                                         const QStringList& knownAlbums,
                                         const QStringList& knownPeople) const
{
    QString prompt = m_systemPrompt;
    prompt        += QLatin1Char('\n');
    prompt        += schemaDescription();

    auto appendHints = [&prompt](const QString& label, const QStringList& items)
    {
        if (items.isEmpty())
        {
            return;
        }

        const QStringList trimmed = items.mid(0, s_maxHintItems);
        prompt += QString::fromLatin1("\n%1: %2\n").arg(label, trimmed.join(QLatin1String(", ")));
    };

    appendHints(QLatin1String("Known tags in this collection"),   knownTags);
    appendHints(QLatin1String("Known albums in this collection"), knownAlbums);
    appendHints(QLatin1String("Known people in this collection"), knownPeople);

    prompt += QLatin1String("\nUser request: ");
    prompt += userQuery;
    prompt += QLatin1String("\nJSON:");

    return prompt;
}

QString SearchPromptBuilder::systemPrompt() const
{
    return m_systemPrompt;
}

} // namespace Digikam
