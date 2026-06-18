# Arena JSON Specification

## Purpose
Smart Grid One serializes its entire patch to JSON on the audio thread (`ToJSON` is polled out of `StateInterchange` inside `ProcessFrame`). `arena-json` (`private/src/Json.hpp`) is a single self-contained JSON value library — depending on neither jansson nor JUCE — that makes this real-time safe by bump-allocating every node, key, and string from a caller-owned arena: the audio thread only advances a pointer, never calling the system allocator. The same library serves the parse direction (`Loads`) and serialization (`Dumps`), and presents the JSON API surface used across the codebase (`Object`/`Array`/`SetNew`/`Append`/`Get`/`GetAt`/`Size`/`StringValue`/`IntegerValue`/`RealValue`/`BooleanValue`/`IsNull`/`Loads`/`Dumps`). It replaced the former dual backend (jansson under `EMBEDDED_BUILD`, `juce::var` in the plugin), which heap-allocated on the audio thread in both builds. See scene-state-management for the patch save/load flow that owns the arena.

## Requirements

### Requirement: Bump-Allocated Document Arena
A JSON document SHALL be backed by an arena that owns a single contiguous byte buffer of a fixed capacity. Allocation SHALL advance an offset pointer with correct alignment and SHALL NOT free individual allocations; all memory is reclaimed only when the arena is destroyed or reset. The arena's backing buffer SHALL be allocated by the owner (heap allocation) before the arena is handed to a consumer; consumers SHALL only bump the offset and never grow the buffer themselves.

#### Scenario: Sequential allocations advance the offset
- **WHEN** two values are allocated from a fresh arena
- **THEN** the second value's address is at or beyond the first's address plus the first's aligned size
- **AND** no allocation is individually freed for the lifetime of the arena

#### Scenario: Reset reclaims all allocations at once
- **WHEN** an arena that has allocated several nodes is reset
- **THEN** the offset returns to zero and the failed flag clears
- **AND** subsequent allocations reuse the same buffer from the start

### Requirement: Allocation Failure Sets a Flag and Yields Null
When an allocation cannot fit in the remaining capacity, the arena SHALL set a sticky `failed` flag and return a null value rather than allocating, crashing, or invoking the system allocator. Once `failed` is set, every further allocation SHALL also return null until the arena is reset.

#### Scenario: Exhaustion returns null instead of allocating
- **WHEN** an allocation request exceeds the arena's remaining capacity
- **THEN** the request returns a null value
- **AND** the arena's `failed` flag is set
- **AND** the system heap allocator is not called

#### Scenario: Failure is sticky until reset
- **WHEN** an allocation has failed and a later, smaller allocation is requested before reset
- **THEN** the later allocation also returns null

### Requirement: Null Propagates Without Per-Call Checks
Building operations SHALL be null-tolerant: setting a member on, appending to, or reading from a null node SHALL be a no-op (writes) or return null/zero/empty (reads), never a crash. A builder that runs on an exhausted arena SHALL therefore collapse to a null root, allowing the caller to detect failure with a single check at the top level instead of after every allocation.

#### Scenario: Writes onto a null node are no-ops
- **WHEN** a member is set on a null object node or a value is appended to a null array node
- **THEN** the operation has no effect and does not crash

#### Scenario: Mid-build exhaustion yields a null root
- **WHEN** a multi-level structure is built and the arena exhausts partway through
- **THEN** the top-level root reported to the caller is null
- **AND** the caller can detect the failure by checking only the root

### Requirement: Copy-on-Grow Containers
Object and array containers SHALL store their entries in a contiguous block within the arena and SHALL provide O(1) indexed access. When a container exceeds its current block, it SHALL bump-allocate a new block of at least double the capacity, copy existing entries into it, and abandon the old block in place. If the growth allocation fails, the arena `failed` flag SHALL be set per the failure requirement.

