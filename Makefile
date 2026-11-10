REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
JUCE_DIR := $(REPO_ROOT)/JUCE/SmartGridOne
JUCE_TARGETS := juce-setup juce-deps build clean run build-sanitized run-sanitized clang-tidy \
	ios-setup ios-build ios-install ios-deploy deploy-ios list-ios-devices
JOBS ?= 4
CORE_CONFIG ?= RelWithDebInfo
VISUALIZERS_CONFIG ?= Release
MIDI_CONFIG ?= Release
CMAKE_ARGS ?=

.PHONY: all $(JUCE_TARGETS)

all: build

$(JUCE_TARGETS):
	$(MAKE) -C "$(JUCE_DIR)" $@

# Complete setup once before starting recursive builds, including parallel goals.
build run build-sanitized run-sanitized clang-tidy ios-build ios-deploy deploy-ios \
	visualizers-configure midi-configure: juce-deps

define cmake_targets
.PHONY: $(1)-configure $(1)-build $(1)-test
$(1)-configure:
	cmake -S "$(REPO_ROOT)/$(2)" -B "$(REPO_ROOT)/build/$(1)/$$($(3))" -DCMAKE_BUILD_TYPE="$$($(3))" $$(CMAKE_ARGS)

$(1)-build: $(1)-configure
	cmake --build "$(REPO_ROOT)/build/$(1)/$$($(3))" --config "$$($(3))" --parallel "$$(JOBS)"

$(1)-test: $(1)-build
	ctest --test-dir "$(REPO_ROOT)/build/$(1)/$$($(3))" --build-config "$$($(3))" --output-on-failure
endef

$(eval $(call cmake_targets,core,private/test,CORE_CONFIG))
$(eval $(call cmake_targets,visualizers,private/test/juce-visualizers,VISUALIZERS_CONFIG))
$(eval $(call cmake_targets,midi,private/test/juce-midi,MIDI_CONFIG))

.PHONY: lean-setup lean-cache lean-test
lean-setup:
	"$(REPO_ROOT)/scripts/lean.sh" setup

lean-cache:
	"$(REPO_ROOT)/scripts/lean.sh" exe cache get Mathlib.Analysis.SpecialFunctions.Pow.Real Mathlib.Tactic.Linarith Mathlib.Tactic.SplitIfs Mathlib.Tactic.NormNum Mathlib.Tactic.Ring Mathlib.Analysis.SpecialFunctions.Trigonometric.Bounds

lean-test:
	python3 "$(REPO_ROOT)/scripts/check_encoder_spec.py"
	python3 "$(REPO_ROOT)/scripts/check_phase_spec.py"
	"$(REPO_ROOT)/scripts/lean.sh" build
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/Encoder/Audit.lean"
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/PhaseTime/Audit.lean"

PERF_DIR := $(REPO_ROOT)/build/tools/perf/Release
.PHONY: perf-configure perf-build perf-test perf-run
perf-configure:
	cmake -S "$(REPO_ROOT)/private/test/perf" -B "$(PERF_DIR)" -DCMAKE_BUILD_TYPE=Release $(CMAKE_ARGS)

perf-build: perf-configure
	cmake --build "$(PERF_DIR)" --config Release --parallel "$(JOBS)"

perf-test: perf-build
	ctest --test-dir "$(PERF_DIR)" --build-config Release --output-on-failure

perf-run: perf-build
	python3 "$(REPO_ROOT)/private/test/perf/run.py" --bin-dir "$(PERF_DIR)" $(PERF_ARGS)
