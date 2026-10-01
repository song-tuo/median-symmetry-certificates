#!/bin/sh
# Optional future reproduction. Not executed during preparation of v1.0.0.
set -eu
artifact_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$artifact_root"
test -f inputs/mom25_v2.cha || { echo 'Run python3 tools/fetch_input.py first.' >&2; exit 1; }
test ! -e outputs || { echo 'outputs/ exists; preserve it and use a fresh checkout.' >&2; exit 1; }
test ! -e logs || { echo 'logs/ exists; preserve it and use a fresh checkout.' >&2; exit 1; }
python3 tools/verify_artifact.py
mkdir outputs logs build
artifact_cxx=${CXX:-c++}
python3 src/record.py --tag build_identity --timeout 180 -- "$artifact_cxx" -std=c++17 -O3 -Wall -Wextra -pedantic src/function_structure.cpp -o build/function_structure
python3 src/record.py --tag build_c4 --timeout 180 -- "$artifact_cxx" -std=c++17 -O3 -Wall -Wextra -pedantic checks/c4/src/c4_acceptance.cpp -o build/c4_acceptance
python3 src/record.py --tag structure_unit --timeout 180 -- build/function_structure --self-test
python3 src/record.py --tag c4_unit --timeout 180 -- build/c4_acceptance --self-test
python3 src/record.py --tag identity --science --timeout 180 -- build/function_structure identity inputs/mom25_v2.cha data/mom25_v2.bin outputs/function_identity.json
python3 src/record.py --tag cofactors --science --timeout 180 -- build/function_structure structure data/mom25_v2.bin outputs
python3 src/record.py --tag posthoc --science --timeout 180 -- build/function_structure posthoc inputs/mom25_v2.cha outputs/recovered_blocks.csv outputs/netlist_posthoc.json
python3 src/record.py --tag c4 --science --timeout 180 -- build/c4_acceptance inputs/mom25_v2.cha data/mom25_v2.bin '12,13,17,11,7,14,18,24,23,19,22,16,20,15,21,10,6,0,1,5,2,8,4,9,3' outputs/c4_raw.json outputs/reflection_counterexample.json
python3 tools/verify_artifact.py --fresh-dir outputs
