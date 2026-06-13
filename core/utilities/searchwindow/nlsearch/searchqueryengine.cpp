/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchqueryengine.h"
#include <klocalizedstring.h>

// Local includes

#include "searchlanguagebackend.h"
#include "searchpromptbuilder.h"
#include "searchintentparser.h"
#include "searchintentresolver.h"
#include "searchquerycache.h"
#include "digikam_debug.h"

namespace Digikam
{

SearchQueryEngine::SearchQueryEngine(QObject* const parent)
    : QObject(parent)
{
}

SearchQueryEngine::~SearchQueryEngine()
{
}

void SearchQueryEngine::setBackend(SearchLanguageBackend* const backend)
{
    if (m_backend)
    {
        disconnect(m_backend, nullptr, this, nullptr);
    }

    m_backend = backend;

    if (m_backend)
    {
        connect(m_backend, &SearchLanguageBackend::rawOutputReady,
                this, &SearchQueryEngine::slotRawOutputReady);

        connect(m_backend, &SearchLanguageBackend::inferenceError,
                this, &SearchQueryEngine::slotBackendError);
    }
}

void SearchQueryEngine::setPromptBuilder(SearchPromptBuilder* const builder)  { m_promptBuilder = builder;  }
void SearchQueryEngine::setParser(SearchIntentParser* const parser)          { m_parser        = parser;   }
void SearchQueryEngine::setResolver(SearchIntentResolver* const resolver)    { m_resolver      = resolver; }
void SearchQueryEngine::setCache(SearchQueryCache* const cache)              { m_cache         = cache;    }

void SearchQueryEngine::setKnownTags(const QStringList& tags)                { m_knownTags     = tags;     }
void SearchQueryEngine::setKnownAlbums(const QStringList& albums)            { m_knownAlbums   = albums;   }
void SearchQueryEngine::setKnownPeople(const QStringList& people)            { m_knownPeople   = people;   }

bool SearchQueryEngine::isReady() const
{
    return (m_backend && m_backend->isModelLoaded() &&
            m_promptBuilder && m_parser && m_resolver);
}

void SearchQueryEngine::interpretQuery(const QString& originalText,
                                       const QString& normalizedText)
{
    if (m_busy)
    {
        Q_EMIT statusMessage(i18n("A query is already being interpreted."));

        return;
    }

    m_originalQuery   = originalText;
    m_normalizedQuery = (normalizedText.isEmpty() ? originalText
                                                  : normalizedText);


    if (m_cache)
    {
        SearchQueryIntent cached;

        if (m_cache->lookup(m_normalizedQuery, &cached))
        {
            qCDebug(DIGIKAM_GENERAL_LOG) << "NL search: cache hit for" << m_normalizedQuery;

            QMetaObject::invokeMethod(this, [this, cached]()
            {
                Q_EMIT intentReady(cached);
            }, Qt::QueuedConnection);

            return;
        }
    }

    if (!isReady())
    {
        Q_EMIT errorOccurred(i18n("Natural-language search is not ready (model not loaded)."));

        return;
    }

    const QString prompt = m_promptBuilder->buildPrompt(m_normalizedQuery,
                                                        m_knownTags,
                                                        m_knownAlbums,
                                                        m_knownPeople);

    m_busy = true;

    Q_EMIT statusMessage(i18n("Interpreting your search..."));

    m_backend->runInference(prompt);
}

void SearchQueryEngine::cancel()
{
    if (m_backend)
    {
        m_backend->cancel();
    }

    m_busy = false;
}

void SearchQueryEngine::slotRawOutputReady(const QString& output)
{
    m_busy = false;

    SearchQueryIntent intent = m_parser->parse(output.toUtf8(),
                                               m_originalQuery,
                                               m_normalizedQuery);

    if (!intent.parseSucceeded)
    {
        Q_EMIT errorOccurred(i18n("Could not interpret the model output. "
                                "Please refine the description or create rules manually."));

        return;
    }

    const ResolvedSearchCriteria criteria = m_resolver->resolve(intent);

    if (intent.requiresClarification)
    {
        Q_EMIT clarificationRequired(intent);

        return;
    }

    if (criteria.resolvedConstraints.isEmpty())
    {
        // Nothing understood: explicit message, leave user in normal

        Q_EMIT errorOccurred(i18n("Could not map '%1' to tags, album names, captions, "
                                "location, or date. Please refine the description "
                                "or create rules manually.", m_originalQuery));

        return;
    }

    intent.constraints = criteria.resolvedConstraints;

    if (!criteria.unresolvedTerms.isEmpty())
    {
        Q_EMIT statusMessage(i18n("Some terms could not be mapped and were skipped: %1",
                          criteria.unresolvedTerms.join(QLatin1String(", "))));
    }

    if (m_cache)
    {
        m_cache->store(m_normalizedQuery, intent);
    }

    Q_EMIT intentReady(intent);
}

void SearchQueryEngine::slotBackendError(const QString& error)
{
    m_busy = false;

    Q_EMIT errorOccurred(error);
}

} // namespace Digikam
