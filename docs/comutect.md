---
layout: page
title: Co-muting: More ways to understand it
nav_exclude: true
permalink: /lamejuis
---

## On Language

Category theory, programming languages and Korean are all ways of expressing ideas.  If you know the language, expressing an idea in that language may resonate with you.  If you don't, no worries, but you may not the use of those languages very enlightening.  Co-muting isn't so hard to understand, but its hard to explaon, and its a different idea that what I've seen in synthesis.  I figure I'd try to express it in a few different languages, in case those languages resonate with anyone.  Let me know if you see it in yet another way!

## Category Theoretic Interpretation

Buckle up kiddos.

Let 

$$I \eq \{0,1\}$$

be the set of bools, and $\mathbb{P}$ be the space of pitches (if you'd like $\mathbb{Q}^+$ or $\log{\mathbb{Q}^+}$, it doesn't matter).  Then we can consider LaMeJuIS's switches and intervals to define a function

$$F_M : I^6 \to \mathbb{P}$$

The subscript $M$ here just reminds us the function depends on the matrix of logic switches and knobs and intervals and all that, but not the co-mutes and percentiles.  Before we go to co-muting, lets describe muting in this language.  Suppose we want to mute the second and fourth input.  Define

$$\iota_{2,4} : I^4 \sim I\times \{0,\}\times I\times \{0,\}\times I\times I \to I^6$$

to be the inclusion.  Then to mute inputs 2 and 4, simply compute the pullback $i^*F_M:I^4\to\mathbb{P}$.  All I've done is used way to fancy language to describe a simple idea. But it highlights something: although this is musically useful, it uses a non-canonical map, which makes category theory nerds anxious.  It can also be accomplished with an external muting module.

To see co-muting, first let $A$ denote the product of the non-co-muted inputs and B the product of the comuted inputs, so we have

$$A\times B \sim I^6$$

(note we may have to re-order inputs in the product, as the comuted inputs need not be contiguous).  So we have

$$F_M:A\times B \to \mathbb{P}$$

Using the good ol [tensor-hom adjoint](https://en.wikipedia.org/wiki/Tensor-hom_adjunction) (also known as [currying](https://en.wikipedia.org/wiki/Currying)), we can partially apply $F_M$ to $A$ to see it as a map

$$\tilde{F_M} : A \to \operatorname{Func}(B, \mathbb{P})$$

That is, "fix the non-co-muted inputs to get a function from the co-muted inputs to $\mathbb{P}$".  But by considering the image of $B$ in $\mathbb{P}$ (as a finite multiset), we get a function

$$\operatorname{Func}(B, \mathbb{P}) \to \operatorname{multiset}\(\mathbb{P}\)$$

which finally can map to $\mathbb{P}$ by taking the nth percentile.  Putting it all together,

$$A \to^{\tilde{F}_M} \operatorname{Func}(B, \mathbb{P})\to \operatorname{multiset}\(\mathbb{P}\)\to^{\operatorname{percentile}} \mathbb{P}$$

Remembering that $A$ is a product of some of the inputs, this is again a function mapping some of the inputs to pitch.  The values of $B$, the co-muted inputs, don't matter, but their effect on $F_M$ still do.  This is a lot of math, but it turns out it sounds better than I could have hoped, while giving you a very natural parameter (the percentile) to sequence.

If you're into this, or you know about someone thinking about synthesizers in this way, hit me up.  A niche within a niche can get lonely.

## Pseudo Code

If you speak code, nothing can be clearer than the code.  

First, we have

```
F_M([I_A, I_B, I_C, I_D, I_E, I_F]) -> Pitch
```

where `M` is the state of all the logic and the chosen intervals, and `I_A` through `I_F` are input bools.  This produces a pitch, but we can apply this function also to any other list of 6 bools.  

Here is a recursive function that starts with an empty input and accumulates a list of possible values based on the co-mute switches, followed by a function which calls the helper to compute the final pitch for that voice.

```
def get_possible_values_helper(possible_values, input, input_id):
    if input_id == num_inputs:
        # Recursive base case, compute F_M on the input
        #
        possible_values.append(F_M(input))
    else if is_co_muted(input_id):
        # input_id is co-muted, so "try both values".  get_input_gate is not called.
        #
        get_possible_values_helper(possible_values, input + [0], input_id + 1)
        get_possible_values_helper(possible_values, input + [1], input_id + 1)
    else:
        # input_id is NOT co-muted, so get the value from the gate in
        #
        get_possible_values_helper(possible_values, input + [get_input_gate(input_id)], input_id + 1)

def compute_pitch():
    possible_values = []
    get_possible_values_helper(possible_values, [], 0)
    # Now possible_values is a list of pitches
    #
    sort(possible_values)
    return possible_values[floor(get_percentile() * len(possible_values))]
```

(I have to cache this computation or the CPU for my module goes up to 40%, but that's neither here nor there)
 
So for co-muted inputs, the value of the input gate is never read.  The switches are used in the computation of `F_M` though.  

So if you co-mute all the values, you get a constant pitch, because "possible_values" will contain all 64 possible pitches, even as the inputs change.  But then you can turn the percentile knob to get any of those 64 pitches, in the index in the last line.  And of course sequencing the percentile will make music.


