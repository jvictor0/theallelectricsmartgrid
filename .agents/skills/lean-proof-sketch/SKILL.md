---
name: lean-proof-sketch
description: Prove a Lean theorem by sketching the argument, giving intermediate lemmas to Sol subagents, and checking that their proofs establish the original statement. Use when a proof has separable mathematical steps or a direct attempt reveals a missing fact.
---

# Lean proof sketch

Start with the theorem as stated in the project. Read the definitions it uses and look for existing lemmas that do the work. If a short direct proof closes the goal, use it.

For a harder proof, make an informal **sketch** in whatever form helps you reason. Work backward from the conclusion. For each lemma you want, ask where you would use it and why its hypotheses should suffice to prove it. A few lines in your working context are enough.

Give each substantial lemma to a Sol subagent. With `spawn_agent`, set `model` to `gpt-6.2-sol` and `fork_turns` to `none`. Point the subagent to the exact statement and give it the relevant idea from your sketch. Send independent lemmas in parallel. A subagent can edit a module it owns directly. When several proofs belong in one module, give each subagent its own scratch `.lean` file in the project environment.

Let each subagent follow the mathematics. It can search the project and Mathlib, add the imports its proof needs, and prove local helpers. If it discovers a lemma another subagent also needs, ask it to send you the exact statement early. Choose who proves it once and tell the others where to find it. While that proof is open, a subagent can check its own argument with the shared lemma as an explicit local hypothesis and report that dependency.

While the subagents work, try the rest of your argument in a scratch declaration with the proposed lemmas as local hypotheses. Lean can tell you immediately whether a lemma has the right quantifiers or whether its caller can supply a side condition. This check establishes only the implication. Each hypothetical lemma still needs a proof independent of the target.

Use a failed attempt to revise the sketch. A subagent may find an existing theorem, a more useful statement, or a counterexample. If you change a lemma, send the revised statement to the affected subagent and recheck each use. If the original claim itself fails, show how the counterexample satisfies its hypotheses and violates its conclusion.

Have each subagent run Lean and return its file path, declaration name, and any open dependencies. When integrating a scratch proof, bring along the helpers, imports, and local context it uses. Replace temporary hypotheses with the proved lemmas and build the original declaration in the project. Verify its statement is intact and inspect its dependencies with the project's proof audit or `#print axioms` to catch `sorryAx` or newly assumed facts. If a step remains open, name the exact claim and distinguish it from what Lean has checked.
