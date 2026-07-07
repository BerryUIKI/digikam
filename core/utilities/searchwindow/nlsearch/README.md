## 1. Architecture

Two concrete backends implement the abstract `SearchLanguageBackend`:

| Backend               | Purpose                                             | Model required |
|-----------------------|-----------------------------------------------------|----------------|
| `SearchMockBackend`   | Canned responses; default build, pipeline dev/tests | No             |
| `SearchLlamaBackend`  | Real GGUF inference via llama.cpp on a worker thread| Yes            |

`SearchLlamaBackend` runs all `llama_*` calls on a dedicated `QThread`
(`SearchLlamaWorker`), so model loading and inference never block the GUI.
Results are delivered back through queued signals.

The default model is **Qwen2.5-1.5B-Instruct** (Q4_K_M, ~1 GB, Apache-2.0).
Decoding is greedy (temperature 0) because the output must be reproducible,
parseable JSON rather than creative text.

---

## 2. Enabling the llama.cpp backend

The backend is **OFF by default**. A normal build uses only the mock backend
and links nothing from llama.cpp. To build the real backend:

    cmake .. -DENABLE_NLSEARCH_LLAMACPP=ON \
             -DCMAKE_BUILD_TYPE=Release \
             -DOpenCV_DIR=/opt/opencv-4.8/lib/cmake/opencv4

### 2.1 llama.cpp submodule

llama.cpp is bundled as a **pinned git submodule** under
`core/utilities/searchwindow/thirdparty/llama.cpp`, pinned to tag **b9859**,
and built in-tree via `add_subdirectory`. Initialize it before configuring:

    git submodule update --init --recursive

It is built **library-only** and **CPU-only** (no CUDA/Metal/Vulkan), which keeps the dependency footprint to
essentially pthread + OpenMP. The ggml compute engine is bundled inside
llama.cpp; there is no external ggml dependency.

### 2.2 Build type and inference speed

**Important:** llama.cpp / ggml are roughly 30x slower when compiled without
optimization. A plain `Debug` build makes a single query take tens of seconds.

To keep developers able to build the rest of digiKam in Debug while still
getting usable inference, the bundled llama/ggml targets are forced to compile
with optimization even in Debug builds (see the CMake notes below). End users
should still build with `-DCMAKE_BUILD_TYPE=Release`.

---

## 3. How llama.cpp was integrated (CMake)

Adding a git submodule and building it in-tree touched several places. Each
change is small and documented here so the integration can be reviewed and
maintained.

### 3.1 The submodule

    git submodule add https://github.com/ggml-org/llama.cpp.git \
        core/utilities/searchwindow/thirdparty/llama.cpp
    cd core/utilities/searchwindow/thirdparty/llama.cpp && git checkout b9859

This created `.gitmodules` (digiKam's first submodule) and records the pinned
commit. The submodule is hosted under `searchwindow/` (which belongs to the
`digikamgui` shared library) rather than `core/libs/` (which builds
`digikamcore`, shared with Showfoto and subject to a tighter MSVC object-count
limit). llama.cpp is only needed by the digiKam application, not by Showfoto.

### 3.2 `core/CMakeLists.txt`

  - Sets `HAVE_LLAMACPP` when `ENABLE_NLSEARCH_LLAMACPP` is ON, **before**
    `digikam_config.h` is generated, so the flag reaches the compiler:

        if(ENABLE_NLSEARCH_LLAMACPP)
            set(HAVE_LLAMACPP TRUE)
        endif()

  - The old `find_package(Llama)` / `MACRO_BOOL_TO_01(Llama_FOUND ...)` lines
    were removed; the submodule is always present when the option is on, so
    there is nothing to "find".

### 3.3 `core/app/utils/digikam_config.h.cmake.in`

  - Added `#cmakedefine HAVE_LLAMACPP 1` so the CMake variable becomes a real
    compiler define.

### 3.4 `core/utilities/searchwindow/CMakeLists.txt`

  - Before including the nlsearch sources: disables llama's demo apps/tools/
    tests and the GPU backends, then `add_subdirectory(thirdparty/llama.cpp)`.
  - Forces optimization on the bundled targets even in Debug builds:

        foreach(_t llama ggml ggml-base ggml-cpu)
            if(TARGET ${_t})
                target_compile_options(${_t} PRIVATE $<$<CONFIG:Debug>:-O2>)
            endif()
        endforeach()

  - Adds llama's include directories to `gui_searchwindow_obj` (an OBJECT
    library, so it needs headers to compile but does not link).

