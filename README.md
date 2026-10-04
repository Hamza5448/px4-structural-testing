SQE ASSIGNMENT 2 - FINAL SUBMISSION
Status: READY FOR ZIP. No ZIP created; section is not recorded.
Students: Hamza Ahmad (24I-3104), Irtaza Abid (24I-3085), Fizza Fatima Niaz Tour (24I-3076).
System: PX4-Autopilot v1.17.0
Exact baseline: d6f12ad1c4f70ad3230afd7d86e971421e02fef4
Authoritative repository: /home/hamzaahmad/PX4-Autopilot (WSL).
The Windows clone is not the authoritative source for final testing evidence.

CONTENTS
Report/: final existing report with targeted technical corrections.
Workbook/: two sheets; Test Inventory has 18 actual GoogleTests (13 Failsafe, 5 Takeoff), eight required columns; MC-DC Evidence has five rows.
Tests/: two student test sources and the two complete relevant CMakeLists files. Failsafe_CMakeLists.txt corresponds to src/modules/commander/failsafe/CMakeLists.txt; Takeoff_CMakeLists.txt corresponds to src/modules/mc_pos_control/Takeoff/CMakeLists.txt.
Patch/: four-file source-only patch and clean-baseline validation result.
Coverage/: authentic upstream baseline, isolated verified student final, gaps, provenance, traceability, reproduction scripts, execution/full-regression logs, and HTML.
Evidence/: 13 retained historical screenshots, document corrections/validation and final package validation.
AI_USAGE_RECORD.txt: assistance disclosure.
SUBMISSION_MANIFEST.json: SHA-256 and size of every packaged file except the manifest itself.

VERIFIED RESULTS
Takeoff upstream: lines 36/42 (85.7%), branches 23/30 (76.7%), functions 3/3 (100%).
Takeoff final: lines 40/42 (95.2%), branches 29/30 (96.7%), functions 3/3 (100%).
Failsafe checkStateAndMode selected lines 445-642 upstream: lines 0/116 (0%), branches 0/218 (0%).
Failsafe selected scope final: lines 116/116 (100%), branches 193/218 (88.5%).
All EXTERNAL1-EXTERNAL8 predicates are covered with exceptions enabled and disabled. Remaining 25 selected-scope branch records are compiler exception/throw instrumentation; no ordinary substantive decision branch remains uncovered.
Takeoff remaining gap: invalid-state defensive default (lines 98-99 and one branch), requiring private-state corruption.
Whole-file Failsafe final: lines 259/431 (60.1%), branches 231/325 (71.1%), functions 13/14 (92.9%). Whole-file values are intentionally different from selected-scope values.
Student execution: FailsafeStructural 13/13 PASS; TakeoffStructural 5/5 PASS.
Full regression: make tests, 149/149 PASS, zero failures, 636.67 seconds. Evidence: Coverage/full-make-tests-final.log and .json. Tests were not rerun for this packaging step.
Production implementation bytes match the exact baseline. Unrelated EKF reference CSV changes are absent.

MC/DC
Q = (A || B) && C && M
01 T F T T -> T -> RTL
02 F F T T -> F -> None
03 F T T T -> T -> RTL
04 F T F T -> F -> Hold
05 F T T F -> F -> None
Pairs: A 01/02; B 02/03; C 03/04; M 03/05.
C uses the observable settled-action distinction RTL versus Hold. Row 04 retains ordinary GCS-loss handling with NAV_DLL_ACT=1; it is not treated as masked.

EVIDENCE AND HTML
Coverage/final-test-traceability.txt maps every actual test to purpose, expectation and coverage.
Coverage/final-coverage-provenance.json records source/build paths, tools, baseline configure/build commands and hashes.
Coverage/upstream-to-student-final-comparison.json records the authentic baseline/final comparison.
Coverage/failsafe-selected-scope-verified-final.json reports the selected 445-642 scope.
Coverage/failsafe-selected-scope-gaps-verified-final.json and failsafe-throw-branch-audit.txt document all 25 throw records.
Coverage/takeoff-selected-gaps-verified-final.json documents the defensive default.
Open Coverage/html/takeoff-final/index.html and Coverage/html/failsafe-final/index.html. These are coverage reports, not frontend tests. Failsafe HTML is whole-file; use the selected-scope JSON for 116/116 and 193/218.
HTML was generated quickly from already verified production .info files. No coverage runs were repeated to make HTML.
Earlier intermediate evidence remains preserved outside this package; it is not used as an upstream baseline. The original folder was backed up before rebuilding.

REPRODUCTION (OPTIONAL; NOT REQUIRED TO REVIEW THE EXISTING VERIFIED RESULTS)
1. Use a clean recursively initialized checkout/worktree at the exact commit above. Keep baseline and student builds separate. Typical setup from an existing PX4 repository:
   git worktree add --detach /tmp/px4-sqe-upstream-d6f12ad d6f12ad1c4f70ad3230afd7d86e971421e02fef4
   git -C /tmp/px4-sqe-upstream-d6f12ad submodule update --init --recursive
   (If that path already exists, inspect and reuse the existing clean worktree.)
2. Validate the patch on the clean baseline before applying:
   git -C /path/to/clean-checkout apply --check /path/to/submission/Patch/SQE_Assignment_2_PX4_Structural_Tests.patch
   For a separate student checkout, apply the same patch with git apply. Do not apply it to the coverage-baseline checkout.
3. Configure/build the clean upstream baseline using the recorded Coverage/final-coverage-provenance.json commands (adjust source/build/dependency paths to the local environment). The only baseline targets are unit-Takeoff and functional-failsafe_test. The upstream Failsafe tests use a test-specific implementation; initial LCOV records retain real unexecuted production code at zero coverage.
4. Student targeted commands from the patched repository:
   make tests TESTFILTER='unit-TakeoffStructural'
   make tests TESTFILTER='functional-FailsafeStructural'
   Full regression: make tests
5. To reproduce isolated coverage, copy the Coverage helper files into the patched repository's coverage/ directory; inspect/update root, baseline and build variables in reproduce-upstream-and-student-coverage.sh. Prepare the clean Coverage baseline build first, then run:
   bash coverage/reproduce-upstream-and-student-coverage.sh
   This optional script resets counters and runs targeted tests; it was not rerun during documentation finalization.
6. Recompute metrics from existing evidence without executing tests:
   python3 Coverage/failsafe_scope_metrics.py --input Coverage/failsafe-production-student-verified-final.info --output /tmp/failsafe-metrics.json
   python3 Coverage/failsafe_scope_metrics.py --input Coverage/takeoff-production-student-verified-final.info --output /tmp/takeoff-metrics.json
7. Recreate HTML from verified production .info files with LCOV/genhtml (run where referenced source paths are available):
   genhtml --branch-coverage --output-directory Coverage/html/takeoff-final Coverage/takeoff-production-student-verified-final.info
   genhtml --branch-coverage --output-directory Coverage/html/failsafe-final Coverage/failsafe-production-student-verified-final.info

ZIP NAMING
Three roll numbers are known from the report cover. Section and the final required naming convention must be confirmed before creating the final ZIP. The complete directory is ready; no technical testing issue remains unresolved.
