"""Independent exhaustive check of ONE five-point geometric lemma.

No witness import, block-partition construction, circuit evaluation, or objective
ranking. Search the 53,130 five-point subsets solely for an obstruction to the
lemma: a noncentral block disjoint from its rotations cannot be translation-
equivalent to its quarter turn. Uses normalization, unlike the witness checker.
"""
from pathlib import Path
from itertools import combinations
from math import comb
import json, traceback

D=Path(__file__).resolve().parents[1]

def key(points):
    left=min(x for x,y in points)
    bottom=min(y for x,y in points)
    return tuple(sorted((x-left,y-bottom) for x,y in points))

def main():
    grid=tuple((x,y) for x in range(-2,3) for y in range(-2,3))
    checked=0;matches=[];counterexamples=[]
    for candidate in combinations(grid,5):
        checked+=1
        quarter=tuple((-y,x) for x,y in candidate)
        if key(candidate)!=key(quarter): continue
        S=set(candidate)
        T=S
        overlaps=[]
        for _ in range(3):
            T={(-y,x) for x,y in T}
            overlaps.append(sorted(S&T))
        forbidden=(0,0) in S or any(overlaps)
        record={'set':candidate,'contains_origin':(0,0) in S,'intersections_with_rotations':overlaps,'excluded_as_noncentral_block':forbidden}
        matches.append(record)
        if not forbidden:counterexamples.append(record)
    assert checked==comb(25,5)==53130
    out={'status':'PASS' if not counterexamples else 'GEOMETRIC_LEMMA_FAILED',
         'five_point_subsets_checked':checked,'quarter_turn_translation_equivalent_sets':len(matches),
         'matches_containing_origin':sum(m['contains_origin'] for m in matches),
         'matches_without_origin_but_overlapping_a_rotation':sum(not m['contains_origin'] and m['excluded_as_noncentral_block'] for m in matches),
         'eligible_counterexample_count':len(counterexamples),'counterexamples':counterexamples,
         'all_matching_sets':matches,'partitions_generated':0,'function_evaluations':0,
         'scope':'Finite check of the geometric lemma only; the analytical proof relates that lemma to the whole-block-reuse lower bound.'}
    (D/'results/geometric_lemma.json').write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({k:v for k,v in out.items() if k not in ['all_matching_sets','counterexamples']}))
    assert not counterexamples

if __name__=='__main__':
    try:main()
    except BaseException:
        (D/'results/geometric_lemma_failure.txt').write_text(traceback.format_exc())
        raise
