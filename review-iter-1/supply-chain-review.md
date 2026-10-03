<!-- LOCAL REVIEW ONLY — iteration 1, head cb58109, not posted to GitHub -->

# supply-chain-review

Lens: `supply-chain-review` (review_label `Supply Chain`, review_group `supply-chain`, lens_kind checklist, contributes_to_ci_safety: true)
Change: lestarch-autobot/fprime-zephyr `devin/1791051832-fat-filename-allocator`, base `b76db2b5` ... head `cb58109b` (1 commit, 10 files, +757)
Iteration 1, local review only (nothing posted).

## Findings

### SC-1 `[Supply Chain] **could fix**` Pool slot size mirrors upstream FatFs internals with no upstream-revision guard

- finding-key: `supply-chain-review:build-upstream-internal-coupling:FprimeZephyrFatFilenameAllocator.cpp:LFN_BUFFER_BYTES`
- class: build/test infrastructure (Scope 3); no listed class matches exactly. Nearest: dependency drift on a third-party module
- severity: could fix
- location: `fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:12` (static_assert) and `:18-24` (LFN_BUFFER_BYTES / EXFAT_DIR_BLOCK_BYTES)
- confidence: medium
- description: The module overrides a third-party symbol (`ff_memalloc`/`ff_memfree` via `-Wl,--wrap`) and serves only one hard-coded size, copied from private `ff.c` macros. In the Zephyr fatfs module pinned by Zephyr's `west.yml` (fatfs `f4ead3bf`, `FF_DEFINED 80386`), these are `INIT_NAMEBUFF` (`ff.c:545/549`) and `MAXDIRB` (`ff.c:518`). The only compile-time check is `FF_USE_LFN == 3`. If a Zephyr bump changes the FatFs revision and the request formula, the build still succeeds. Every path-taking FatFs call (`f_open`, `f_stat`, `f_mkdir`, ...) then fails with `FR_NOT_ENOUGH_CORE`/`-ENOMEM` at runtime, and the only sign is `rejectedCount`. Pinning the FatFs revision you verified turns silent drift into a build error that forces re-verification. Below must-fix: no current deployment is affected. Today's pinned revision matches the formula, which I checked against upstream `ff.c`.
- suggested fix (best-effort, verify before applying):
  ```cpp
  static_assert(FF_USE_LFN == 3, "FprimeZephyrFatFilenameAllocator requires CONFIG_FS_FATFS_LFN_MODE_HEAP");
  // LFN_BUFFER_BYTES / EXFAT_DIR_BLOCK_BYTES mirror INIT_NAMEBUFF / MAXDIRB of this FatFs revision; re-verify on change
  static_assert(FF_DEFINED == 80386, "FatFs revision changed: re-verify the ff_memalloc request size in ff.c");
  ```
  You could also add a link/UT check that compares against `ff.c`'s `INIT_NAMEBUFF` expression.
- cc @LeStarch @thomas-bc: low-confidence finding (lens-fit). Please confirm whether this belongs to supply-chain or correctness.

## Ledger (enumerate-then-evaluate, contract §13e)

