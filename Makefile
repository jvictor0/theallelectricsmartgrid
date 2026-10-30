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
