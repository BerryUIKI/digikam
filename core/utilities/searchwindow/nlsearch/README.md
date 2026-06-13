# Interface the database search engine to an AI-based LLM


 A user
types a description such as *"landscape photos with red labels"*; the pipeline
maps that text to a structured intent, converts it to digiKam's Search XML, and
populates the existing Advanced Search widgets. The LLM produces what
a human clicking the dialog could produce.

## Pipeline overview

```
User query text
      │
SearchQueryEngine  ──(check)──>  SearchQueryCache        (hit: return early)
      │
SearchPromptBuilder            (system rules + JSON schema + collection hints)
      │
SearchLanguageBackend          (mock now / llama later, async, off GUI thread)
      │
SearchIntentParser             (extract JSON, whitelist-validate every field/op)
      │
SearchIntentResolver  <──────>  SearchCapabilityDictionary   (aliases, values)
      │
emits exactly one of:
   intentReady              > populate Advanced Search via Search XML
   clarificationRequired    > ask the user (e.g. "best" is ambiguous)
   errorOccurred            > explain; leave the dialog untouched
```



## Classes

### `SearchQueryIntent` / `SearchQueryConstraint`  (`searchqueryintent.h`)

Value types that carry a parsed query through the pipeline.

`SearchQueryConstraint` is one constraint: `field` (canonical name such as
`tag`, `daterange`, `colorlabel`), `op` (`eq`, `contains`, `gte`, `lte`,
`between`), and `value`. `isValid()` reports whether field and op are set.

`SearchQueryIntent` is the full result: the list of constraints, clarification
state (`requiresClarification`, `clarificationMessage`, `clarificationChoices`),
the verbatim `originalQuery` and the `normalizedQuery` used as the cache key,
the raw model output for debugging, and `parseSucceeded`. Helpers: `isEmpty()`,
`isAmbiguous()`. Both types are registered with `Q_DECLARE_METATYPE` so they
travel through queued signal/slot connections.

### `SearchLanguageBackend`  (`searchlanguagebackend.h/.cpp`)

Abstract `QObject` interface that isolates the pipeline from any specific model
runtime.

- `loadModel(path)` / `unloadModel()` / `isModelLoaded()` - model lifecycle.
- `runInference(prompt)` *(slot)* - must return immediately; the result arrives
  later via signal. This contract is what keeps the GUI responsive.
- `cancel()` *(slot)* - best-effort cancellation.
- Signals: `rawOutputReady(QString)`, `inferenceError(QString)`,
  `modelLoaded()`, `inferenceProgress(...)`.

### `SearchMockBackend`  (`searchmockbackend.h/.cpp`)

Canned-response backend implementing the abstract interface. Lets the entire
pipeline run and be unit-tested without a real model, and remains useful for CI
permanently.

- `addCannedResponse(querySubstring, json)` - register a fixed output.
- `runInference(prompt)` - substring-matches the prompt and emits the matching
  JSON via `QTimer::singleShot(0, …)`, so the asynchronous signal ordering is
  identical to a real backend. An unmatched prompt yields a valid-but-empty
  schema, which exercises the engine's "nothing understood" path.
- The constructor pre-loads the proposal's demonstration queries.

### `SearchLlamaBackend`  (`searchllamabackend.h/.cpp`)  *(optional)*

llama.cpp / GGUF backend, compiled only when `ENABLE_NLSEARCH_LLAMACPP=ON`
(off by default, so master builds stay clean). The front-end object is a thin
signal router; a `SearchLlamaWorker` lives on a `QThread` and owns the model and
context as opaque handles so `llama.h` never leaks into other headers.

- `loadModel` / `runInference` / `cancel` forward to the worker via queued
  signals; `setMaxTokens(int)` and `setTemperature(float)` set generation params
  (temperature 0 = greedy, deterministic - desirable for parsed output).
- The token decode loop is the integration point (TODO).

### `SearchPromptBuilder`  (`searchpromptbuilder.h/.cpp`)

Builds the constrained prompt sent to the backend.

- `buildPrompt(query, knownTags, knownAlbums, knownPeople)` - assembles the
  system rules, the JSON output schema, and collection hints (capped per list so
  the prompt stays within small-model context windows), followed by the user's
  request.