| # | Unit | Disposition |
|---|---|---|
| 1 | `fprime-zephyr/Fs/FatFilenameAllocator/docs/sdd.md` (new): prompt-injection scan | clean: no HTML/markdown comments, no non-ASCII/zero-width chars, no reviewer-directed text |
| 2 | sdd.md "Other ff_memalloc callers" lists only `dir_clear()`/`f_mkfs()`. Upstream `ff.c:6565` `f_fdisk()` also calls `ff_memalloc(FF_MAX_SS)` when given no work buffer | out of scope (stale-documentation / correctness): documentation completeness. The pool rejects it, so no supply-chain impact |
| 3 | `default/zephyr-config/FatFilenameAllocatorCfg.hpp` (new) | clean: plain config constant, no external input |
| 4 | `FatFilenameAllocator.hpp` (new) | clean for this lens: no third-party code, no fetch. Assert/bounds are out of scope (security/correctness) |
| 5 | `FprimeZephyrFatFilenameAllocator.hpp` (new) | clean |
| 6 | `FprimeZephyrFatFilenameAllocator.cpp`: `#include <ff.h>`, `<zephyr/spinlock.h>`, constants mirroring `ff.c` | finding SC-1 (could fix) |
| 7 | `FprimeZephyrFatFilenameAllocator.cpp`: `__wrap_ff_memalloc`/`__wrap_ff_memfree` override third-party Zephyr symbols | clean: the vendored source is not edited (no `vendor-local-edit`), and the PR description explicitly justifies the downstream override. LTO is guarded by FATAL_ERROR |
| 8 | `test/ut/FatFilenameAllocatorTestMain.cpp` (new): prompt-injection scan of comments/strings | clean |
| 9 | UT deps: gtest (from F Prime), `find_package(Threads REQUIRED)` | clean: system/toolchain package, no new external dependency |
| 10 | `default/zephyr-config/CMakeLists.txt`: adds header to `register_fprime_config` | clean |
| 11 | `fprime-zephyr.cmake`: `add_fprime_subdirectory(.../Fs)` | clean |
| 12 | `fprime-zephyr/Fs/CMakeLists.txt` (new) | clean |
| 13 | `Fs/FatFilenameAllocator/CMakeLists.txt`: `option()`, CONFIG_LTO guard, `register_fprime_module`, `target_link_libraries(app ...)`, `zephyr_ld_options(-Wl,--wrap=...)` | clean for `cmake-unverified-fetch`: no FetchContent/ExternalProject/git clone/download, and no toolchain selection change. The global link-flag change is gated on `CONFIG_FS_FATFS_LFN_MODE_HEAP` with an opt-out (user-approved design) |
| 14 | Scope 1 dependency manifests (`ci/pyproject.toml`, `ci/setup.py`, requirements) | clean: not touched |
| 15 | Scope 2 submodules / vendored (`.gitmodules` absent; no vendored dirs touched) | clean |
| 16 | Scope 3 Dockerfile (`ci/docker/Dockerfile`), scripts | clean: not touched |
| 17 | Scope 4 workflows/actions (`.github/actions/setup`) | clean: not touched |
| 18 | Scope 5 generator output without input change | clean: no generated files in diff |
| 19 | Scope 6 commit message (cb58109b), title, summary, branch name | clean: no override phrases, HTML comments, base64 or invisible chars |
| 20 | Scope 7 review-system integrity (`.github/agents/**`, `.github/skills/**`) | clean: not touched |

## Hidden metadata (would-be review body; not posted)

```
<!-- fprime-agent: supply-chain-review; run: 1 -->
<!-- reviewed_head: cb58109bfa914f11c6338a291048c1e3e4d5aae6 -->
<!-- counts: must_fix=0; suggestion=0; could_fix=1; future_work=0 -->
<!-- ci_safety: Go -->
<!-- ci_safety_rationale: No must-fix; no deps/workflows/actions/containers/fetches touched; no prompt-injection; one could-fix on upstream FatFs coupling -->
<!-- surfaces:
- Dependencies: clean
- Vendored / submodule: clean
- Build / test infrastructure: 1 could-fix — ff_memalloc slot size mirrors ff.c INIT_NAMEBUFF/MAXDIRB with no FF_DEFINED revision guard
- Workflows / actions / scripts: clean
- Generator output: clean
- Prompt-injection: clean
- Review-system integrity: clean
-->
<!-- ledger_rows: 20 -->
```

    findings: {'key': 'supply-chain-review:build-upstream-internal-coupling:FprimeZephyrFatFilenameAllocator.cpp:LFN_BUFFER_BYTES', 'lens': 'supply-chain-review', 'class': 'build/test infrastructure (Scope 3) - upstream FatFs internal coupling; no listed class matches exactly', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/FprimeZephyrFatFilenameAllocator.cpp:12,18-24', 'severity': 'could fix', 'confidence': 'medium (low-confidence on lens fit; maintainer ping)', 'description': "The slot size copies private ff.c macros (INIT_NAMEBUFF ff.c:545/549, MAXDIRB ff.c:518 of the Zephyr fatfs module pinned in west.yml at f4ead3bf, FF_DEFINED 80386). The only compile-time guard is FF_USE_LFN==3. If a Zephyr/FatFs bump changes the request formula, the build still succeeds. Every path-taking FatFs call then returns FR_NOT_ENOUGH_CORE at runtime, and the only signal is rejectedCount. It matches today's pinned revision.", 'suggested_fix': 'Add static_assert(FF_DEFINED == 80386, "FatFs revision changed: re-verify the ff_memalloc request size in ff.c"); next to the existing FF_USE_LFN static_assert (best-effort; verify before applying).'}
