/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Maintainable mapping layer between natural-language
 *               terms and digiKam Advanced Search capabilities.
 *               Extending search support means editing this table,
 *               not retraining the model.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

#include "digikam_export.h"

// Qt includes

#include <QMap>
#include <QString>
#include <QStringList>

namespace Digikam
{

class DIGIKAM_GUI_EXPORT DictionaryEntry
{
public:

    DictionaryEntry() = default;

    QString     canonicalField; // e.g. "colorlabel"
    QStringList aliases;        // e.g. { "color label", "colour label" }
    QStringList allowedValues;  // e.g. { "red", "orange", ... }; empty = free-form
};

class SearchCapabilityDictionary
{
public:

    SearchCapabilityDictionary();

    void initializeDefaults();

    // "color label" -> "colorlabel". Returns false if unknown.
    bool resolveFieldAlias(const QString& alias, QString* canonicalField) const;

    // ("picklabel", "best") -> "accepted". Returns false if unknown.
    bool resolveValue(const QString& field,
                      const QString& rawValue,
                      QString* resolvedValue) const;

    QStringList supportedFields()                                  const;

    // Clarification choices for an ambiguous term ("best" -> Pick Label / Rating).
    QStringList choicesForAmbiguousWord(const QString& word)       const;

    bool isKnownTag(const QString& tag)                            const;
    void setKnownTags(const QStringList& tags);

private:

    QMap<QString, DictionaryEntry>  m_fieldMap;      // canonical field -> entry
    QMap<QString, QStringList>      m_valueAliases;  // "field/rawvalue" -> resolved
    QMap<QString, QStringList>      m_ambiguousWords;
    QStringList                     m_knownTags;
};

} // namespace Digikam
