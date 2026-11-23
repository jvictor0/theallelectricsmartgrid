---
name: lean-proof-sketch
description: Prove a Lean theorem by sketching the argument, giving intermediate lemmas to Sol subagents, and checking that their proofs establish the original statement. Use when a proof has separable mathematical steps or a direct attempt reveals a missing fact.
---

# Lean proof sketch

Start with the exact theorem the user wants. Find its declaration in the project, or state it in Lean if it only exists in prose. Check its assumptions, quantifiers, and any numerical threshold against the request. Read its definitions and look for existing lemmas that do the work. If a short direct proof closes the goal, use it.

For a harder proof, make an informal **sketch** in whatever form helps you reason. Work backward from the conclusion. For each proposed lemma, identify where its conclusion is used and why you think it holds. If a proposed lemma simply renames the hard part, unpack the argument for it or flag it as the open problem. A few lines in your working context are enough.

Before delegating the proofs, have Lean check a scratch proof of the original theorem with the proposed lemmas as explicit local hypotheses. Every added hypothesis is a proof you still owe. For a quantitative target, substitute the intended constants and make Lean check the final inequality. If the proof leaves a gap, give that gap a precise statement and revise the sketch. You can ask a subagent to help find the missing argument. Passing this check establishes that proving the proposed lemmas would finish the user’s theorem.

Give each substantial lemma to a Sol subagent. With `spawn_agent`, set `model` to `gpt-6.2-sol` and `fork_turns` to `none`. Point the subagent to the exact statement and give it the relevant idea from your sketch. Send independent lemmas in parallel. A subagent can edit a module it owns directly. When several proofs belong in one module, give each subagent its own scratch `.lean` file in the project environment.

Let each subagent follow the mathematics. It can search the project and Mathlib, add the imports its proof needs, and prove local helpers. If it discovers a lemma another subagent also needs, ask it to send you the exact statement early. Choose who proves it once and tell the others where to find it. While that proof is open, a subagent can check its own argument with the shared lemma as an explicit local hypothesis and report that dependency.

When a proof stalls, ask for the remaining goal and the inference that failed. A counterexample to a proposed lemma rules out that step. To refute the user’s theorem, the example must satisfy that theorem’s hypotheses and violate its conclusion. If the lemma is still open, try a different argument or revise the sketch and recheck the implication to the original target before redirecting the subagents.

Have each subagent run Lean and return its file path, declaration name, and any open dependencies. When integrating a scratch proof, bring along the helpers, imports, and local context it uses. Replace temporary hypotheses with the proved lemmas and build the original declaration in the project. Verify its statement is intact and inspect its dependencies with the project's proof audit or `#print axioms` to catch `sorryAx` or newly assumed facts. If you cannot finish, identify the outstanding lemma, where the original proof needs it, and what the failed attempt establishes. For a bound that misses its target, give the bound obtained and the bound required.
