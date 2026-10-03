<!-- LOCAL REVIEW ONLY — iteration 1, head cb58109, not posted to GitHub -->

# test-quality-review

Local review, iteration 1. Repository: lestarch-autobot/fprime-zephyr.
Base: `b76db2b58025ea80efbe9999a4f1a15e95201775`.
Head: `cb58109bfa914f11c6338a291048c1e3e4d5aae6`.
Fresh review definitions: nasa/fprime `devel` at `55f597d76f8749b647068fd7aa3581a14dab6ff9`.

## Findings

### 1. Factor the repeated invalid-release assertion pattern

[ Test Quality ] **suggestion**

- **Finding key:** `c5111501494b0c7f95d1216e768f40290d8b9f9012e5c258d3ec1426cf54fa69`
- **Class:** `test-copy-paste-structure`
- **Severity:** suggestion (non-blocking)
- **Location:** `fprime-zephyr/Fs/FatFilenameAllocator/test/ut/FatFilenameAllocatorTestMain.cpp:238-244`
- **Confidence:** high

**Description:** ForeignPointersAssert (197-205), DoubleFreeAsserts (221-228), and ReleaseOfNeverAllocatedSlotAsserts (238-244) repeat the same register-hook → release → deregister-hook → check assert count/index and pool state sequence. This meets the lens's three-case repetition threshold and requires maintaining the hook lifecycle and common verification in three places.

**Suggested fix:** Extract a parameterized expectReleaseAssert helper into FatFilenameAllocatorTest, taking the pointer, expected first assert argument and expected in-use count. Return the recorded count so ForeignPointersAssert retains its at-least-one check while DoubleFreeAsserts and ReleaseOfNeverAllocatedSlotAsserts still require exactly one. Keep each test's distinct setup and subsequent reuse checks.

Concrete helper addition immediately before the fixture's `TestPool m_pool;` declaration (line 69):

```suggestion
    FwSizeType expectReleaseAssert(void* ptr, FwAssertArgType expectedIndex, FwSizeType expectedInUse) {
        RecordingAssertHook hook;
        hook.registerHook();
        this->m_pool.release(ptr);
        hook.deregisterHook();
        EXPECT_GE(hook.m_count, 1U);
        EXPECT_EQ(hook.m_firstArg, expectedIndex);
        EXPECT_EQ(this->m_pool.getStats().inUse, expectedInUse);
        return hook.m_count;
    }
    TestPool m_pool;
```

Replace lines 238-244 with:

```suggestion
    EXPECT_EQ(this->expectReleaseAssert(slot1, static_cast<FwAssertArgType>(1), 1U), 1U);
```

Use the same helper in `ForeignPointersAssert` with `TEST_SLOT_COUNT` / `1U`, and in `DoubleFreeAsserts` with `3` / `TEST_SLOT_COUNT - 1U`; retain the latter's exact-count and reuse assertions.

<!-- fprime-agent: test-quality-review; finding-key: c5111501494b0c7f95d1216e768f40290d8b9f9012e5c258d3ec1426cf54fa69; site-key: 91632c8f66e6a28fa2d883240debac669cc89179b67afa652fed44458c91fd0d; v2 -->

## Review scope and validation

All 10 changed files were read in full at the requested head, and all 11 test-quality categories were evaluated; working ledger: 35 rows, no unexplored candidates. No FPP surface is added or modified. No blocking findings were identified.

Dedicated coverage of non-FPP helpers, linker integration, target hardware and coverage metrics is outside this lens. No tests were rerun; the author's reported host/sanitizer and deployment-link results are not independent verification by this review.

Verdict: **Go** for this lens (0 must fix, 1 suggestion, 0 could fix, 0 future work).
No repository source edits or GitHub posts, pushes, commits, statuses, labels, issues or PRs were made.

    findings: {'key': 'c5111501494b0c7f95d1216e768f40290d8b9f9012e5c258d3ec1426cf54fa69', 'lens': 'test-quality-review', 'class': 'test-copy-paste-structure', 'location': 'fprime-zephyr/Fs/FatFilenameAllocator/test/ut/FatFilenameAllocatorTestMain.cpp:238-244', 'severity': 'suggestion', 'confidence': 'high', 'description': "ForeignPointersAssert (197-205), DoubleFreeAsserts (221-228), and ReleaseOfNeverAllocatedSlotAsserts (238-244) repeat the same register-hook → release → deregister-hook → check assert count/index and pool state sequence. This meets the lens's three-case repetition threshold and requires maintaining the hook lifecycle and common verification in three places.", 'suggested_fix': "Extract a parameterized expectReleaseAssert helper into FatFilenameAllocatorTest, taking the pointer, expected first assert argument and expected in-use count. Return the recorded count so ForeignPointersAssert retains its at-least-one check while DoubleFreeAsserts and ReleaseOfNeverAllocatedSlotAsserts still require exactly one. Keep each test's distinct setup and subsequent reuse checks."}
