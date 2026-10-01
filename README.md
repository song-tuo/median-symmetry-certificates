# Spatial symmetry certificates for hierarchical median networks

Frozen research artifact **v1.1.0** for *Rewiring Hierarchical Median Networks: Spatial Symmetry on Odd Square Windows*, Song Dong.

The artifact makes the complete Boolean certificates and their implementations inspectable. The full-cube results were produced on 2026-09-29; the separate reuse witness and geometric-lemma checks were completed on 2026-10-01. Preparing this release performs file/hash and saved-record checks only; it does not rerun the scientific experiments or claim a freshly validated portable execution.

## Start with the evidence

| Paper result | Public file |
|---|---|
| The 42-comparator public core, an independently built five-by-five majority composition, and the saved truth table agree on all 33,554,432 inputs | [Function identity](results/structure/function_identity.json) |
| Complete one-pin and two-pin cofactor signatures | [All signatures](results/structure/all_first_second_order_signatures.json) |
| All 2,300 triple counts | [CSV](results/structure/all_2300_third_order_counts.csv) |
| Five recovered modules | [Recovered blocks](results/structure/recovered_blocks.json) |
| Table II: seven transforms and the union defect | [Raw machine result](results/c4/acceptance_raw.json), [readable CSV](results/table_ii.csv) |
| First reflection counterexample, index 6335 | [Counterexample](results/c4/reflection_counterexample.json) |
| Complete 2^25 truth table | [Binary table](data/mom25_v2.bin) |
| Fixed pin mapping and group convention | [Wiring](data/c4_wiring.json) |
| Whole-block reuse: model, 28-evaluation lower bound and attaining witness | [Proof](proofs/whole_block_reuse.md), [witness](results/reuse/witness.json) |
| Independent five-point geometric-lemma check | [Result](results/reuse/geometric_lemma.json), [portable checker](checks/reuse/src/check_geometric_lemma.py) |
| Finite D4 block-system enumeration | [Saved result](results/structure/enumerator_result.json), [enumerator](src/independent_group_enumerator.py) |

The general odd-window classification is a symbolic theorem in the paper. A finite program is not represented as a proof for every odd window size. The exact-copy manifest binds the scientific source and result bytes to the frozen originals. Historical command receipts include exit status and resource measurements; local machine paths are explicitly redacted and those derivatives are not labeled byte-identical raw receipts.

## Inspect and verify without rerunning science

