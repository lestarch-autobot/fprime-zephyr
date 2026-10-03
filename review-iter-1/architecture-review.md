<!-- LOCAL REVIEW ONLY — iteration 1, head cb58109, not posted to GitHub -->

# architecture-review

<!-- fprime-agent: architecture-review v1 -->
Lens: `architecture-review` (review_label `Architecture`, review_group `architecture`, lens_kind `checklist`), iteration 1.
Change: lestarch-autobot/fprime-zephyr `b76db2b5...cb58109b` ("Add FprimeZephyrFatFilenameAllocator").
Verdict: **Go** (0 must fix, 1 suggestion, 0 could fix, 0 future work).
<!-- ledger_rows: 14 -->
<!-- unexplored_below_must_fix: 0 -->

## Applicability note

No `.fpp` model, topology, or deployment file changes, and no F Prime component is added or modified. The registry's
`routing_skip_when` for this lens would normally skip the change; it was run as asked. Step 1 (classify each touched
component) has nothing to classify, so categories 1-5 do not apply. Categories 6 (primitive misuse) and 7
(undocumented departure) were checked against the C++ and CMake changes.

## Findings

### F1. New top-level `Fs` package with no stated reason for not placing it under `Os`

- **finding-key:** `arch-undocumented-departure:fprime-zephyr.cmake:Fs-package`
- **site-key:** `fprime-zephyr.cmake:5`
- **class:** `arch-undocumented-departure`
- **severity:** `**suggestion**`
- **location:** `fprime-zephyr.cmake:5`, `fprime-zephyr/Fs/CMakeLists.txt:1`, `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:116` (`## Design`)
- **confidence:** low
- **description:** `[Architecture] **suggestion**` fprime-zephyr follows the F Prime package layout (`Os`, `Drv`, `Svc`). The
  Zephyr file-system layer is in `fprime-zephyr/Os/` (`File.cpp`, `FileSystem.cpp`, `Directory.cpp`, namespace
  `Os::Zephyr`), and those files are the F Prime code that calls FatFs. This PR adds a fourth top-level package `Fs`,
  which has no F Prime counterpart, and puts the allocator in namespace `Zephyr`, which `Drv` components use. Each
  choice may be reasonable: the module is not an `Os` interface implementation, and `Os/` contains only
  `register_fprime_implementation` targets. But neither the PR summary nor the SDD gives the reason, and later
  platform-support modules have no rule to follow.
- **suggested fix:** Either move the module to `fprime-zephyr/Os/FatFilenameAllocator/` beside the `Os::File` and
  `Os::FileSystem` implementations it supports, or keep `Fs/` and state why in the SDD's Design section, for example:
  ```suggestion
  ## Design

  The module lives in a separate `Fs` package, not `Os`, because it implements no F Prime `Os` interface: it replaces
  a Zephyr FatFs hook (`ff_memalloc`/`ff_memfree`) below the `Os::File`/`Os::FileSystem` implementations in `Os/`.
  ```
  (best-effort fix; verify before applying)
  cc @LeStarch @thomas-bc — low-confidence finding, please confirm.

## Ledger (working state, not findings)

| # | Unit | Disposition |
|---|---|---|
| 1 | Cat 1 sync-on-async component | clean: no component or FPP in the diff |
| 2 | Cat 2 async-on-passive component | clean: no component or FPP in the diff |
| 3 | Cat 3 kind/port incoherence | clean: no component or FPP in the diff |
| 4 | Cat 4 rate-group wiring | clean: no topology change; nothing connects to a rate group |
| 5 | Cat 5 unclassifiable component | clean: the pool is a library module, not a component; Step 1 does not apply |
| 6 | Cat 6 bespoke thread or custom queue | clean: no thread, task, or message queue added |
| 7 | Cat 6 raw lock instead of an F Prime primitive (`ZephyrSpinLockPolicy`, `FprimeZephyrFatFilenameAllocator.cpp:27`) | clean: no component exists to give a `guarded` port. `Os::Mutex` would need a runtime constructor, which conflicts with FZFA-007 (no startup constructor). FatFs calls the hook from arbitrary threads, so a `k_spinlock` in the platform layer fits, and this repo's own `Os/` already uses Zephyr kernel primitives directly |
| 8 | Cat 6 direct cross-component calls that bypass ports | clean: `__wrap_ff_memalloc`/`__wrap_ff_memfree` are called only by Zephyr FatFs, not by an F Prime component |
| 9 | Cat 6 global accessor `FprimeZephyrFatFilenameAllocator::getStats()` (`.cpp:51`) instead of telemetry | clean (user-approved decision 3). Risk: a deployment that wants to see exhaustion on the ground must write its own component to poll this accessor |
| 10 | Cat 7 module links itself into `app` and adds a global `--wrap` (`CMakeLists.txt:35-36`) outside the deployment `DEPENDS` model | clean (user-approved decision 1, documented in SDD "Configuration"). Risk: any deployment that includes fprime-zephyr with `LFN_MODE_HEAP=y` silently changes allocator behavior; the opt-out is the only control |
| 11 | Cat 7 new top-level `Fs` package and `Zephyr` namespace (`fprime-zephyr.cmake:5`) | finding F1 |
| 12 | Config header `FatFilenameAllocatorCfg.hpp` registered with `register_fprime_config` | clean: matches the `LoRaCfg.hpp` pattern in `default/zephyr-config` |
| 13 | Host UT (`register_fprime_ut` in the non-Zephyr branch) | out of scope: test-quality-review |
| 14 | SDD accuracy (slot sizes, behavior table) | out of scope: stale-documentation-review / correctness-review |

    findings: {'key': 'arch-undocumented-departure:fprime-zephyr.cmake:Fs-package', 'lens': 'architecture-review', 'class': 'arch-undocumented-departure', 'location': 'fprime-zephyr.cmake:5 (also fprime-zephyr/Fs/CMakeLists.txt:1, fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md:116)', 'severity': 'suggestion', 'confidence': 'low', 'description': 'fprime-zephyr follows the F Prime package layout (Os, Drv, Svc). The Zephyr file-system layer, which is the F Prime code that calls FatFs, is in fprime-zephyr/Os/ (File.cpp, FileSystem.cpp, Directory.cpp; namespace Os::Zephyr). This PR adds a fourth top-level package, Fs, which has no F Prime counterpart, and uses namespace Zephyr, which Drv components use. Neither the PR summary nor the SDD says why the module is not under Os. The choice may be reasonable: the module implements no Os interface, and Os/ contains only register_fprime_implementation targets.', 'suggested_fix': 'Either move the module to fprime-zephyr/Os/FatFilenameAllocator/, or keep Fs/ and add one sentence to the SDD Design section explaining why: the module implements no F Prime Os interface; it replaces a Zephyr FatFs hook below the Os::File/Os::FileSystem implementations. Best-effort fix; verify before applying. cc @LeStarch @thomas-bc (low confidence).'}
