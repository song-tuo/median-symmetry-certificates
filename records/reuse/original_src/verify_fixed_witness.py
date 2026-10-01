"""Exact set and wire checks for the single frozen witness; no data evaluation."""
from pathlib import Path
from itertools import combinations
import hashlib, json, traceback

D = Path(__file__).resolve().parents[1]
R = D.parents[1]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

def rot(p):
    x,y = p
    return (-y,x)

def shift(points, d):
    return {(x+d[0],y+d[1]) for x,y in points}

def translation(a,b):
    # Test candidate offsets by anchoring a point, not by shape normalization.
    anchor = min(a)
    for q in sorted(b):
        d = (q[0]-anchor[0],q[1]-anchor[1])
        if shift(a,d) == b:
            return d
    return None

def main():
    cfg = json.loads((D/'protocol.json').read_text())
    ordered = [[tuple(p) for p in cfg['frozen_blocks'][name]] for name in ['C0','C1']]
    for _ in range(3): ordered.append([rot(p) for p in ordered[-1]])
    blocks = [set(b) for b in ordered]
    grid = {(x,y) for x in range(-2,3) for y in range(-2,3)}
    assert all(len(b)==5 for b in blocks)
    assert all(not blocks[i]&blocks[j] for i,j in combinations(range(5),2))
    assert set.union(*blocks)==grid
    action = [next(j for j,b in enumerate(blocks) if {rot(p) for p in a}==b) for a in blocks]
    assert action == [0,2,3,4,1]
    assert blocks[3] == shift(blocks[1],(-2,0))
    assert blocks[4] == shift(blocks[2],(0,-2))
    offsets = {f'{i}->{j}': translation(blocks[i],blocks[j]) for i in range(5) for j in range(5)}
    classes=[];unseen=set(range(5))
    while unseen:
        i=min(unseen);group=sorted(j for j in unseen if translation(blocks[i],blocks[j]) is not None)
        classes.append(group);unseen.difference_update(group)
    assert classes==[[0],[1,3],[2,4]]
    # An exact symbolic schedule check compares absolute sample sets, not images.
    w=(11,13)
    assert shift(blocks[3],w)==shift(blocks[1],(w[0]-2,w[1]))
    assert shift(blocks[4],w)==shift(blocks[2],(w[0],w[1]-2))
    pmap=[(x+2)+5*(y+2) for block in ordered for x,y in block]
    assert sorted(pmap)==list(range(25))
    inverse={q:i for i,q in enumerate(pmap)}
    induced=[]
    for turns in range(4):
        q=[]
        for block in ordered:
            for p in block:
                for _ in range(turns): p=rot(p)
                q.append(inverse[(p[0]+2)+5*(p[1]+2)])
        assert sorted(q)==list(range(25))
        images=[]
        for j in range(5):
            image={q[i]//5 for i in range(5*j,5*j+5)}
            assert len(image)==1
            images.append(next(iter(image)))
        assert sorted(images)==list(range(5))
        induced.append({'quarter_turns':turns,'pin_permutation':q,'module_permutation':images})
    net=R/'inputs/netlists/mom25_v2.cha'
    prior=R/'runs/r4_final_clarity_20261001T063709Z/netlist_operation_count_audit.json'
    static=json.loads(prior.read_text())
    assert sha(net)==static['source_sha256']
    assert static['module_count']==6 and static['comparators_per_module']==7
    assert static['all_six_modules_have_identical_normalized_connectivity']
    identity=R/'runs/r4_public_release_20261001053206Z/staging/median-symmetry-certificates/results/structure/function_identity.json'
    ident=json.loads(identity.read_text())
    assert ident['status']=='PASS' and ident['input_bits_compared']==2**25
    assert all(ident[k]['count']==0 for k in ['netlist_vs_saved','formula_vs_saved','netlist_vs_formula'])
    out={'status':'PASS','scope':'Exact set geometry and wire-permutation checks; historical function identity consumed, not rerun',
         'ordered_blocks':ordered,'core_pin_to_spatial_index':pmap,'quarter_turn_block_action':action,
         'translation_classes':classes,'translation_offsets':offsets,'induced_pin_actions':induced,
         'schedule':{'m3(w)':'m1(w-(2,0))','m4(w)':'m2(w-(0,2))','new_inner_medians_per_output':3,'outer_medians_per_output':1,'comparator_evaluations_per_output':28,'scan':'x and y increasing; use a 2-position delay of m1 and a 2-row delay of m2 after alignment. Reverse vertical scan swaps the computed/cached quarter-turn pair.'},
         'strict_C4_basis':'Every conjugated quarter turn is a module automorphism; combine with existing netlist=F5 identity and threshold lifting.',
         'input_hashes':{str(p):sha(p) for p in [D/'protocol.json',net,prior,identity]},
         'full_cube_executions':0,'images_evaluated':0,'reflection_defects_evaluated':0}
    (D/'results/witness.json').write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({'status':out['status'],'translation_classes':classes,'operation_count':28,'pin_map':pmap}))

if __name__=='__main__':
    try: main()
    except BaseException:
        (D/'results/witness_failure.txt').write_text(traceback.format_exc())
        raise
