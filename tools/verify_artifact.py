#!/usr/bin/env python3
"""Validate hashes and saved-record consistency, without evaluating the Boolean cube."""
import argparse,csv,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text())
def require(v,msg):
    if not v:raise ValueError(msg)
def main():
    p=argparse.ArgumentParser();p.add_argument('--fresh-dir',type=Path,help='Additionally compare results from a separately executed reproduction');a=p.parse_args()
    m=read(ROOT/'MANIFEST.json')
    for row in m['files']:
        f=ROOT/row['path'];b=f.read_bytes()
        require(len(b)==row['bytes'] and hashlib.sha256(b).hexdigest()==row['sha256'],'Hash mismatch: '+row['path'])
    identity=read(ROOT/'results/structure/function_identity.json')
    require(identity['N']==2**25 and identity['comparators']==42,'Identity domain/comparator count')
    require(all(identity[k]['count']==0 for k in ['netlist_vs_saved','formula_vs_saved','netlist_vs_formula']),'Identity mismatches')
    raw=read(ROOT/'results/c4/acceptance_raw.json')
    with (ROOT/'results/table_ii.csv').open() as f:
        for r in csv.DictReader(f):
            for label,arm in [('row_wiring_disagreements','identity'),('c4_wiring_disagreements','c4')]:
                v=raw[arm]['d4_union_defect_count'] if r['transform']=='D_union' else raw[arm]['per_transform_disagreement'][r['transform']]
                require(int(r[label])==v,'Table II row mismatch')
            require(int(r['boolean_inputs'])==raw['N'],'Table II domain mismatch')
    spec=read(ROOT/'provenance/upstream_input.json');net=ROOT/'inputs/mom25_v2.cha'
    if net.exists():require(hashlib.sha256(net.read_bytes()).hexdigest()==spec['sha256'],'Input mismatch')
    if a.fresh_dir:
        for fn in ['function_identity.json','first_order.json','second_order.json','third_order.json']:
            require(read(a.fresh_dir/fn)==read(ROOT/'results/structure'/fn),'Fresh result differs: '+fn)
        require(read(a.fresh_dir/'c4_raw.json')==raw,'Fresh C4 result differs')
        require(read(a.fresh_dir/'reflection_counterexample.json')==read(ROOT/'results/c4/reflection_counterexample.json'),'Fresh counterexample differs')
    print(json.dumps({'status':'PASS','payload_files_verified':len(m['files']),'input_present_and_verified':net.exists(),'saved_identity_mismatches':0,'table_ii_matches_raw_record':True,'fresh_results_compared':bool(a.fresh_dir),'scientific_evaluator_invoked_by_this_tool':False},indent=2))
if __name__=='__main__':main()
