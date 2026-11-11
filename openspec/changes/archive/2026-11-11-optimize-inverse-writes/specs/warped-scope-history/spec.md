## MODIFIED Requirements

### Requirement: Concurrent publication and inverse reads
UI inverse and scope reads SHALL use race-free shared payload access and a published real-time cutoff. Reused inverse coordinates outside the retained span and returned timestamps newer than the cutoff SHALL be rejected. Inverse readers SHALL busy-wait until the shared revision is unchanged and even; contention SHALL NOT return an unavailable point. Writer sections SHALL contain bounded cell/frontier transitions or fixed-capacity batches of such transitions, with unbounded traversal outside, and the audio writer SHALL NOT wait for readers. Every committed batch SHALL represent a completed prefix of forward traversal. Marker acceptance SHALL use a coherent publication while the writer remains nonblocking. Genuine absent history SHALL remain unavailable without retrying.

#### Scenario: Writer advances during display
- **WHEN** the inverse writer overlaps a UI lookup
- **THEN** the lookup retries until it reads a committed snapshot
- **AND** the returned timestamp SHALL NOT exceed the accepted publication, even when an interpolation endpoint does

#### Scenario: Absent history
- **WHEN** a coherent lookup has no retained timestamp within the captured bounds
- **THEN** the reader returns an unavailable point without retrying
- **AND** the renderer breaks its path instead of drawing zero or a partially summed weighted value

#### Scenario: Long forward traversal
- **WHEN** one sample crosses arbitrarily many grid cells
- **THEN** no individual odd revision section processes more than the fixed batch capacity
- **AND** batching preserves the final selected inverse mapping

## ADDED Requirements

### Requirement: Proved inverse write optimization
Optimized cell initialization and complete traversal SHALL preserve observable inverse reads, including missing results, cutoffs, boundaries and replacement selection. Inactive replacement fields MAY retain old values only while the discriminant makes them unreadable, and every activation SHALL initialize required fields. Lean proofs SHALL precede the native optimization, and source correspondence SHALL identify its operations. Runtime scope SHALL remain within inverse computation.

#### Scenario: Reused cell has stale replacement payload
- **WHEN** a full forward traversal replaces a cell that previously had a replacement
- **THEN** all reads select the new fallback until a later valid replacement is initialized

#### Scenario: Performance comparison
- **WHEN** an optimization is assessed for retention
- **THEN** its correctness and remote serial CPU results SHALL be compared against the landed inverse across ordinary, demanding and pathological inputs
