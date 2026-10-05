# Harmonic concepts and persistence

A fixed harmonic incidence relation determines formal concepts and two
filtrations measuring shared pitches and shared slices. Their persistent
homology describes how overlap patterns survive as the required number of
common witnesses changes. This page defines those objects, their duality, and
a barcode convention with a worked example.

Use the definitions in [Harmonic incidence and Boolean geometry](harmonic-incidence.md):
$`T`$ is the set of slices modulo a fixed lens, $`\Sigma`$ is the finite set of
pitch labels, and $`tR\sigma`$ means that $`\sigma`$ is available at $`t`$.
For $`S\subseteq T`$ and $`C\subseteq\Sigma`$,

```math
S^\uparrow=\{\sigma:\forall s\in S,\ sR\sigma\},
\qquad
C^\downarrow=\{t:\forall\sigma\in C,\ tR\sigma\}.
```

These are mathematical analysis definitions, not a claim that the engine
currently computes persistence. The input dimension is $`d`$, the witness
threshold is $`k`$, and the homological degree is $`q`$.

## Formal concepts

A formal concept is a pair $`(S,C)`$ such that

```math
S^\uparrow=C,\qquad C^\downarrow=S.
```

Its **extent** $`S`$ contains all slices admitting its pitch collection, and its
**intent** $`C`$ contains all pitches shared by those slices. Equivalently, a
nonempty concept describes a maximal rectangle $`S\times C`$ of incidences.
Concepts are ordered by

```math
(S,C)\leq(S',C')\iff S\subseteq S'\iff C\supseteq C'.
```