### 3.5 `core/app/DigikamGuiTarget.cmake`

  - `digikamgui` is exported via `install(EXPORT DigikamGuiConfig)`. A bundled
    in-tree target cannot appear in that export set, so llama is linked by its
    built file rather than by target name, and an explicit build-order
    dependency is added:

        target_link_libraries(digikamgui PRIVATE $<TARGET_FILE:llama>)
        target_include_directories(digikamgui PRIVATE .../thirdparty/llama.cpp/include)
        add_dependencies(digikamgui llama)

    `libllama` carries its own ggml dependency, so ggml need not be linked
    explicitly.

### 3.6 `core/cmake/macros/MacroUtils.cmake`

  - `HEADER_DIRECTORIES()` recursively globs `*.h` and feeds every directory
    into the global include path. With llama.cpp now living inside
    `core/utilities`, that swept the submodule's headers into unrelated targets
    (dimg, facesengine, ...) and broke their compilation. Fixed by excluding
    vendored code from the sweep:

        list(FILTER new_list EXCLUDE REGEX "/thirdparty/")

    This is a latent issue that any future vendored code would also have hit.

---

## 4. Code changes (summary)

  - `searchllamabackend.{h,cpp}` : real model loading (`slotDoLoad`), the
    decode loop (`slotDoInference`) with greedy sampling and an early stop once
    a balanced JSON object is produced, and cleanup (`slotDoUnload`).
  - `searchwindow.cpp` : selects the llama backend when the feature is built
    and the model is installed, otherwise falls back to the mock backend.
  - `searchintentparser.cpp` : handles JSON values that arrive as numbers or
    booleans, not only strings (a numeric rating was previously dropped).
  - `searchpromptbuilder.cpp` : enforces `start..end` date ranges, injects the
    current date so relative dates ("last year") resolve correctly, and
    strengthens the instruction to abstain rather than guess.
  - `searchqueryengine.cpp` : a clearer "model not installed" vs. "model still
    loading" message.

---

## 5. Testing

### 5.1 Unit tests (no model required)

The pipeline is covered by
`core/tests/nlsearch/nlsearchpipeline_utest.cpp`, which runs entirely against
the mock backend:

    cd <build-dir>
    ctest -R nlsearchpipeline_utest --output-on-failure

It covers composite queries, clarification, error-not-guess behaviour, cache
hits, malformed-output rejection, and two regressions found during
development: numeric JSON values and date-range preservation.

### 5.2 Manual verification of real inference

With the backend enabled and the model installed:

  1. Build with `-DENABLE_NLSEARCH_LLAMACPP=ON -DCMAKE_BUILD_TYPE=Release`.
  2. Launch digiKam; open Advanced Search (or the sidebar search field).
  3. Type a request, e.g. *"photos from 2023 rated 5 stars"*, and press Enter.
  4. The Advanced Search criteria populate (date range 2023-01-01..2023-12-31,
     rating 5) and the matching photos are returned.

Model load takes a few seconds on first use; querying before it finishes shows
the "still loading" message.


---

## 6. Getting the model

The Qwen2.5-1.5B model is hosted on the KDE mirrors and downloaded through
digiKam's standard model download dialog: tick **"Use Natural Language Search
feature"**, download, and the file is SHA-256 verified and placed in the shared
model directory. **Restart digiKam** after downloading.

If the model is not installed, the feature degrades gracefully to the mock
backend and the search field reports that the model needs to be downloaded.

