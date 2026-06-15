/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Cache of successful natural-language interpretations
 *               so repeated queries skip model inference entirely.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

#include "digikam_export.h"

// Qt includes

#include <QHash>

// Local includes

#include "searchqueryintent.h"

namespace Digikam
{

class DIGIKAM_GUI_EXPORT SearchQueryCache
{
public:

    SearchQueryCache() = default;

    // Lookup by normalized query text (case-insensitive, trimmed).
    bool lookup(const QString& query, SearchQueryIntent* outIntent) const;

    void store(const QString& query, const SearchQueryIntent& intent);
    void clear();

    // Drop cached entries when the prompt/JSON schema changes.
    void invalidateSchemaVersion(int version);

    int size() const;

private:

    QString normalizeKey(const QString& query) const;

private:

    // In-memory only. Persistence to a small JSON file under
    // the digiKam config dir is planned for later.

    QHash<QString, SearchQueryIntent> m_entries;
    int                               m_schemaVersion = 1;
    static const int                  s_maxEntries    = 256;
};

} // namespace Digikam