Download the attached `median-symmetry-certificates-v1.1.0.zip` from the [release](https://github.com/song-tuo/median-symmetry-certificates/releases/tag/v1.1.0), or check out the `v1.1.0` tag. Then run:

```sh
python3 tools/verify_artifact.py
```

This verifies every payload hash, the recorded zero-mismatch identity, and Table II's agreement with its saved raw record, and consistency of the saved reuse records. It does not recalculate the Boolean function. SHA256SUMS.txt and MANIFEST.json define the frozen payload; the release notes identify the exact Git commit and attached archive hash.

## Obtain the exact upstream comparator netlist

The third-party AxMED netlist is not mirrored here because an explicit redistribution license was not found in the inspected upstream metadata. Its fixed revision, URL, expected 699-byte size, SHA-256 and DOI are in [upstream_input.json](provenance/upstream_input.json).

```sh
python3 tools/fetch_input.py
```

On a machine that requires an explicit existing trusted CA bundle, pass `--ca-file /path/to/trusted-ca.pem`. HTTPS certificate and hostname validation remain enabled. The script performs one request, follows only HTTPS redirects, checks the complete hash and refuses to replace mismatching existing input. It does not use a GitHub account or token. The frozen full truth and certificates remain directly available in this release.

## Optional complete scientific reproduction

Use a fresh checkout with Python 3, a C++17 Clang/GCC-compatible compiler and Unix `wait4`/`/bin/ps`. No third-party Python package is required. The original environment is recorded in [records/environment.json](records/environment.json). Compiler and runtime paths may differ on another machine.

After obtaining the netlist, run explicitly:

```sh
sh tools/reproduce.sh
```

The script builds the unchanged implementations, runs their original unit checks, then performs identity, all low-/third-order cofactors, post-hoc module comparison, and the two fixed-wiring evaluations. Fresh outputs are compared with the saved records. It stops on the first failure and refuses to overwrite an existing output or log directory. It does not launch an optimizer, new mapping search, image experiment, or hardware flow.

The original supervisor retains 180 seconds per scientific command, 1,800 seconds cumulative scientific time and 4 GiB direct-child sampled RSS (about 0.2-second intervals, plus `wait4` peak). This is not a hard address-space or process-tree limit. No scientific child subprocesses are allowed. A budget stop, resource-monitor failure, missing compiler or platform incompatibility is a failed reproduction, not permission to relax limits. This wrapper was statically checked at release preparation; the full sequence was not rerun in this release round.

## Whole-block reuse evidence added in v1.1.0

The independent proof and saved checks establish the following operation counts per steady-state interior output with the existing seven-comparator median primitive:

| Evaluation | Fresh inner medians | Fresh outer medians | Comparator evaluations |
|---|---:|---:|---:|
| Row wiring, whole-block reuse | 1 | 1 | 14 |
| Alternative C4 wiring, whole-block reuse | 3 | 1 | 28 |
| Direct single-window evaluation | 5 | 1 | 42 |

The 28 lower bound applies to complete-result translation reuse with that primitive and one fresh outer median. It is attained by the new witness. Memory, control, borders, partial-comparison sharing and physical implementation are outside this operation-count model. The new witness is separate from the original Table II wiring: no new reflection count or image-quality result is claimed for it.

The geometry checker originally examined 53,130 five-point subsets, not 1,536 complete partitions, and performed no Boolean-circuit evaluation. The historical protocol records the original checker round, including its no-publication boundary; this release is a later publication round. The source/result derivation map distinguishes byte-identical evidence from path-redacted receipts and portable adapters.

Optional reproduction of these two limited checks (after fetching the pinned netlist) is separate from the full-cube reproduction:

```sh
python3 tools/reproduce_reuse.py --output-dir reproduction-reuse
```

This requires Python 3 and Unix `resource`, allows 30 seconds per child, and checks the completed child's peak RSS against 256 MiB. The RSS check is post-run, not an enforced address-space limit. It refuses existing output directories, stops on failure, and compares scientific fields against the saved results. The adapters change only paths/output handling; release preparation checked syntax and function AST equivalence without rerunning the checkers.

The v1.0.0 tag and assets are preserved. This release extends its evidence while leaving its scientific source, truth table, mappings, and results unchanged.

## Interpretation and provenance

The C4 wiring has zero quarter-turn disagreement, while its reflection count remains positive. The row and C4 wirings retain different four-element subgroups. Neither the table nor this release establishes unrestricted minimum defect, a natural-image quality gain, or a streaming hardware cost advantage. Pin-to-position direction, active action, coordinate index and bit packing are explicitly recorded; bit `x % 8` of byte `x / 8` stores the truth value for input `x`, with pin `i` equal to `(x >> i) & 1`.

Upstream: Mrazek and Vasicek, AxMED, ISCAS 2025, [DOI](https://doi.org/10.1109/ISCAS56072.2025.11043775), fixed commit `bea7946a0653d0f6ca0612aa9cdc04593597aeda`. The evaluator and identity/cofactor implementation in this repository were independently written for this study; the C4 adapter reuses this study's bitset evaluator. It is not a second independent evaluator.

See [RIGHTS.md](RIGHTS.md), [claim-to-evidence map](provenance/claim_evidence.json), [exact-copy manifest](provenance/EXACT_COPY_MANIFEST.json), [redaction map](records/REDACTION_MAP.json), and [CITATION.cff](CITATION.cff). No publication decision or novelty certificate is implied.
