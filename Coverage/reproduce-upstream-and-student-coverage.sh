#!/usr/bin/env bash
set -euo pipefail
root=/home/hamzaahmad/PX4-Autopilot
baseline=/tmp/px4-sqe-upstream-d6f12ad
baseline_build=/tmp/px4-sqe-upstream-build
student_build="$root/build/px4_sitl_test"
cd "$root"
out="$root/coverage"

# A separate exact-commit build; only upstream test executables run here.
test "$(git -C "$baseline" rev-parse HEAD)" = d6f12ad1c4f70ad3230afd7d86e971421e02fef4
test -z "$(git -C "$baseline" status --porcelain)"
test ! -e "$baseline/src/modules/commander/failsafe/FailsafeStructuralTest.cpp"
test ! -e "$baseline/src/modules/mc_pos_control/Takeoff/TakeoffStructuralTest.cpp"
lcov --zerocounters --directory "$baseline_build" > "$out/upstream-baseline-coverage.log" 2>&1
# Include compiled but unexecuted failsafe.cpp, so a real zero baseline is retained.
lcov --capture --initial --branch-coverage --gcov-tool gcov --ignore-errors mismatch \
 --directory "$baseline_build/src/modules/commander/failsafe" \
 --directory "$baseline_build/src/modules/mc_pos_control/Takeoff" \
 --base-directory "$baseline_build" --output-file "$out/upstream-baseline-initial.info" >> "$out/upstream-baseline-coverage.log" 2>&1
ctest --test-dir "$baseline_build" -R '^(functional-failsafe_test|unit-Takeoff)$' \
 --output-on-failure -V > "$out/upstream-baseline-tests.log" 2>&1
lcov --capture --branch-coverage --gcov-tool gcov --ignore-errors mismatch \
 --directory "$baseline_build/src/modules/commander/failsafe" \
 --directory "$baseline_build/src/modules/mc_pos_control/Takeoff" \
 --base-directory "$baseline_build" --output-file "$out/upstream-baseline-runtime.info" >> "$out/upstream-baseline-coverage.log" 2>&1
lcov --add-tracefile "$out/upstream-baseline-initial.info" \
 --add-tracefile "$out/upstream-baseline-runtime.info" --branch-coverage \
 --output-file "$out/upstream-baseline-combined.info" >> "$out/upstream-baseline-coverage.log" 2>&1
lcov --extract "$out/upstream-baseline-combined.info" '*/src/modules/commander/failsafe/failsafe.cpp' \
 --branch-coverage --output-file "$out/failsafe-upstream-baseline.info" >> "$out/upstream-baseline-coverage.log" 2>&1
lcov --extract "$out/upstream-baseline-combined.info" '*/src/modules/mc_pos_control/Takeoff/Takeoff.cpp' \
 --branch-coverage --output-file "$out/takeoff-upstream-baseline.info" >> "$out/upstream-baseline-coverage.log" 2>&1
python3 "$out/failsafe_scope_metrics.py" --input "$out/failsafe-upstream-baseline.info" \
 --output "$out/failsafe-selected-scope-baseline.json" > "$out/failsafe-baseline-metrics.log"
python3 "$out/failsafe_scope_metrics.py" --input "$out/takeoff-upstream-baseline.info" \
 --output "$out/takeoff-upstream-baseline.json" > "$out/takeoff-baseline-metrics.log"

# Separate counter resets isolate each student executable. Previous final artifacts remain.
for label in failsafe takeoff; do
 lcov --zerocounters --directory "$student_build" > "$out/$label-student-verified-coverage.log" 2>&1
 if [ "$label" = failsafe ]; then
  executable=functional-FailsafeStructural
  module=src/modules/commander/failsafe
  source='*/src/modules/commander/failsafe/failsafe.cpp'
  metrics=failsafe-selected-scope-verified-final.json
 else
  executable=unit-TakeoffStructural
  module=src/modules/mc_pos_control/Takeoff
  source='*/src/modules/mc_pos_control/Takeoff/Takeoff.cpp'
  metrics=takeoff-student-verified-final.json
 fi
 "$student_build/$executable" > "$out/$label-student-verified-tests.log" 2>&1
 lcov --capture --branch-coverage --gcov-tool gcov --ignore-errors mismatch \
  --directory "$student_build/$module" --base-directory "$student_build" \
  --output-file "$out/$label-student-verified-final.info" >> "$out/$label-student-verified-coverage.log" 2>&1
 lcov --extract "$out/$label-student-verified-final.info" "$source" --branch-coverage \
  --output-file "$out/$label-production-student-verified-final.info" >> "$out/$label-student-verified-coverage.log" 2>&1
 python3 "$out/failsafe_scope_metrics.py" --input "$out/$label-production-student-verified-final.info" \
  --output "$out/$metrics" > "$out/$label-verified-final-metrics.log"
done
git -C "$baseline" status --porcelain > "$out/upstream-baseline-final-status.txt"
