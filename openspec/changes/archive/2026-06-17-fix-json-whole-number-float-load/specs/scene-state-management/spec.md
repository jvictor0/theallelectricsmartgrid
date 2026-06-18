## ADDED Requirements

### Requirement: Patch Load Preserves Default Whole-Number Encoder Floats
Patch save/load round-trips SHALL preserve default non-zero encoder float parameters whose normalized values are exact whole numbers. Loading a patch saved from a fresh system MUST NOT turn such parameters into zero because of JSON numeric node type differences.

#### Scenario: Fresh patch preserves master and low-pass defaults
- **WHEN** a fresh patch is saved and then loaded into a fresh system
- **THEN** `MasterVolume` is restored to `1.0`
- **AND** `LPCutoff` is restored to `1.0`
- **AND** `Source1LP` is restored to `1.0`

#### Scenario: Whole-number float patch remains compatible
- **WHEN** a patch file contains encoder float fields encoded as JSON whole-number tokens such as `1`
- **THEN** patch load restores those fields to their numeric values
- **AND** no patch file migration is required