Increasing the extent weakens the common pitch requirement. The full concept
poset is a lattice. For its relation to the topology of binary relations and
applications to music, see
[Freund, Andreatta, and Giavitto](https://doi.org/10.1007/s10472-014-9445-3).

For a poset $`Q`$, write $`\Delta Q`$ for its **order complex**: vertices are
elements of $`Q`$, and simplices are nonempty finite chains. Thus a simplex of
the concept order complex is a chain of concepts, not simply a concept.
Closed extents alone need not be closed under taking subsets and generally do
not form a simplicial complex on $`T`$.

## The total weight Dowker filtration

For a nonempty set of slices, define its common-pitch count

```math
w_T(S)=|S^\uparrow|.
```

For each positive integer $`k`$, define

```math
K_k^{M,U}=\{\varnothing\neq S\subseteq T:|S^\uparrow|\geq k\}.
```

Suppress $`M,U`$ when the configuration and lens are fixed. We list only nonempty
simplices throughout; adjoining the empty face is an equivalent convention.
Since $`S'\subseteq S`$ implies $`(S')^\uparrow\supseteq S^\uparrow`$, each $`K_k`$
is a simplicial complex. Its vertices are precisely the points $`t\in T`$ with
at least $`k`$ available pitch labels. Furthermore,

```math
K_{k+1}\subseteq K_k,\qquad K_1=D(R).
```

Here $`D(R)`$ is the ordinary Dowker complex. This decreasing sequence is the
**total-weight Dowker filtration**, indexed by $`\mathbb N^{\mathrm{op}}`$.
It is the common-witness filtration studied by
[Hellmer and Spaliński](https://arxiv.org/abs/2405.15592).

A vertex in $`K_k`$ has a palette of at least $`k`$ pitches. An edge represents two
slices sharing at least $`k`$ pitches. A higher simplex requires the same $`k`$
pitches to be shared by all its vertices. Pairwise intersections alone do not
determine these higher simplices.

The vertices are points of the affine Boolean scheme $`T`$. The simplex rule
adds the labelled harmonic incidence data to that discrete space. Arbitrary
pitch relabelling leaves the complex unchanged, so rational interval heights,
pitch distances, and perceived dissonance are not encoded by this filtration
alone.

## Retraction to concepts

Let $`P(K_k)`$ be the poset of nonempty faces of $`K_k`$, ordered by inclusion.
Its order complex is the barycentric subdivision $`\mathrm{sd}\,K_k`$.
The Galois closure

```math
c:P(K_k)\longrightarrow P(K_k),\qquad c(S)=S^{\uparrow\downarrow}
```

is well-defined because

```math
c(S)^\uparrow=S^\uparrow.
```

It is order-preserving, satisfies $`S\subseteq c(S)`$, and is idempotent. The
closure-operator theorem therefore gives a strong deformation retract, after
geometric realization, of $`\mathrm{sd}\,K_k`$ onto the order complex of its
fixed points. See
[Björner, Corollary 10.12](https://webhomes.maths.ed.ac.uk/~v1ranick/papers/bjorner2.pdf).

Those fixed points are the extents of concepts satisfying $`S\neq\varnothing`$
and $`|C|\geq k`$. The same map $`c`$ restricts to every filtration level, and
the order homotopy from the identity to $`c`$ does too. Consequently the
retractions preserve the persistence module, not just the homotopy type at
each individual level.

Retain exactly these eligible concepts. The entire concept lattice has a
least and a greatest element, so its order complex is contractible. Removing
concepts with empty extent or insufficient intent is essential; one should
not indiscriminately add or delete the lattice extrema.

## The opposite filtration and filtered duality

Interchanging slices and pitches gives another decreasing filtration:

```math
L_k=\{\varnothing\neq C\subseteq\Sigma:|C^\downarrow|\geq k\}.
```

Here a simplex is a pitch collection available together in at least $`k`$
distinct lens classes. Ordinary Dowker duality gives $`K_1\simeq L_1`$, but the
two total-weight filtrations need not have matching homotopy types or barcodes.

For example, let one slice admit two pitches. Then $`K_2`$ is a point, whereas
$`L_2`$ is empty because there is only one slice. This is a valid harmonic
example: take a one-bit input, co-mute it, and assign its two states different
pitches.

The filtered counterpart of $`K_k`$ on the pitch side is instead

```math
D_k=\Delta\{C\in L_1:|C|\geq k\}.
```

The braces describe a poset ordered by inclusion, rather than a complex on
the original pitch vertices. A simplex of $`D_k`$ is a chain

```math
C_0\subsetneq\cdots\subsetneq C_r,\qquad |C_0|\geq k.
```

Thus $`D_k`$ is a subcomplex of $`\mathrm{sd}\,L_1`$. Filtered Dowker duality
gives a compatible equivalence $`K_\bullet\simeq D_\bullet`$: common-witness
count on one side corresponds to face cardinality in the subdivided opposite
complex. See the filtered duality result in
[Hellmer and Spaliński](https://arxiv.org/abs/2405.15592).

For this finite relation, the closures give a direct proof. The face poset of
$`K_k`$ retracts to closed extents with at least $`k`$ common pitches. The poset
defining $`D_k`$ retracts under $`C\mapsto C^{\downarrow\uparrow}`$ to closed
intents of size at least $`k`$ with nonempty extent. Up and down identify these
two fixed-point posets in reverse order, and reversing chains identifies their
order complexes. All these operations respect the threshold inclusions.

## A symmetric bifiltration of concepts

Both common-witness filtrations can be retained in one object. For $`a,b\geq1`$,
take the full subposet

```math
\mathcal C_{a,b}(R)
=\{(S,C)\text{ a concept}:|S|\geq a,\ |C|\geq b\}
```

and define $`B_{a,b}(R)=\Delta\mathcal C_{a,b}(R)`$. Increasing either threshold
removes concepts and gives an inclusion into the earlier order complex. The
closure retractions above yield compatible equivalences

```math
K_b\simeq B_{1,b}(R),\qquad L_a\simeq B_{a,1}(R).
```

Transposing $`R`$ sends a concept $`(S,C)`$ to $`(C,S)`$ and reverses its order.
Consequently there is a simplicial isomorphism, compatible with both
parameters,

```math
B_{a,b}(R)\cong B_{b,a}(R^\top).
```

This construction follows directly from the Galois closures. It packages the
two possibly different barcodes as boundary slices of a symmetric bifiltration.
For a related treatment combining row and column total weights using extended
Dowker duality, see
[Vaupel and Dunn](https://arxiv.org/abs/2310.11529).

## Barcode conventions

Choose a coefficient field $`\Bbbk`$, for example $`\mathbb F_2`$. This choice is
independent of the Boolean coefficient field used to describe availability.
The inclusions give a persistence module

```math
\cdots\longrightarrow H_q(K_{k+1};\Bbbk)
\longrightarrow H_q(K_k;\Bbbk)\longrightarrow\cdots
\longrightarrow H_q(K_1;\Bbbk).
```

All spaces are finite. Reversing the threshold index makes this an ordinary
finite increasing filtration, whose persistence module decomposes into
intervals. The intervals, with multiplicity, form its degree-$`q`$ barcode.

For an explicit indexing convention, put $`N=|\Sigma|`$ and

```math
\mathcal K_r=K_{N+1-r},\qquad r=0,1,\ldots,N.
```

Then $`\mathcal K_0=K_{N+1}`$ is empty and $`\mathcal K_N=K_1`$ is the full
Dowker complex. A nonempty simplex $`S\in K_1`$ enters at
$`r=N+1-|S^\uparrow|`$. Its faces enter no later. Extend the filtration
constantly for $`r>N`$ when using $`\infty`$ as the death endpoint for classes
surviving the final threshold.

In this convention:

- Degree zero records connected components of the slice-overlap complex.
- Degree one records cycles of overlaps that are not filled by its higher
  simplices. A cycle representative need not be unique.
- Higher degrees record higher-dimensional homological features.

A bar records the survival of a homology class through inclusion maps. It is
not itself a single pitch, a particular formal concept, or a unique harmonic
explanation. Betti numbers at separate thresholds omit these maps and do not
by themselves determine the barcode.

## A worked barcode

Restrict the relation to three slices and three pitch labels with the following
availability. The complexes in this example are built from this subrelation:

| Slice | $`\alpha`$ | $`\beta`$ | $`\gamma`$ |
| --- | --- | --- | --- |
| $`t_1`$ | available | available | |
| $`t_2`$ | | available | available |
| $`t_3`$ | available | | available |

Each slice has two pitches. Every pair of slices shares exactly one pitch,
and all three share none. Therefore:

| Threshold | Complex | $`\dim H_0`$ | $`\dim H_1`$ |
| --- | --- | --- | --- |
| $`k\geq3`$ | Empty | 0 | 0 |
| $`k=2`$ | Three isolated vertices | 3 | 0 |
| $`k=1`$ | Triangle boundary with no filled triangle | 1 | 1 |

These are ordinary homology dimensions over $`\mathbb F_2`$. Here $`N=3`$,
so $`\mathcal K_0`$ and $`\mathcal K_1`$ are empty, $`\mathcal K_2`$ consists of
three vertices, and $`\mathcal K_3`$ is the triangle boundary. The barcodes,
written as multisets, are

```math
\begin{aligned}
\mathcal B_0&=\{[2,3),[2,3),[2,\infty)\},\\
\mathcal B_1&=\{[3,\infty)\}.
\end{aligned}
```

Three components appear when the shared-pitch threshold reaches two. They
merge into one when it reaches one, where an unfilled cycle also appears.
Infinite endpoints mean survival through the last threshold under the stated
constant extension; they make no claim about elapsed musical time.

Adding a pitch available at every slice makes $`K_1`$ a full simplex, so its
higher homology vanishes. The higher-threshold complexes may still retain
structure. A Betti number at $`k=1`$ alone can therefore hide variation that the
filtration reveals.

## What to retain in an analysis

For reproducible barcodes, retain the accepted configuration, the lens, the
pitch identity convention, the labelled relation, the coefficient field, and
the threshold direction. Counts on $`T=X/{\sim_U}`$ count lens classes equally;
they do not weight classes by how often playback visits them.

The two natural outputs are a slice barcode from $`K_\bullet`$, counting common
pitches, and a pitch barcode from $`L_\bullet`$, counting common slices. They
are complementary measurements. A matching dual computation uses
$`K_\bullet`$ and $`D_\bullet`$ instead. The concept bifiltration supports both
views, but its two-parameter persistence module generally has no complete
ordinary interval-barcode decomposition. Restricting it to a monotone path
along which the complexes grow gives a one-parameter barcode.

Practical computations can intersect bitsets of available pitch labels to
obtain $`S^\uparrow`$ and its cardinality. The concept retraction provides an
alternative model, although neither the number of concepts nor the number of
simplices is guaranteed to be small. To compute $`H_q`$ correctly, include
$`(q+1)`$-simplices as well: they can fill $`q`$-cycles. Preserve distinct pitch
labels even when their availability columns coincide. When compressing equal
columns to compute common-pitch counts, retain their multiplicities as weights.

Chronological persistence requires a path $`t_0,t_1,\ldots`$ through $`T`$, with
repeated visits retained. If $`P_i=F_{t_i}^M(U)`$, the inclusions

```math
\Bbbk[P_0]\longleftarrow\Bbbk[P_0\cap P_1]
\longrightarrow\Bbbk[P_1]\longleftarrow\cdots
```

form a zigzag, where $`\Bbbk[P_i]`$ is the vector space with basis $`P_i`$.
For these discrete labelled sets, its intervals track uninterrupted runs of
individual pitch availability. This uses a different parameter from the
shared-pitch threshold and does not describe the pitches actually selected by
the voices. See [Carlsson and de Silva on zigzag persistence](https://arxiv.org/abs/0812.0197).

## Related documentation

- [Harmonic incidence and Boolean geometry](harmonic-incidence.md) — Up/down,
  the functions $`e_\sigma`$, Stone spaces, and zero loci.
- [LameJuis](lamejuis.md) — The harmonic model and selection strategies.
- [Theory of Time](theory-of-time.md) — Actual traversal of Boolean input states.
- [Documentation index](index/README.md) — The surrounding synthesizer model.
