#!/usr/bin/env bash
# Runs every tools/tests/test_*.sh; exits non-zero if any fails.
set -u
cd "$(dirname "$0")/../.." || exit 1
status=0
for t in tools/tests/test_*.sh; do
  echo "== $t"
  if ! bash "$t"; then status=1; fi
done
if [[ $status -eq 0 ]]; then echo "ALL TESTS PASSED"; else echo "SOME TESTS FAILED"; fi
exit $status
