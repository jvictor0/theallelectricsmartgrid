## Purpose

Defines the maintained build entry points for the repository so obsolete wrappers and retired compile modes do not remain discoverable as supported targets.

## Requirements

### Requirement: Supported Build Entry Points
The repository SHALL define the maintained build entry points as the SmartGridOne JUCE app builds, including iOS, and the standalone CMake test executable.

#### Scenario: Maintained app builds are discoverable
- **WHEN** a developer inspects project build files
- **THEN** the SmartGridOne JUCE app project remains available
- **AND** the SmartGridOne iOS exporter under `JUCE/SmartGridOne/Builds/iOS/` remains available
- **AND** obsolete top-level iOS wrapper projects are not presented as supported targets

#### Scenario: Maintained tests are discoverable
- **WHEN** a developer inspects test build files
- **THEN** the maintained test entry point is the standalone CMake target under `private/test`

### Requirement: Obsolete Top-Level App Wrappers Removed
The repository SHALL NOT retain checked-in obsolete top-level iOS wrapper projects.

#### Scenario: TheNonagon wrapper removed
- **WHEN** the repository tree is scanned for top-level app wrappers
- **THEN** `TheNonagon/` is absent
- **AND** `private/src/TheNonagon.hpp` remains available

#### Scenario: OctaFade cleanup
- **WHEN** the repository tree is scanned for `OctaFade` project artifacts
- **THEN** no top-level `OctaFade` application project remains checked in

#### Scenario: SmartGridOne iOS exporter preserved
- **WHEN** the JUCE project tree is scanned for generated build projects
- **THEN** `JUCE/SmartGridOne/Builds/iOS/` is present

### Requirement: No Embedded Build Mode
The repository SHALL NOT define or document `EMBEDDED_BUILD` as a supported compile mode.

#### Scenario: Test target compile definitions
- **WHEN** the standalone CMake test target is configured
- **THEN** it does not define `EMBEDDED_BUILD`

#### Scenario: Production headers
- **WHEN** production headers are compiled by supported targets
- **THEN** they do not select behavior through `EMBEDDED_BUILD`

### Requirement: Private Source Has No JUCE Dependency
Code under `private/src` SHALL NOT include JUCE headers directly or indirectly through project-owned compatibility scaffolding.

#### Scenario: Core header scan
- **WHEN** files under `private/src` are scanned
- **THEN** they do not contain `#include <JuceHeader.h>`
- **AND** they do not contain `#include "JuceHeader.h"`

#### Scenario: Standalone test compile
- **WHEN** the standalone CMake test target compiles core headers
- **THEN** it does not require JUCE include paths

### Requirement: Documentation Matches Supported Targets
Docs, specs, and helper scripts SHALL describe SmartGridOne JUCE app builds, including iOS, and standalone tests as the maintained build topology.

#### Scenario: Current support docs
- **WHEN** documentation describes current supported platforms or build targets
- **THEN** it preserves the SmartGridOne iOS app as supported
- **AND** it does not claim that obsolete top-level wrappers or embedded targets are supported
