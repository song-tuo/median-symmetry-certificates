"""Independent exhaustive D4 homomorphism/equivariant-coloring enumerator.
No netlist/truth/block-recovery imports. Generic presentation and stabilizers only.
"""
import argparse
from collections import Counter
import itertools
import json
import math
from pathlib import Path

def compose(p,q): return tuple(p[q[i]] for i in range(len(p)))
def power(p,k):
    ans=tuple(range(len(p)))
    for _ in range(k): ans=compose(p,ans)
    return ans

def spatial_action(side):
    coords=[(x,y) for y in range(side) for x in range(side)]
    idx={p:i for i,p in enumerate(coords)}
    r=tuple(idx[(side-1-y,x)] for x,y in coords)
    s=tuple(idx[(side-1-x,y)] for x,y in coords)
    group=[compose(power(r,k),power(s,e)) for k in range(4) for e in range(2)]
    if side>1: assert len(set(group))==8
    return coords,group

def orbit_partition(group):
    unused=set(range(len(group[0])));ans=[]
    while unused:
        representative=min(unused)
        orbit=sorted({g[representative] for g in group})
        assert set(orbit)<=unused
        ans.append(orbit);unused.difference_update(orbit)
    return ans

def homomorphisms(colors):
    identity=tuple(range(colors))
    permutations=list(itertools.permutations(range(colors)))
    for r in permutations:
        if power(r,4)!=identity:continue
        for s in permutations:
            if power(s,2)!=identity:continue
            if compose(compose(s,r),s)!=power(r,3):continue
            phi=[compose(power(r,k),power(s,e)) for k in range(4) for e in range(2)]
            yield r,s,phi

def orbit_colorings(orbit,group,phi,colors):
    representative=orbit[0]
    stabilizer=[a for a,g in enumerate(group) if g[representative]==representative]
    possibilities=[]
    for color in range(colors):
        if not all(phi[a][color]==color for a in stabilizer):continue
        coloring={}
        for a,g in enumerate(group):
            vertex=g[representative];value=phi[a][color]
            if vertex in coloring: assert coloring[vertex]==value
            coloring[vertex]=value
        assert sorted(coloring)==orbit
        # Independently verify all eight group actions at every point in orbit.
        assert all(coloring[g[v]]==phi[a][coloring[v]]
                   for a,g in enumerate(group) for v in orbit)
        count=[0]*colors
        for v in orbit:count[coloring[v]]+=1
        possibilities.append({'representative_color':color,
                              'colors_in_point_order':[coloring[v] for v in orbit],
                              'counts':count})
    return {'points':orbit,'stabilizer_group_indices':stabilizer,'choices':possibilities}

def count_balanced(orbit_data,capacity,colors,retain_witnesses=False):
    # Enumerates all choices through exact multiplicity DP, not a fixed-point
    # obstruction rule. States exceeding capacity cannot become balanced.
    start=(0,)*colors
    states=Counter({start:1})
    witnesses={start:[]}
    layer_counts=[1]
    for od in orbit_data:
        nxt=Counter();nw={}
        for state,multiplicity in states.items():
            for choice_id,choice in enumerate(od['choices']):
                target=tuple(state[i]+choice['counts'][i] for i in range(colors))
                if any(x>capacity for x in target):continue
                nxt[target]+=multiplicity
                if retain_witnesses and target not in nw:nw[target]=witnesses[state]+[choice_id]
        states=nxt;witnesses=nw
        layer_counts.append(len(states))
    target=(capacity,)*colors
    return states[target],witnesses.get(target),layer_counts