#### Scenario: Growth preserves existing entries
- **WHEN** entries are appended past a container's initial block capacity
- **THEN** the container allocates a larger block, copies prior entries, and all previously appended values remain readable at their original indices

#### Scenario: Indexed access stays constant-time
- **WHEN** an element is read by index from an array of any size
- **THEN** the read resolves without scanning preceding elements

### Requirement: Arena-as-Factory Build API
Value construction SHALL be performed through the arena instance: the arena SHALL expose factory methods that allocate and return object, array, string, integer, real, boolean, and null values from its own buffer. Strings and object keys SHALL be copied into the arena at construction time, so callers MAY pass temporary buffers. Functions that build JSON SHALL receive the arena as their first argument and thread it to the values they construct.

#### Scenario: Factories allocate from the owning arena
- **WHEN** a value is created via an arena factory method
- **THEN** the value's storage lies within that arena's buffer

#### Scenario: Strings and keys are copied
- **WHEN** a string value or object key is created from a temporary buffer that is then overwritten
- **THEN** the stored JSON string and key retain their original content

### Requirement: Audio-Thread-Safe Serialization with Message-Thread Retry
Building a JSON document on the audio thread SHALL allocate solely from a caller-provided arena and SHALL NOT call the system allocator. When the produced root is null (arena exhausted), the owning (message) thread SHALL free the arena, allocate a new arena of at least double the previous capacity, and re-request the build; the audio thread SHALL only ever bump the arena offset. The default arena capacity SHALL be sized so that retry does not occur for representative patches.

#### Scenario: Audio-thread build performs no heap allocation
- **WHEN** a document is built from a sufficiently-sized arena on the audio thread
- **THEN** no system heap allocation occurs during the build
- **AND** a non-null root is produced

#### Scenario: Exhaustion triggers a doubled retry off the audio thread
- **WHEN** an audio-thread build returns a null root because the arena was too small
- **THEN** the message thread releases the arena, allocates one of at least double the capacity, and re-requests the build
- **AND** the retried build with adequate capacity produces a non-null root

### Requirement: Parse, Serialize, and Round-Trip Compatibility
The library SHALL parse a JSON text into an arena-backed tree (`Loads`) and serialize an arena-backed tree to a JSON text string (`Dumps`). Parsed nodes SHALL be allocated from the arena under the same failure semantics as the build API. Serialization SHALL produce output that round-trips with existing on-disk patch files written by the prior backends, with `Incref`/`Decref` defined as no-ops because node lifetime equals arena lifetime.

#### Scenario: Build/serialize/parse round-trips
- **WHEN** a document is built, serialized to a string, and parsed back into a new arena
- **THEN** the parsed tree is structurally equal to the original

#### Scenario: Existing patch files load and re-emit equivalently
- **WHEN** an existing on-disk patch file is parsed and re-serialized
- **THEN** the result is semantically equivalent to the original patch

### Requirement: Numeric Float Reads Accept Whole-Number Tokens
The arena JSON library SHALL provide a numeric floating-point read path for schema fields that semantically expect real-valued numbers. This read path SHALL return the stored value for both `JsonType::Real` and `JsonType::Integer` nodes, including integer nodes produced by parsing JSON tokens without a decimal point or exponent.
Strict type-specific reads MAY remain available for callers that need to distinguish integer and real nodes, but persisted float fields MUST have an accessor that treats JSON numeric spellings such as `1`, `1.0`, and `1e0` as the same numeric value.

#### Scenario: Parsed whole number reads as a float
- **WHEN** JSON text `{"value":1}` is parsed into an arena-backed tree
- **THEN** reading `value` through the numeric floating-point read path returns `1.0`

#### Scenario: Parsed real token reads as the same float
- **WHEN** JSON text `{"value":1.0}` is parsed into an arena-backed tree
- **THEN** reading `value` through the numeric floating-point read path returns `1.0`

#### Scenario: Strict reads remain type-specific
- **WHEN** a caller uses strict integer or real accessors
- **THEN** those accessors preserve their documented type-specific behavior
