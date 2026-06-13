/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchintentresolver.h"

// Local includes

#include "searchcapabilitydictionary.h"

namespace Digikam
{

SearchIntentResolver::SearchIntentResolver(SearchCapabilityDictionary* const dictionary)
    : m_dictionary(dictionary)
{
}

ResolvedSearchCriteria SearchIntentResolver::resolve(const SearchQueryIntent& intent) const
{
    ResolvedSearchCriteria result;

    if (!intent.parseSucceeded || !m_dictionary)
    {
        return result;
    }

    for (const SearchQueryConstraint& in : intent.constraints)
    {
        SearchQueryConstraint out;

        if (resolveConstraint(in, &out))
        {
            result.resolvedConstraints << out;
        }
        else
        {
            // Never silently invent metadata: surface it instead.
            result.unresolvedTerms << QString::fromLatin1("%1: %2").arg(in.field, in.value);
        }
    }

    result.canApplyDirectly = (!result.resolvedConstraints.isEmpty() &&
                               !intent.requiresClarification);

    return result;
}

bool SearchIntentResolver::canApplyDirectly(const SearchQueryIntent& intent) const
{
    return resolve(intent).canApplyDirectly;
}

bool SearchIntentResolver::resolveConstraint(const SearchQueryConstraint& in,
                                             SearchQueryConstraint* out) const
{
    if (!in.isValid() || !out)
    {
        return false;
    }

    QString canonicalField;

    if (!m_dictionary->resolveFieldAlias(in.field, &canonicalField))
    {
        return false;
    }

    QString resolvedValue;

    if (!m_dictionary->resolveValue(canonicalField, in.value, &resolvedValue))
    {
        return false;
    }

    out->field = canonicalField;
    out->op    = in.op;
    out->value = resolvedValue;

    return true;
}

} // namespace Digikam
