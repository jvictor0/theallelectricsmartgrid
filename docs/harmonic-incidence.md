# Harmonic incidence and Boolean geometry

For a fixed LameJuis configuration and lens, harmonic analysis starts with a
relation between time slices and available pitch labels. This page defines its
Galois connection and expresses it using Boolean functions on a finite Stone
space. [Harmonic concepts and persistence](harmonic-persistence.md) develops the
associated complexes, filtrations, and barcodes.

These pages specify a mathematical analysis model. They do not assert that the
engine currently computes concepts or persistent homology. See
[LameJuis](lamejuis.md) for the pitch engine and its implementation terminology.

## Domains and notation

Fix an accepted configuration $M$ and a lens $U$. Let $I=\{0,1\}$ and

$$
X=I^d,\qquad T=X/{\sim_U},\qquad \pi_U:X\longrightarrow T.
$$

Two inputs are equivalent when they agree on the coordinates read by $U$.
Thus $T\cong I^{|U|}$, and each element of $T$ represents an entire fiber of
co-muted inputs. The current engine has $d=6$. The symbol $d$ denotes this input
dimension; $k$ will denote a filtration threshold and $q$ a homological degree.

Choose a finite set $\Sigma$ of pitch or section identities and write
$p_M:X\to\Sigma$ for the resulting output map. This is the map called $M$ in
the expression $F_x^M(U)$ in the engine documentation. Here the subscript
distinguishes the configuration from the function it determines.

The identity convention is part of the analysis: accumulator sections, exact
evaluated pitches, and pitches modulo octave can give different sets $\Sigma$.
Choose one convention before constructing the relation. Take $\Sigma$ to be
the image of $p_M$, so every label occurs somewhere.

| Symbol | Meaning |
| --- | --- |
| $x,y\in X$ | Full Boolean input states |
| $t\in T$ | A time slice modulo the fixed lens |
| $S\subseteq T$, $s\in S$ | A collection of slices and one of its slices |
| $\sigma,\tau\in\Sigma$ | Pitch labels |
| $C\subseteq\Sigma$ | A collection of pitch labels |
| $\mathcal P(E)$ | The power set of a set $E$ |

For $t=\pi_U(x)$, define

$$
F_t^M(U)=p_M(\pi_U^{-1}(t))=F_x^M(U).
$$

This is independent of the representative $x$. The incidence relation is

$$
R=R^{M,U}\subseteq T\times\Sigma,
\qquad tR\sigma\iff\sigma\in F_t^M(U).
$$

This relation describes availability across all slices. A chronological path
through those slices is additional data from the
[Theory of Time](theory-of-time.md).

## The up and down operations

Define the two order-reversing operations

$$
\begin{aligned}
S^\uparrow
&=\{\sigma\in\Sigma:\forall s\in S,\ sR\sigma\},\\
C^\downarrow
&=\{t\in T:\forall\sigma\in C,\ tR\sigma\}.
\end{aligned}
$$