- `schemaDescription()` / `systemPrompt()` - the supported fields, operators,
  ISO date format, and the "respond only with JSON, never invent metadata"
  instructions.

### `SearchIntentParser`  (`searchintentparser.h/.cpp`)

Turns raw model output into a validated `SearchQueryIntent`. Treats model output
as untrusted.

- `parse(rawOutput, original, normalized)` - extracts the first balanced JSON
  object (tolerating prose around it), parses it, and builds the intent. Any
  unknown field or operator rejects the **whole** output (`parseSucceeded =
  false`) rather than applying part of it.
- `validate(intent)` / `validationErrors(intent)` - whitelist checks; range
  operators are restricted to the fields that support them.

### `SearchCapabilityDictionary`  (`searchcapabilitydictionary.h/.cpp`)

The maintainable map between natural-language terms and digiKam capabilities.
Extending search support means editing this table, not retraining a model.

- `initializeDefaults()` - field aliases (e.g. *"location"* → `place`),
  enumerated allowed values (color labels, pick labels, orientation), value
  aliases, and ambiguous-word entries with their clarification choices.
- `resolveFieldAlias(alias, out)` / `resolveValue(field, raw, out)` - map a term
  to its canonical form; free-form fields pass through, enumerated fields must
  match.
- `choicesForAmbiguousWord(word)` - clarification options for words like
  *"best"*.
- `setKnownTags(...)` / `isKnownTag(...)` - collection awareness.

### `SearchIntentResolver`  (`searchintentresolver.h/.cpp`)

Turns a parsed intent into concrete, applicable criteria, deciding what can be
applied and what must be surfaced to the user.

- `resolve(intent)` → `ResolvedSearchCriteria` - resolves each constraint
  through the dictionary; anything unresolvable goes into `unresolvedTerms`
  (shown to the user, never silently dropped) rather than being invented.
- `canApplyDirectly(intent)` - true when there are results and no clarification
  is pending.

### `SearchQueryCache`  (`searchquerycache.h/.cpp`)

In-memory cache so repeated queries skip inference.

- `lookup(query, out)` / `store(query, intent)` - keyed on the trimmed,
  lower-cased, simplified query; only successful, non-ambiguous intents are
  cached. Bounded (256 entries).
- `clear()` / `size()` / `invalidateSchemaVersion(v)` - maintenance; the last
  lets persisted caches expire when the prompt schema changes.

### `SearchNlModelManager`  (`searchnlmodelmanager.h/.cpp`)  *(skeleton)*

Model-file lifecycle. Bridges to digiKam's `DNNModelManager` and
`FilesDownloader` (the same mechanism the face/auto-tag models use) so the
backend simply receives a local path.

- `defaultModelPath()` / `isModelAvailable()` *(static)* - path conventions.
- `ensureModelAvailable()` - emits `modelReady` if present.
- Signals: `downloadProgress`, `modelReady`, `modelError`.

### `SearchQueryEngine`  (`searchqueryengine.h/.cpp`)

The coordinator - the only class that knows the pipeline order.

- `setBackend / setPromptBuilder / setParser / setResolver / setCache` -
  dependency injection (the unit test wires the same engine with the mock;
  `SearchWindow` wires it with production parts). `setBackend` safely re-wires
  signal connections.
- `setKnownTags / setKnownAlbums / setKnownPeople` - prompt hints.
- `interpretQuery(original, normalized)` *(slot)* - the entry point. Busy-guard,
  cache check, prompt build, then `runInference`. An empty `normalized` falls
  back to `original` (the translation-failure contract). Cache hits are emitted
  asynchronously so timing matches the inference path.
- `slotRawOutputReady(output)` - parse → resolve → emit one of `intentReady`,
  `clarificationRequired`, or `errorOccurred`; reports skipped terms via
  `statusMessage` and caches successful results.
- Signals: `intentReady`, `clarificationRequired`, `errorOccurred`,
  `statusMessage`.

---

## Building and testing

The pipeline builds into digiKam by default; no extra flags are required.

### Unit tests

```sh
cmake -DBUILD_TESTING=ON ..
make -j$(nproc) nlsearchpipeline_utest
ctest -R nlsearch --output-on-failure
```
---

