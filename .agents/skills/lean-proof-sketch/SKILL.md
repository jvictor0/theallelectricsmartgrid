---
name: lean-proof-sketch
description: Prove a Lean theorem by checking a hierarchy of conditional proofs before delegating leaf lemmas to Sol subagents. Use when a proof has separable mathematical steps or a direct attempt reveals a missing fact.
---

# Lean proof sketch

Start with the theorem as stated in the project. Read the definitions it uses and look for existing lemmas that do the work. If a short direct proof closes the goal, use it.

## Before delegating

The lead agent works backward from the conclusion and writes the exact Lean statements of the lemmas it needs. For the original theorem, check a scratch proof in Lean that takes those lemmas as explicit local hypotheses and establishes the original conclusion under its original hypotheses. This checks that the caller can supply every side condition and that the lemmas together suffice.

Apply the same procedure recursively to any lemma whose proof is too large or whose argument has an unresolved step. Break it into smaller lemmas and check that the parent follows from their exact statements. For each leaf, identify a concrete proof argument from its stated hypotheses and check any uncertain side conditions. Resolve plausible counterexamples and gaps in that argument before delegating. A restatement of an open case with no proof argument needs further decomposition.

Before spawning any subagent, the lead agent must have checked the conditional proof at every level and confirmed a proof route for every leaf. Keep dependencies acyclic. Use scratch declarations with explicit hypotheses so the check leaves every assumed lemma visible and adds no axioms or `sorry`.

## Prove and integrate

Give each substantial leaf to a Sol subagent. With `spawn_agent`, set `model` to `gpt-6.2-sol` and `fork_turns` to `none`. Send the exact statement, its proof idea, and any checked dependencies. Send independent leaves in parallel and assign each shared lemma once. A subagent can edit a module it owns directly. When several proofs belong in one module, give each subagent its own scratch `.lean` file in the project environment.

Have each subagent run Lean and return its file path, declaration name, and any open dependencies. If a subagent finds a missing fact or a counterexample, the lead agent revises the affected part of the hierarchy and rechecks its conditional proofs before sending revised tasks. If the original claim fails, show how the counterexample satisfies its hypotheses and violates its conclusion.

Integrate the proved leaves and discharge the local hypotheses from the bottom up. Include the helpers, imports, and local context they use. Build the original declaration in the project, verify its statement is intact, and inspect the project's proof audit or `#print axioms` for `sorryAx` or newly assumed facts. An open integration step calls for another round of decomposition and checking. If a concrete blocker leaves a step open, name the exact claim and distinguish it from what Lean has checked.
