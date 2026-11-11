## 1. Model and proof

- [x] 1.1 Prove that omitted inactive payload stores preserve every lookup and that replacement activation initializes its fields.
- [x] 1.2 Prove direct complete-cell writes and batched forward traversals equivalent to the scalar transitions, including aliased ring slots and seed/frontier metadata.
- [x] 1.3 Prove fixed batch bounds and coherent publication; run the new proofs before any native optimization.

## 2. Native inverse and validation

- [x] 2.1 Implement only the proved inverse transformations and retain a frozen landed reference for comparisons.
- [x] 2.2 Add differential startup, reversal, cutoff, boundary-touch, wrapping and concurrent-read regressions.
- [x] 2.3 Review and update source correspondence; run relevant native, proof and specification checks.

## 3. Remote performance

- [x] 3.1 Prepare studio.local's dedicated benchmark checkout and standard Release build with reproducible metadata.
- [x] 3.2 Compare landed, fewer-store, direct-cell and bounded-batch variants serially; include reader contention and the full 120-case range.
- [x] 3.3 Retain measured improvements, document normal and worst-case results and limitations, and finish the inverse-only change without deployment.
