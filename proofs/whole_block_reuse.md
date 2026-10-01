# Minimum work under complete-median translation reuse

## Model

The spatial domain is the integer lattice, with a translated 5×5 window at every output location. Fix a partition of the 25 sample positions into five size-five inner-median blocks, followed by an outer median of their five results. A complete inner-median result can be reused only when the sample-position sets coincide after shifting the output location. Each newly evaluated inner median uses the supplied seven-comparator routine. The outer median is evaluated afresh using that same routine once per output. Partial comparisons, value-dependent shortcuts and reuse of outer outputs are outside this model. Count interior outputs in steady state, excluding initialization, boundaries, memory, control and routing.

Two different five-element sample sets cannot define the same median function on all inputs: choose a variable in their symmetric difference; the median of the set containing it depends essentially on that variable, while the other median does not. Thus exact reuse of a complete result is characterized by identical sample sets, and at the level of relative blocks by translation equivalence.

Let k be the number of translation-equivalence classes of the inner blocks. Computing one median-result field for each class and using spatial shifts supplies all five inner results, so the model cost is 7(k+1). Each distinct class requires its own result field; over an arbitrarily large interior region its density is one evaluation per output. Finite-image startup and edge effects need a separate schedule analysis. In this model, minimize k rather than treating physical hardware cost as proportional to the operation count.

## Structure of every C4-compatible partition

Let r(x,y)=(-y,x) act on X={-2,-1,0,1,2}². The origin is fixed and the other points form six free orbits of size four. The block containing the origin is fixed setwise and must consist of the origin and one whole free orbit.

The other four blocks cannot form a block orbit of size 1 or 2. Each free point orbit mapping into a block orbit of size q contributes 4/q points to each of its blocks, which is even for q=1 or 2, whereas each block has odd size five. Hence those four blocks form one orbit C1,rC1,r²C1,r³C1. Each of the remaining five free point orbits contributes exactly one point to each noncentral block. In particular, each noncentral block excludes the origin and is disjoint from its three rotations.

## Geometric lemma

No noncentral block in such a partition is translation-equivalent to its quarter turn.

Suppose rS=S+t for a size-five set S. The affine isometry h(x)=rx-t permutes S and has order four about its unique center c. Every h-orbit other than {c} has size four. Since |S|=5, c belongs to S and is an integer grid point. Therefore S=c+{0,v,rv,-v,-rv}, with nonzero integer v.

Put d=max(|v_x|,|v_y|). Both coordinate extrema of S are c_j±d. Containment in X forces d=1 or 2 and |c_j|≤2-d. If d=2, c=(0,0), excluded from a noncentral block. If d=1 and c=0 the same exclusion applies. The remaining centers have axial type (±1,0),(0,±1), or diagonal type (±1,±1). Up to grid symmetries, v has axial type (1,0) or diagonal type (1,1):

| Center type | v type | Violation |
|---|---|---|
| Axial | Axial | S contains the origin. |
| Diagonal | Diagonal | S contains the origin. |
| Diagonal | Axial | For c=(1,1), S contains (1,0) and (0,1), from the same origin rotation orbit. |
| Axial | Diagonal | For c=(1,0), S contains (1,0), (0,1), and (0,-1), from the same origin rotation orbit. |

Each case contradicts the noncentral block structure. The proof is geometric; it does not rely on an enumerated partition count or program output.

## At least three classes

The center block is quarter-turn invariant about the origin. Any translate of it would be translation-equivalent to its own quarter turn, so by the lemma it cannot be a noncentral block. The center block is therefore in a separate class.

Two adjacent members of C1,rC1,r²C1,r³C1 cannot be translation-equivalent by the same lemma (rotate the purported relation back to S versus rS). Thus these four blocks occupy at least two further classes. Therefore k≥3 and the model cost is at least 7(3+1)=28.

## Fixed construction attaining the bound

C0={(0,0),(1,1),(-1,1),(-1,-1),(1,-1)}.

C1={(1,0),(1,2),(1,-2),(2,2),(0,-2)}, with Cj=r^(j-1)C1 for j=2,3,4.

These five sets partition X; r fixes C0 and cycles C1 through C4. C1 is centrally symmetric about (1,0), giving C3=C1-(2,0). Rotating that identity gives C4=C2-(0,2). The three translation classes are {C0}, {C1,C3}, {C2,C4}.

Let m_j(w)=med{I(w+u):u∈Cj}. Then m3(w)=m1(w-(2,0)) and m4(w)=m2(w-(0,2)). A scan increasing x and y can evaluate m0,m1,m2 and retrieve the other two from past positions, with spatial alignment and startup handled by the implementation. A reversed vertical scan exchanges which of C2/C4 is evaluated and which is retrieved. The outer seven-comparator median combines the five results, giving 28 evaluations per steady-state output.

The existing netlist-to-F5 identity and the proven module automorphism group transfer C4 invariance to this wiring. Threshold commutation transfers it to scalar median inputs. This argument uses the historical identity record; no new full-cube function evaluation is required or claimed.

## Independent finite check

The separate checker tests all 53,130 five-point subsets for translation equivalence with a quarter turn by coordinate normalization. It examines only the matching sets for origin membership or overlap with a rotation. It imports neither the witness checker nor a manual case table, and does not construct complete partitions, measure reflection defect or rank candidate mappings. Its role is to cross-check the geometric lemma, not replace the above proof.
