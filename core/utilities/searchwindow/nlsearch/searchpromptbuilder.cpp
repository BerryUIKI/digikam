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

// Qt includes
#include <QDate>

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
        "Never invent tags, places, or dates that are not implied by the request.\n"
        "Never map a word to a field it does not belong to: a word like "
        "\"vacation\" is not a person and \"nice\" is not a caption. "
        "Returning fewer constraints is always better than returning wrong ones. "
        "If nothing maps cleanly, return {\"constraints\":[]} with a "
        "clarification asking the user to refine.\n");
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
        "Date values MUST be a range in the form YYYY-MM-DD..YYYY-MM-DD, "
        "with op \"between\". A whole year like 2023 becomes "
        "2023-01-01..2023-12-31. A whole month like March 2024 becomes "
        "2024-03-01..2024-03-31. Never output a single bare date.\n"
        "Example: request \"photos from 2023 rated 5 stars\" produces:\n"
        "{ \"constraints\": [ "
        "{ \"field\": \"daterange\", \"op\": \"between\", "
        "\"value\": \"2023-01-01..2023-12-31\" }, "
        "{ \"field\": \"rating\", \"op\": \"eq\", \"value\": 5 } ] }\n");
}

QString SearchPromptBuilder::buildPrompt(const QString& userQuery,
                                         const QStringList& knownTags,
                                         const QStringList& knownAlbums,
                                         const QStringList& knownPeople) const
{
    QString prompt = m_systemPrompt;
    prompt        += QLatin1Char('\n');
    prompt        += schemaDescription();

    // Give the model a reference point for resolving relative dates
    // ("last year", "this month", ...) - it has no clock of its own.
    const QDate today = QDate::currentDate();

    prompt += QString::fromLatin1("\nToday's date is %1. Resolve relative dates "
                                  "against it: \"last year\" means %2-01-01..%2-12-31, "
                                  "\"this year\" means %3-01-01..%3-12-31.\n")
              .arg(today.toString(QLatin1String("yyyy-MM-dd")))
              .arg(today.year() - 1)
              .arg(today.year());

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
