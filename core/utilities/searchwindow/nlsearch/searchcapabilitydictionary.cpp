/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchcapabilitydictionary.h"

// Qt includes

#include <QMap>

namespace Digikam
{

class Q_DECL_HIDDEN DictionaryEntry
{
public:

    DictionaryEntry() = default;

    QString     canonicalField;   ///< e.g. "colorlabel"
    QStringList aliases;          ///< e.g. { "color label", "colour label" }
    QStringList allowedValues;    ///< e.g. { "red", "orange", ... }; empty = free-form
};

class Q_DECL_HIDDEN SearchCapabilityDictionary::Private
{
public:

    Private() = default;

    QMap<QString, DictionaryEntry>  fieldMap;       ///< canonical field -> entry
    QMap<QString, QStringList>      valueAliases;   ///< "field/rawvalue" -> resolved
    QMap<QString, QStringList>      ambiguousWords;
    QStringList                     knownTags;
};

SearchCapabilityDictionary::SearchCapabilityDictionary()
    : d(new Private)
{
    initializeDefaults();
}

SearchCapabilityDictionary::~SearchCapabilityDictionary()
{
    delete d;
}

void SearchCapabilityDictionary::initializeDefaults()
{
    d->fieldMap.clear();
    d->valueAliases.clear();
    d->ambiguousWords.clear();

    auto addField = [this](const QString& canonical,
                           const QStringList& aliases,
                           const QStringList& allowedValues = {})
    {
        DictionaryEntry e;
        e.canonicalField = canonical;
        e.aliases        = aliases;
        e.allowedValues  = allowedValues;
        d->fieldMap.insert(canonical, e);
    };

    addField(QLatin1String("tag"),
             { QLatin1String("tag"), QLatin1String("tags"), QLatin1String("keyword") });

    addField(QLatin1String("album"),
             { QLatin1String("album"), QLatin1String("folder") });

    addField(QLatin1String("person"),
             { QLatin1String("person"), QLatin1String("people"), QLatin1String("face") });

    addField(QLatin1String("place"),
             { QLatin1String("place"), QLatin1String("location"), QLatin1String("near") });

    addField(QLatin1String("daterange"),
             { QLatin1String("date"), QLatin1String("taken"), QLatin1String("time") });

    addField(QLatin1String("rating"),
             { QLatin1String("rating"), QLatin1String("stars") });

    addField(QLatin1String("picklabel"),
             { QLatin1String("pick label"), QLatin1String("pick") },
             { QLatin1String("none"), QLatin1String("rejected"),
               QLatin1String("pending"), QLatin1String("accepted") });

    addField(QLatin1String("colorlabel"),
             { QLatin1String("color label"), QLatin1String("colour label"),
               QLatin1String("label") },
             { QLatin1String("none"), QLatin1String("red"), QLatin1String("orange"),
               QLatin1String("yellow"), QLatin1String("green"), QLatin1String("blue"),
               QLatin1String("magenta"), QLatin1String("gray"), QLatin1String("black"),
               QLatin1String("white") });

    addField(QLatin1String("orientation"),
             { QLatin1String("orientation") },
             { QLatin1String("landscape"), QLatin1String("portrait") });

    addField(QLatin1String("caption"),
             { QLatin1String("caption"), QLatin1String("description"),
               QLatin1String("comment") });

    d->valueAliases.insert(QLatin1String("picklabel/best"),
                           { QLatin1String("accepted") });

    d->valueAliases.insert(QLatin1String("picklabel/flagged"),
                           { QLatin1String("accepted") });

    d->valueAliases.insert(QLatin1String("rating/high rating"),
                           { QLatin1String("4") });

    // ambiguous words -> clarification choices

    d->ambiguousWords.insert(QLatin1String("best"),
                             { QLatin1String("Pick Label: Accepted"),
                               QLatin1String("Rating >= 4") });

    d->ambiguousWords.insert(QLatin1String("favorite"),
                             { QLatin1String("Pick Label: Accepted"),
                               QLatin1String("Rating = 5") });
}

bool SearchCapabilityDictionary::resolveFieldAlias(const QString& alias,
                                                   QString* canonicalField) const
{
    const QString needle = alias.trimmed().toLower();

    for (auto it = d->fieldMap.constBegin() ; it != d->fieldMap.constEnd() ; ++it)
    {
        if ((it.key() == needle) || it.value().aliases.contains(needle))
        {
            if (canonicalField)
            {
                *canonicalField = it.key();
            }

            return true;
        }
    }

    return false;
}

bool SearchCapabilityDictionary::resolveValue(const QString& field,
                                              const QString& rawValue,
                                              QString* resolvedValue) const
{
    const QString needle = rawValue.trimmed().toLower();
    const QString key    = field + QLatin1Char('/') + needle;

    if (d->valueAliases.contains(key))
    {
        if (resolvedValue)
        {
            *resolvedValue = d->valueAliases.value(key).first();
        }

        return true;
    }

    const DictionaryEntry entry = d->fieldMap.value(field);

    if (entry.allowedValues.isEmpty() || entry.allowedValues.contains(needle))
    {
        if (resolvedValue)
        {
            *resolvedValue = needle;
        }

        return true;
    }

    return false;
}

QStringList SearchCapabilityDictionary::supportedFields() const
{
    return d->fieldMap.keys();
}

QStringList SearchCapabilityDictionary::choicesForAmbiguousWord(const QString& word) const
{
    return d->ambiguousWords.value(word.trimmed().toLower());
}

bool SearchCapabilityDictionary::isKnownTag(const QString& tag) const
{
    return d->knownTags.contains(tag, Qt::CaseInsensitive);
}

void SearchCapabilityDictionary::setKnownTags(const QStringList& tags)
{
    d->knownTags = tags;
}

} // namespace Digikam