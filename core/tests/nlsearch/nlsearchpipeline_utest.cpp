/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : End-to-end pipeline test:
 *               hardcoded query -> (mock) LLM -> parse -> resolve.
 *               Validates the resolution pipeline before any real
 *               model is integrated. The intent -> Search XML 
 *               serialization is not asserted here.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

// Qt includes

#include <QTest>
#include <QSignalSpy>

// Local includes

#include "searchqueryengine.h"
#include "searchmockbackend.h"
#include "searchpromptbuilder.h"
#include "searchintentparser.h"
#include "searchcapabilitydictionary.h"
#include "searchintentresolver.h"
#include "searchquerycache.h"

using namespace Digikam;

class NlSearchPipelineTest : public QObject
{
    Q_OBJECT

private:

    void buildEngine(SearchQueryEngine& engine,
                     SearchMockBackend& backend,
                     SearchPromptBuilder& prompts,
                     SearchIntentParser& parser,
                     SearchCapabilityDictionary& dict,
                     SearchIntentResolver& resolver,
                     SearchQueryCache& cache)
    {
        // The mock backend ignores the path, loadModel() only flips the
        // "ready" flag that SearchQueryEngine::isReady() checks. There is no
        // real model in this test. It validates the backend-independent pipeline.
        // Model loading is done by the llama backend (ENABLE_NLSEARCH_LLAMACPP).
        backend.loadModel(QLatin1String("mock-no-model"));

        engine.setBackend(&backend);
        engine.setPromptBuilder(&prompts);
        engine.setParser(&parser);
        engine.setResolver(&resolver);
        engine.setCache(&cache);
    }

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<SearchQueryIntent>("SearchQueryIntent");
        qRegisterMetaType<SearchQueryIntent>();     
    }

    void testCompositeQueryResolves()
    {
        SearchQueryEngine engine;
        SearchMockBackend backend;
        SearchPromptBuilder prompts;
        SearchIntentParser parser;
        SearchCapabilityDictionary dict;
        SearchIntentResolver resolver(&dict);
        SearchQueryCache cache;
        buildEngine(engine, backend, prompts, parser, dict, resolver, cache);

        QSignalSpy readySpy(&engine, &SearchQueryEngine::intentReady);

        engine.interpretQuery(QLatin1String("landscape photos with red labels"),
                              QString());

        QVERIFY(readySpy.wait(2000));

        const auto intent = readySpy.takeFirst().at(0).value<SearchQueryIntent>();
        QCOMPARE(intent.constraints.size(), 2);
        QCOMPARE(intent.constraints.at(0).field, QLatin1String("tag"));
        QCOMPARE(intent.constraints.at(1).field, QLatin1String("colorlabel"));
        QCOMPARE(intent.constraints.at(1).value, QLatin1String("red"));
    }

    // "best photos from last summer" => clarification, never a silent guess.
    void testAmbiguousQueryAsksForClarification()
    {
        SearchQueryEngine engine;
        SearchMockBackend backend;
        SearchPromptBuilder prompts;
        SearchIntentParser parser;
        SearchCapabilityDictionary dict;
        SearchIntentResolver resolver(&dict);
        SearchQueryCache cache;
        buildEngine(engine, backend, prompts, parser, dict, resolver, cache);

        QSignalSpy clarSpy(&engine, &SearchQueryEngine::clarificationRequired);
        QSignalSpy readySpy(&engine, &SearchQueryEngine::intentReady);

        engine.interpretQuery(QLatin1String("best photos from last summer"),
                              QString());

        QVERIFY(clarSpy.wait(2000));
        QCOMPARE(readySpy.count(), 0);

        const auto intent = clarSpy.takeFirst().at(0).value<SearchQueryIntent>();
        QVERIFY(intent.requiresClarification);
        QVERIFY(!intent.clarificationChoices.isEmpty());
    }

    // Unknown query => user-visible error, nothing invented.
    void testUnknownQueryProducesErrorNotGuess()
    {
        SearchQueryEngine engine;
        SearchMockBackend backend;
        SearchPromptBuilder prompts;
        SearchIntentParser parser;
        SearchCapabilityDictionary dict;
        SearchIntentResolver resolver(&dict);
        SearchQueryCache cache;
        buildEngine(engine, backend, prompts, parser, dict, resolver, cache);

        QSignalSpy errorSpy(&engine, &SearchQueryEngine::errorOccurred);

        engine.interpretQuery(QLatin1String("my happiest photos"), QString());

        QVERIFY(errorSpy.wait(2000));
    }

    void testCacheHitSkipsInference()
    {
        SearchQueryEngine engine;
        SearchMockBackend backend;
        SearchPromptBuilder prompts;
        SearchIntentParser parser;
        SearchCapabilityDictionary dict;
        SearchIntentResolver resolver(&dict);
        SearchQueryCache cache;
        buildEngine(engine, backend, prompts, parser, dict, resolver, cache);

        QSignalSpy readySpy(&engine, &SearchQueryEngine::intentReady);

        engine.interpretQuery(QLatin1String("photos from Paris in 2023"), QString());
        QVERIFY(readySpy.wait(2000));
        QCOMPARE(cache.size(), 1);

        // Unload model: a cache hit must still answer instantly.
        backend.unloadModel();

        engine.interpretQuery(QLatin1String("Photos from PARIS in 2023  "), QString());
        QVERIFY(readySpy.wait(2000));
        QCOMPARE(readySpy.count(), 2);
    }

    // Malformed model output is rejected, never applied.
    void testMalformedOutputRejected()
    {
        SearchIntentParser parser;

        const auto intent = parser.parse("not json at all { broken",
                                         QLatin1String("q"), QLatin1String("q"));
        QVERIFY(!intent.parseSucceeded);

        const auto intent2 = parser.parse(
            R"({ "constraints": [ { "field": "droptable", "op": "eq", "value": "x" } ] })",
            QLatin1String("q"), QLatin1String("q"));
        QVERIFY(!intent2.parseSucceeded);
    }
};

QTEST_GUILESS_MAIN(NlSearchPipelineTest)

#include "nlsearchpipeline_utest.moc"
