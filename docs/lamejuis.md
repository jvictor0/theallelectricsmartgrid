---
layout: page
title: LaMeJuIS: Logic Matrix and Just Intonation Sequencer
nav_exclude: true
permalink: /lamejuis
---

![Expression]({{ "/LameJuisFrontPanel.png" | relative_url }})


## Overview

LameJuis is a module which takes some gate inputs and produces three interlocked, evolving melodie.  LameJuis is modeless and stateless, meaning the values on the inputs and positions of the many switches and knobs completely determine the output.  The melodies it creates seem to imply a strong sense of harmonic progression, tonal center and song form despite knowing nothing of scales and chords (not even the chromatic scale).  These melodies are generated using three simple ideas, none of which are hard to understand, but each of which is (to my knowledge) outside the mainstream of Eurorack sequencing.  The ideas are

* [Euler's Lattice](https://en.wikipedia.org/wiki/Tonnetz): Laying out notes in a grid such that nearby notes are strongly related.
* Logic Matrix: Using a matrix of logic operations to select notes on this grid.
* Co-Muting: Selectively ignoring inputs to the logic matrix in order to generate multiple related melodies.

## The Theory

The typical way to organize notes is to sort them numerically, by frequency, so notes close in frequency end up near each other.  In order to only pick "good" notes, the allowed frequencies are then quantized into some pre-determined scale.  Another posibility is to sort notes so that harmonically related notes are near each other.  Doing this in one dimension by stacking fifths gives the well known circle of fifths, which has a specific sort of sound, but what happens when we lay out notes in two dimensions, with one axis going up by fifths and one axis going up by major thirds, like so:

Because going up a perfect fifth and then a major third is the same as going up a major third and then a perfect fifth, we get a well defined grid of notes.  Whats more, simple geometric shapes tend to form nice chords (go find your favorites) and nearby shapes tend for form nice chord progressions.  And since the notes are laid out in 2-d, two patterns taking a small number of values can address all 12 notes, and if the ranges of those values are constrained or related in some way, the notes will have some notion of a key, or at least make some level of harmonic sense.

So the game becomes this: find a way to draw patterns on that grid.  Thats where the logic comes in.  A few logic operations (on the same inputs) are assigned to different intervals.  Count up the number of "high" gates for each interval and get a position on the grid.

One more note, (and this can mostly be ignored except by people who love tuning): because the intervals are laid out on a grid where the relationships between notes are very well defined, it makes complete sense to use "Just Intervals" instead of 12-TET intervals.  That means the frequency ratio between a note and its neighbor to the right is exactly 3/2, and its neighbor to the top is 5/4.  Don't worry, its easy to make sequences where very pure tuning won't really sound wrong.  The pure intervals can produce a delightful "buzzing" effect in some cases, and sound "very xenharmonic" if notes far about on the lattice are used together.  For musicians who are happy thinking just about 12-TET, this detail can be happily ignored.  

## Inputs

LameJuis has six gate inputs labeled A-F.  Each input is normalled to a divide-by-2 clock divider (flip flop) of the input above, meaning you can run the entire sequencer on a single clock at the A-input and get cascading clock dividers the other inputs.  Running in this mode can feel a lot like a 64-step sequencer, but by sending other sorts of gate patterns you can get all sorts of behaviors out of LameJuis.

Quick note about conventions: although you can plug any gate patterns into LameJuis you want, things become easier to explain if we assume the top inputs are changing faster than the low inputs.  Then we can ay the top inputs represent melodic movement while the middle inputs represent chord progression and bottom inputs represent song form.  This is, of course, just a thing in our brains, but our brains are so used to hearing music organized in this way we might as well exploit this hierarchy.  But from the perspective of how the sequencer actually works, all six inputs are identical.

## Logic Matrix

The six identical inputs are processed by six identical logic operations.  Each operation is a row in the large switch matrix in the middle of the module.  The first six switches, labeled A-F, determine which inputs are processed by the operation.  The switches are three position switches.  In the "up" position, the input is part of the operation.  In the middle position, it is ignored.  In the down position, the input is "negated", so treated as low if its high and high if its low.  The knob selects the logic operation.  So, for instance, if the first switch is up, the third switch is down and the rest are neutral, the operation will apply to A AND (NOT C).  The gate output of each operation is available at the corresponding Logic Out output.  The outputs are unaffected by melodic parameters like co-mutes and intervals (which will be discussed later), so already LameJuis is a highly configurable six-in-six-out logic module.  

You may notice some operations like NAND are not included.  This is because it is not needed, given the ability to negate individual inputs.  For instance, A NAND B is the same as (NOT A) OR (NOT B).  This is called De-Morgan's law and can be fun to think through if you haven't seen it before.  Similarly, there is no CV over the state of the matrix, but instead variation comes from logic operations changing their response as slower changing gates go from low to high and back again.  The logic matrix becomes surprisingly intuitive after you play with it a bit, and its very easy to just start flipping switches and see what comes out.  You can see the tips and trick section for tips and tricks.

## Intervals

Each logic operation is finally routed by the final three-position switch to one of three intervals.  These intervals are determined by the three Interval knobs and their corresponding CV input.  When the operation outputs high, the pitch moves up by the selected interval (before co-muting), and is unaffected otherwise.  Consider the following example.  The first three operations have the interval selector switch in the "up" position, so are routed to a perfect fifth, P5.  The second two are routed to the middle, a major third M3, and the last is routed to the third and final interval, which is "Off".  Since two of the fifths and one third are high (see the indicator light in the "Logic out" column), the pitch moves up from C by two fifths and a major third, to F#.  To say another way, for each of the three possible positions of the interval selector switch, count the number of logic operations which are currently high, and go up by that interval.

The value of the "Interval CV" input is added to the interval from the knob.  Thus if the knob is "Off", the Interval CV is used (so 12 TET intervals can be supplied, or other just ratios or microtonal generators, or an offset from a slider).  

For the most part, you can pretend these intervals are "standard" 12-TET intervals you might find on the piano.  However, because of the explicit use of intervals in calculating the pitch, Just Intonation is natural.  Insead of irrational intervals like 2^{7/12} for a perfect fifth, Just frequency ratios like 3/2 are used. The following ratios are used:

- Off: 1/1 (unison)
- m2: 16/15 (an octave up, a fifth and a major third down).
- M2: 9/8 (two fifths up, an octave down)
- m3: 6/5 (a fifth up, a major third down)
- M3: 5/4 (the fifth harmonic, two octaves down)
- P4: 4/3 (an octave up, a fifth down)
- P5: 3/2 (the third harmonic, an octave down)
- m7: 7/4 (the 7th harmonic, two octaves down)
- 8va: 2/1 (an octave up)

This is the melody that will come out if all co-mutes are tuned off (the up position).  And the melodies are quite fascinating.  No music theory knowledge was used to construct them, no scale chosen, and yet the feeling of key, cadence, theme and variation arise naturally, depending on the switch positions.  All we did was some binary logic and multiplied some fractions, yet the result definitively sounds like something.  Depending on the configuration, melodies can go from quite familiar to exotic and xen, but there's a quality about it which, at least to my ears, sounds musical.

## Co-Muting

The above algorithm produces a single melody, which has a kind of jolted quality to it.  Co-muting, and the associated percentile knob and CV input, allow the single matrix described above to produce multiple interlocking melodies while producing "smoother" melodies.  The idea is as follows: start by assuming each input is a flip-flop of the one above.  Then, for instance, you might hear A and B as a melody outlining a chord progression defined by C, D and E which changes from a "verse" to "chorus" based on F (these, of course, are just stories the composer tells themselves!).  How can you create a bass-line from this?  Well, we're saying that the state of inputs (C,D,E,F) defines an implied chord that is played as A and B vary.  As A and B vary, up to four pitches are played, so why not say the bass just plays the lowest of the four!  Its an entirely simplistic idea, but it the results just sound right.

Co-muting generalizes this idea, and its very fun, performable and sequencable.  Each of the three identical voices has six co-mute switches and a percentile knob with CV input.  For each voice, the values of the co-muted inputs are ignored.   However, instead of being set to zero, the swith matrix is evaluated at all possible values of the co-muted inputs.  The results are sorted, and based off the percentile knob and CV one of the values is selected.  For instance, if A and B are comuted, the matrix will be evaluated as if A and B were both high, both low, A high/B low, and B high/A low.  If the percentile knob+CV is fully counter-clockwise, the lowest of the four is chosen, while if fully clockwise the highest is chosen, and a middle value if its in the middle.  In other words, among possible pitches which the matrix can produce leaving the non-co-muted values fixed and trying every possible co-muted value, pick the one which, from low-to-high, matches percentile input+CV.

These pitches are available at the three voice outputs.  The trigger output produces a gate whenever these values change.

As far as I'm aware, this is a novel method for generated sequences.  It isn't as tricky to learn as it might seem, just play with the switches.  Co-muting the left few inputs tends to produces harmonically coherent melodies.  Voices with more co-muted inputs tend to play more sparesly.  Multiple voices with the same inputs co-muted but different positions for the percentile knob tend to form a bits of counterpoint and interlocked rhythms.

## Percentile CV ins

