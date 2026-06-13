/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchquerycache.h"

namespace Digikam
{

SearchQueryCache::SearchQueryCache()
{
}

QString SearchQueryCache::normalizeKey(const QString& query) const
{
    return query.trimmed().toLower().simplified();
}

bool SearchQueryCache::lookup(const QString& query, SearchQueryIntent* outIntent) const
{
    const auto it = m_entries.constFind(normalizeKey(query));

    if (it == m_entries.constEnd())
    {
        return false;
    }

    if (outIntent)
    {
        *outIntent = it.value();
    }

    return true;
}

void SearchQueryCache::store(const QString& query, const SearchQueryIntent& intent)
{
    // Only cache fully successful, non-ambiguous interpretations.
    if (!intent.parseSucceeded || intent.requiresClarification)
    {
        return;
    }

    if (m_entries.size() >= s_maxEntries)
    {
        m_entries.erase(m_entries.begin());
    }

    m_entries.insert(normalizeKey(query), intent);
}

void SearchQueryCache::clear()
{
    m_entries.clear();
}

void SearchQueryCache::invalidateSchemaVersion(int version)
{
    if (version != m_schemaVersion)
    {
        clear();
        m_schemaVersion = version;
    }
}

int SearchQueryCache::size() const
{
    return m_entries.size();
}

} // namespace Digikam
