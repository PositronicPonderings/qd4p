#!/bin/sh
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  QD4P host tests: the library on a PC, no Pico needed.
#
#  Needs: a C compiler (cc / gcc / clang) and python3
#  Run:   sh tests/host/run_tests.sh      (from anywhere)
#
#  Every test_*.c here is built against the library and run; each prints
#  its own checks and exits non-zero on a failure. Library files that need
#  the Pico SDK (*_pico.c, the hardware entropy sources) are left out;
#  qd_entropy_host.c stands in for them.
# ---------------------------------------------------------------------------
cd "$(dirname "$0")" || exit 1
L=../../qd4p; B=build
mkdir -p $B
CC=${CC:-cc}
CF="-std=c11 -O2 -Wall -Wextra -Werror -I$L -I. -I$B"
LIB=$(ls $L/*.c | grep -v '_pico\.c$')
pass=0; fail=0

# The roll engine's goldens are written fresh from the Python reference
# every run, so the C engine and the reference are always compared.
python3 rng_reference.py --c-goldens > $B/roll_goldens.h || { echo "FAIL  rng_reference.py"; exit 1; }
python3 fair_reference.py --c-goldens > $B/fair_goldens.h || { echo "FAIL  fair_reference.py"; exit 1; }
# ... and the random-table test asset from its text, by the PC tool itself.
python3 ../../examples/tables/tables.py build tables_test.txt --c $B/tables_test.h --name tables_test > /dev/null 2>&1 \
    || { echo "FAIL  examples/tables/tables.py"; exit 1; }

for t in test_*.c; do
    name=${t%.c}
    # a test can ask for an example's source too: a line " * also: path"
    ALSO=$(sed -n 's/^ \* also: //p' $t)
    INC=""; for a in $ALSO; do INC="$INC -I$(dirname $a)"; done
    if ! $CC $CF $INC -o $B/$name $t $ALSO $LIB -lm 2> $B/$name.build.log; then
        echo "BUILD FAIL  $name (see tests/host/$B/$name.build.log)"
        fail=$((fail+1)); continue
    fi
    if ./$B/$name > $B/$name.log 2>&1; then
        echo "PASS  $(tail -1 $B/$name.log)"; pass=$((pass+1))
    else
        echo "FAIL  $name:"; sed 's/^/      /' $B/$name.log; fail=$((fail+1))
    fi
done

if python3 test_tables_py.py > $B/tables_py.log 2>&1; then
    echo "PASS  $(tail -1 $B/tables_py.log)"; pass=$((pass+1))
else
    echo "FAIL  table tool:"; sed 's/^/      /' $B/tables_py.log; fail=$((fail+1))
fi

if python3 test_fair_pc.py > $B/fair_pc.log 2>&1; then
    echo "PASS  $(tail -1 $B/fair_pc.log)"; pass=$((pass+1))
else
    echo "FAIL  fair mode on the PC console:"; sed 's/^/      /' $B/fair_pc.log; fail=$((fail+1))
fi

# The manual: every example compiles, runs and prints what the page shows,
# and every roll in the notation guide does what the guide says.
if python3 doc_examples.py > $B/doc_examples.log 2>&1; then
    echo "PASS  $(tail -1 $B/doc_examples.log)"; pass=$((pass+1))
else
    echo "FAIL  manual examples:"; sed 's/^/      /' $B/doc_examples.log; fail=$((fail+1))
fi
if python3 notation_examples.py > $B/notation_examples.log 2>&1; then
    echo "PASS  notation guide: $(tail -1 $B/notation_examples.log)"; pass=$((pass+1))
else
    echo "FAIL  notation guide:"; sed 's/^/      /' $B/notation_examples.log; fail=$((fail+1))
fi

if python3 check_disclosure.py > $B/disclosure.log 2>&1; then
    echo "PASS  $(head -1 $B/disclosure.log)"; pass=$((pass+1))
else
    echo "FAIL  disclosure:"; sed 's/^/      /' $B/disclosure.log; fail=$((fail+1))
fi

echo
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