def enumerate_problem(side,colors,capacity,details=True):
    assert side*side==colors*capacity
    coords,group=spatial_action(side)
    # Verify abstract multiplication, not merely the generator relations.
    # (r^k s^e)(r^l s^f)=r^(k+(-1)^e*l) s^(e+f).
    for k,e,l,f in itertools.product(range(4),range(2),range(4),range(2)):
        target=2*((k+(-1 if e else 1)*l)%4)+(e+f)%2
        assert compose(group[2*k+e],group[2*l+f])==group[target]
    orbits=orbit_partition(group)
    records=[];hom_count=0;total_balanced=0;total_unrestricted=0;example=None
    for r,s,phi in homomorphisms(colors):
        hom_count+=1
        for k,e,l,f in itertools.product(range(4),range(2),range(4),range(2)):
            target=2*((k+(-1 if e else 1)*l)%4)+(e+f)%2
            assert compose(phi[2*k+e],phi[2*l+f])==phi[target]
        od=[orbit_colorings(o,group,phi,colors) for o in orbits]
        unrestricted=math.prod(len(o['choices']) for o in od)
        balanced,witness,layers=count_balanced(od,capacity,colors,True)
        total_unrestricted+=unrestricted;total_balanced+=balanced
        if balanced and example is None:
            coloring=[None]*(side*side)
            for o,cid in zip(od,witness):
                for v,c in zip(o['points'],o['choices'][cid]['colors_in_point_order']):coloring[v]=c
            assert Counter(coloring)==Counter({i:capacity for i in range(colors)})
            assert all(coloring[g[v]]==phi[a][coloring[v]]
                       for a,g in enumerate(group) for v in range(side*side))
            example={'r':r,'s':s,'coloring':coloring}
        if details:
            records.append({'homomorphism_index':hom_count-1,'r_image':r,'s_image':s,
                            'orbits':od,'all_equivariant_colorings':unrestricted,
                            'balanced_colorings':balanced,'dp_layer_state_counts':layers})
    return {'schema':'r4-independent-d4-coloring-v1','side':side,'colors':colors,
            'capacity_per_color':capacity,'group_convention':'index=2*k+e; active r^k s^e; r(x,y)=(side-1-y,x), s(x,y)=(side-1-x,y)',
            'grid_coordinates':coords,'grid_group_permutations':group,
            'point_orbits':orbits,'orbit_sizes':sorted(map(len,orbits)),
            'candidate_generator_pairs_total':math.factorial(colors)**2,
            'homomorphisms_count':hom_count,
            'all_equivariant_colorings_over_homomorphisms':total_unrestricted,
            'balanced_labeled_colorings_count':total_balanced,
            'balanced_unlabeled_partitions_count':total_balanced//math.factorial(colors),
            'first_feasible_example':example,'homomorphisms':records,
            'counting_note':'All labeled homomorphisms, including nonfaithful actions, and all orbit maps are included. DP retains exact multiplicities. A balanced coloring is surjective and uniquely determines the homomorphism; each partition has colors! labelings.',
            'decision':'D4_INVARIANT_BLOCK_SYSTEM_FOUND_STOP' if total_balanced else 'NO_INVARIANT_BALANCED_BLOCK_SYSTEM'}

def self_test():
    # Independent small exhaustive check of output count against all bijective
    # color assignments on a square; no 5x5 formal execution here.
    t=enumerate_problem(2,4,1,False)
    assert t['balanced_labeled_colorings_count']==24
    assert t['balanced_unlabeled_partitions_count']==1
    t1=enumerate_problem(3,1,9,False)
    assert t1['homomorphisms_count']==1 and t1['balanced_labeled_colorings_count']==1
    # DP multiplicities: duplicate count vectors are distinct orbit choices.
    v=[{'choices':[{'counts':[1,0]},{'counts':[1,0]}]},
       {'choices':[{'counts':[0,1]},{'counts':[0,1]},{'counts':[0,1]}]}]
    assert count_balanced(v,1,2)[0]==6
    # Closed orbit maps independently equal brute color assignments on 2x2.
    _,gg=spatial_action(2); oo=orbit_partition(gg)
    for _,_,phi in homomorphisms(2):
        observed=orbit_colorings(oo[0],gg,phi,2)
        brute=[c for c in itertools.product(range(2),repeat=4)
               if all(c[g[x]]==phi[a][c[x]] for a,g in enumerate(gg) for x in range(4))]
        assert len(observed['choices'])==len(brute)
    print(json.dumps({'status':'PASS','fixtures':['2x2 four singleton blocks:24 labelings','3x3 one block:1','DP duplicate multiplicity:6','2x2 all homomorphisms into S2 vs direct colorings'],'formal_5x5_not_run':True}))

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--self-test',action='store_true');ap.add_argument('--output')
    args=ap.parse_args()
    if args.self_test:self_test()
    else:
        assert args.output
        p=Path(args.output);assert not p.exists(),'refuse overwrite'
        result=enumerate_problem(5,5,5)
        assert result['orbit_sizes']==[1,4,4,4,4,8]
        result['formal_complete']=True
        p.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps({k:v for k,v in result.items() if k not in ('homomorphisms','grid_coordinates','grid_group_permutations','point_orbits')}))
        raise SystemExit(0 if result['balanced_labeled_colorings_count']==0 else 23)