The **intent** $S^\uparrow$ is the set of pitches common to every slice in $S$.
The **extent** $C^\downarrow$ is the set of slices admitting every pitch in $C$.
The up/down notation is an established convention for the derivation operations
of formal concept analysis; see, for example,
[this formulation of the operators](https://link.springer.com/article/10.1007/s40314-025-03440-3).

Their defining Galois equivalence is

$$
C\subseteq S^\uparrow\quad\Longleftrightarrow\quad S\subseteq C^\downarrow.
$$

Universal quantification gives the empty-set conventions
$\varnothing^\uparrow=\Sigma$ and $\varnothing^\downarrow=T$. Composing the
operations gives two closures:

$$
\operatorname{cl}_T(S)=S^{\uparrow\downarrow},
\qquad
\operatorname{cl}_\Sigma(C)=C^{\downarrow\uparrow}.
$$

Each closure is extensive, order-preserving, and idempotent. In particular,

$$
S^{\uparrow\downarrow\uparrow}=S^\uparrow,
\qquad C^{\downarrow\uparrow\downarrow}=C^\downarrow.
$$

Closing a set of slices therefore adds every slice satisfying its common pitch
constraints, without changing those common pitches. Closing a pitch collection
adds every pitch that is always available wherever the original collection is
available.

For illustration, restrict to the following three-slice relation. The operations
in this example use only the displayed rows:

| Slice | $\alpha$ | $\beta$ |
| --- | --- | --- |
| $t_1$ | available | |
| $t_2$ | available | available |
| $t_3$ | | available |

Here $\{t_1\}^\uparrow=\{\alpha\}$, so
$\{t_1\}^{\uparrow\downarrow}=\{t_1,t_2\}$.
Also $\{\alpha,\beta\}^\downarrow=\{t_2\}$. The singleton $\{t_1\}$
is not closed under the incidence closure even though it is a closed subset of
the finite Stone space.

## Boolean functions for pitch availability

Let

$$
A=\operatorname{Map}(T,\mathbb F_2).
$$

This is both a Boolean ring and a Boolean algebra: ring addition is XOR,
multiplication is AND, and complement is $f\mapsto1+f$. Define

$$
e:\Sigma\longrightarrow A,
\qquad e_\sigma(t)=1+\mathbf1[tR\sigma].
$$

The convention is that **zero means available**. Thus $e_\sigma$ is the unique
Boolean function with zero locus equal to the availability locus of $\sigma$.
The principal ideal it generates is exactly

$$
(e_\sigma)
=\{f\in A:f(t)=0\text{ whenever }tR\sigma\}.
$$

Indeed, every multiple of $e_\sigma$ vanishes there; conversely any such $f$
satisfies $f=e_\sigma f$.

The map $e$ need not be injective. Different pitches can have identical
availability patterns and hence equal Boolean functions and equal principal
ideals. They remain different members of $\Sigma$ and count separately in
$|S^\uparrow|$. Replacing the labelled family $(e_\sigma)_{\sigma\in\Sigma}$
by the unlabelled set $e(\Sigma)$ would lose this count.

This label multiplicity is also distinct from the number of full input states
producing the same pitch. The latter is recorded by

$$
W_{t,\sigma}=\#\{y\in\pi_U^{-1}(t):p_M(y)=\sigma\}.
$$

The binary relation remembers only whether $W_{t,\sigma}>0$. The Dowker
thresholds in the companion page count pitch labels, not these input witnesses.
The multiset viewpoint remains relevant to the engine's percentile chooser;
see [co-muting](comutect.md).

## Zero loci and vanishing ideals

Use $Z$ for the common-zero operation. For pitch labels this means

$$
Z(C)=\{t\in T:\forall\sigma\in C,\ e_\sigma(t)=0\}=C^\downarrow.
$$

For a family of functions or an ideal in $A$, $Z$ has the same common-zero
meaning. In particular,

$$
Z(C)=Z\bigl(\langle e_\sigma:\sigma\in C\rangle\bigr).
$$

To discuss the full algebraic vanishing ideal, use a different typeface:

$$
\mathfrak I(S)=\{f\in A:f|_S=0\}.
$$

The relationship to the up operation is especially direct:

$$
S^\uparrow=e^{-1}(\mathfrak I(S)).
$$

Thus the up operation returns **labels whose designated functions vanish on
$S$**. The ideal $\mathfrak I(S)$ contains **all functions vanishing on $S$**.
The earlier notation $I(S)$ for common pitches is superseded by $S^\uparrow$;
$\mathfrak I$ is reserved for the actual ideal.

The selected equations generate

$$
J(S)=\langle e_\sigma:\sigma\in S^\uparrow\rangle
\subseteq\mathfrak I(S),
$$

and the two zero-locus identities are

$$
Z(J(S))=S^{\uparrow\downarrow},\qquad Z(\mathfrak I(S))=S.
$$

For the availability table above, $e_\alpha=(0,0,1)$ and
$e_\beta=(1,0,0)$, listing values at $t_1,t_2,t_3$. If $S=\{t_1\}$,
then $J(S)=(e_\alpha)$ cuts out $\{t_1,t_2\}$. The full ideal
$\mathfrak I(S)$ additionally contains $(0,1,0)$, which excludes $t_2$.

The incidence closure is therefore closure relative to a labelled family of
pitch equations. In the finite Boolean coordinate ring, every subset already
has its own full vanishing ideal and is closed in the Zariski topology.

## The affine Boolean scheme

The Boolean cube has coordinate ring

$$
A_X=\mathbb F_2[z_1,\ldots,z_d]/(z_i^2-z_i)_{i=1}^{d}
\cong\operatorname{Map}(X,\mathbb F_2)
\cong\prod_{x\in X}\mathbb F_2.
$$

Consequently $X\cong\operatorname{Spec}A_X$. Likewise,
$T\cong\operatorname{Spec}A$; pullback along $\pi_U$ identifies $A$ with the
subring of functions on $X$ constant on each lens fiber. The cube has $2^d$
points, so its function ring has $2^d$ field factors. This differs from the
ring $\mathbb F_2^d$, whose spectrum has $d$ points.

Finite Stone spaces are discrete. General Stone spaces have a basis of clopen
sets and need not be discrete. In a Boolean ring the principal zero locus of
$f$ equals the principal open locus of $1+f$. This is the precise relationship
between the affine and clopen descriptions; see
[Tressl on Stone duality and prime spectra](https://personalpages.manchester.ac.uk/staff/marcus.tressl/papers/StoneDualityBooleanAlgebras.pdf).

Boolean constraints $g_1=\cdots=g_r=1$ define the solution scheme

$$
\operatorname{Spec}\bigl(A_X/(1+g_1,\ldots,1+g_r)\bigr).
$$

The constraints are unsatisfiable exactly when this ideal contains $1$.
The harmonic equations $e_\sigma=0$ use this same algebra of constraints.

## Boolean sections and the lens presheaf

Separate the Boolean output map from its pitch evaluation:

$$
X\xrightarrow{\beta_M}\mathbb F_2^m
\xrightarrow{\phi}P,\qquad p_M=\phi\circ\beta_M.
$$

Here $m$ counts Boolean outputs, and $P$ is the chosen pitch or section space;
$\Sigma$ is the finite image of the composite. The Boolean output is a global
section

$$
\beta_M\in\Gamma(X,\mathcal O_X^{\oplus m})=A_X^m.
$$

The free module sheaf is $\mathcal O_X^{\oplus m}$; $\beta_M$ is one of its
sections. The pitch evaluator $\phi$ is generally a map of sets rather than an
$\mathbb F_2$-linear map. The composite is a global section of the sheaf of
sets $V\mapsto\operatorname{Map}(V,P)$ on the discrete space $X$.

The lens construction instead takes the **image of that chosen section** on a
fiber, $F_x^M(U)=p_M(\pi_U^{-1}(\pi_U(x)))$. For fixed $x$, if $U\subseteq V$
as sets of read coordinates, there is an inclusion

$$
F_x^M(V)\hookrightarrow F_x^M(U).
$$

This defines a presheaf on the poset of lenses. It can be realized as a sheaf
on that poset with the Alexandrov topology of downward-closed subsets, where
the minimal open neighborhood of $U$ is the set of lenses contained in $U$.
This topology is different from treating lenses themselves as open subsets of
the coordinate set and imposing gluing for their unions.

The Boolean module sheaf on $X$, the presheaf over lenses, and the fixed-lens
incidence relation have different bases and roles. The nontrivial simplicial
topology studied next comes from the incidence relation between points of $T$
and labels in $\Sigma$.

## Continue reading

- [Harmonic concepts and persistence](harmonic-persistence.md) — Formal concepts,
  closure retractions, Dowker duality, and barcode analysis.
- [LameJuis](lamejuis.md) — Logic configuration, sections, lenses, and selection.
- [Documentation index](index/README.md) — The surrounding synthesizer model.
