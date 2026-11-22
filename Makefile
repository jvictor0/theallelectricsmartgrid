REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
JUCE_DIR := $(REPO_ROOT)/JUCE/SmartGridOne
JUCE_TARGETS := juce-setup juce-deps build clean run build-sanitized run-sanitized clang-tidy \
	ios-setup ios-build ios-install ios-deploy deploy-ios list-ios-devices
JOBS ?= 4
CORE_CONFIG ?= RelWithDebInfo
VISUALIZERS_CONFIG ?= Release
MIDI_CONFIG ?= Release
CMAKE_ARGS ?=
CCACHE ?= $(shell command -v ccache 2>/dev/null)
CMAKE_CACHE_ARGS := $(foreach language,C CXX OBJC OBJCXX,-DCMAKE_$(language)_COMPILER_LAUNCHER="$(CCACHE)")

ifneq ($(strip $(CCACHE)),)
CMAKE_BUILD_ENV = cache_dir=$$(python3 "$(REPO_ROOT)/scripts/build_cache.py" root) && \
	CCACHE_BASEDIR="$${CCACHE_BASEDIR:-$(REPO_ROOT)}" CCACHE_DIR="$${CCACHE_DIR:-$$cache_dir/ccache}"
endif

define cmake_build
$(CMAKE_BUILD_ENV) cmake --build "$(1)" --config "$(2)" --parallel "$(JOBS)" $(3)
endef

.PHONY: all $(JUCE_TARGETS)

all: build

$(JUCE_TARGETS):
	$(MAKE) -C "$(JUCE_DIR)" $@

define cmake_targets
.PHONY: $(1)-configure $(1)-build $(1)-test
$(1)-configure:
	$(if $(4),juce_root=$$$$(python3 "$(REPO_ROOT)/scripts/build_cache.py" juce) && )cmake \
		-S "$(REPO_ROOT)/$(2)" -B "$(REPO_ROOT)/build/$(1)/$$($(3))" -DCMAKE_BUILD_TYPE="$$($(3))" \
		$(if $(4),-DSMARTGRID_JUCE_ROOT="$$$$juce_root") $$(CMAKE_CACHE_ARGS) $$(CMAKE_ARGS)

$(1)-build: $(1)-configure
	$$(call cmake_build,$(REPO_ROOT)/build/$(1)/$$($(3)),$$($(3)))

$(1)-test: $(1)-build
	ctest --test-dir "$(REPO_ROOT)/build/$(1)/$$($(3))" --build-config "$$($(3))" --output-on-failure
endef

$(eval $(call cmake_targets,core,private/test,CORE_CONFIG))
$(eval $(call cmake_targets,visualizers,private/test/juce-visualizers,VISUALIZERS_CONFIG,juce))
$(eval $(call cmake_targets,midi,private/test/juce-midi,MIDI_CONFIG,juce))

.PHONY: lean-setup lean-cache lean-test lean-dsp-test lean-dsp-codegen lean-dsp-generate lean-dsp-codegen-test
lean-setup:
	"$(REPO_ROOT)/scripts/lean.sh" setup

lean-cache:
	"$(REPO_ROOT)/scripts/lean.sh" exe cache get \
		Mathlib.Algebra.BigOperators.Field \
		Mathlib.Algebra.Polynomial.Degree.Support \
		Mathlib.Analysis.Calculus.Deriv.MeanValue \
		Mathlib.Analysis.Calculus.Deriv.Mul \
		Mathlib.Analysis.Calculus.Deriv.Prod \
		Mathlib.Analysis.Calculus.Deriv.Shift \
		Mathlib.Analysis.Complex.RealDeriv \
		Mathlib.Analysis.Normed.Algebra.Exponential \
		Mathlib.Analysis.Normed.Module.FiniteDimension \
		Mathlib.Analysis.SpecialFunctions.ExpDeriv \
		Mathlib.Analysis.SpecialFunctions.Exponential \
		Mathlib.Analysis.SpecialFunctions.Pow.Real \
		Mathlib.Analysis.SpecialFunctions.Trigonometric.Basic \
		Mathlib.Analysis.SpecialFunctions.Trigonometric.Bounds \
		Mathlib.Data.Complex.Basic \
		Mathlib.Data.Fin.VecNotation \
		Mathlib.Data.Nat.Basic \
		Mathlib.Data.Real.Basic \
		Mathlib.FieldTheory.RatFunc.Basic \
		Mathlib.LinearAlgebra.Matrix.Charpoly.Coeff \
		Mathlib.LinearAlgebra.Matrix.NonsingularInverse \
		Mathlib.LinearAlgebra.Matrix.ToLin \
		Mathlib.MeasureTheory.Integral.IntervalIntegral.FundThmCalculus \
		Mathlib.RingTheory.LaurentSeries \
		Mathlib.RingTheory.PowerSeries.Basic \
		Mathlib.RingTheory.PowerSeries.Inverse \
		Mathlib.RingTheory.PowerSeries.NoZeroDivisors \
		Mathlib.Tactic.DeriveTraversable \
		Mathlib.Tactic.FieldSimp \
		Mathlib.Tactic.Linarith \
		Mathlib.Tactic.LinearCombination \
		Mathlib.Tactic.NormNum \
		Mathlib.Tactic.Ring \
		Mathlib.Tactic.SplitIfs

DSP_RUNTIME_MODULES := DSP.Filters.Capture DSP.Filters.OnePoleZOHLowPass.Response DSP.Filters.OnePoleTPTLowPass \
	DSP.Filters.OnePoleTPTHighPass DSP.Filters.TPTBaseWidth DSP.Filters.TPTSVF \
	DSP.Filters.MorphingTPTSVF DSP.Filters.TPTSVF4Pole DSP.Filters.TPTLadder \
	DSP.Filters.LinkwitzRileySplitter DSP.Filters.Coupled \
	DSP.Filters.ConservativeReuppingLadder.Response DSP.Filters.MultibandSaturator.Runtime DSP.Filters.Runtime \
	DSP.Filters.DelayKernels.Runtime DSP.Codegen.DelayBuffer DSP.Codegen.QuadReverb DSP.Codegen.SmoothRandomLFO DSP.SmoothRandomLFO
DSP_HEADERS := OnePoleZOHLowPass OnePoleTPTLowPass OnePoleTPTHighPass TPTBaseWidth TPTSVF \
	MorphingTPTSVF TPTSVF4PoleHighPass TPTSVF4PoleLowPass TPTSVF4PoleBandPass TPTLadder \
	CoupledProcessor ConservativeReuppingLadderCore TPTSVFLowPass TPTSVFHighPass TPTSVFBandPass TPTSVFNotch TPTSVFAllPass

DSP_GENERIC_HEADERS := $(addsuffix Generic,$(DSP_HEADERS)) LinkwitzRileySplitter4BandGeneric MultibandSaturator4BandCoreGeneric \
	CircularBufferGeneric SchroederAllPassCoreGeneric TunedDecayCombCoreGeneric \
	TunedDecayCombTuningGeneric TunedDecayCombResponseGeneric SmoothRandomLFOPrepareGeneric SmoothRandomLFOCubicGeneric SmoothRandomLFOStreamGeneric SquiggleBoyQuadReverb

lean-dsp-codegen:
	"$(REPO_ROOT)/scripts/lean.sh" build DSP.Codegen.Cpp $(DSP_RUNTIME_MODULES)
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/GenerateDSP.lean"

lean-dsp-generate: lean-dsp-codegen
	mkdir -p "$(REPO_ROOT)/formal/DSP/generated"
	for header in $(DSP_HEADERS) $(DSP_GENERIC_HEADERS); do \
		cp "$(REPO_ROOT)/build/lean/generated/$$header.hpp" "$(REPO_ROOT)/formal/DSP/generated/$$header.hpp" || exit 1; \
	done

lean-dsp-codegen-test: lean-dsp-codegen
	"$(REPO_ROOT)/scripts/lean.sh" build DSP.Tests.CodegenFixtures dsp_codegen_reference dsp_multiband_reference dsp_delay_codegen_reference
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/TestDSPCodegen.lean"
	python3 "$(REPO_ROOT)/scripts/test_dsp_codegen.py"
	python3 "$(REPO_ROOT)/scripts/test_dsp_delay_codegen.py"

lean-dsp-test: lean-dsp-codegen-test
	"$(REPO_ROOT)/scripts/lean.sh" build DSP dsp_filter_smoke
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/DSP/Audit.lean"
	"$(REPO_ROOT)/scripts/lean.sh" exe dsp_filter_smoke

lean-test:
	python3 "$(REPO_ROOT)/scripts/check_encoder_spec.py"
	python3 "$(REPO_ROOT)/scripts/check_phase_spec.py"
	"$(REPO_ROOT)/scripts/lean.sh" build
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/DSP/Audit.lean"
	"$(REPO_ROOT)/scripts/lean.sh" exe dsp_filter_smoke
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/Encoder/Audit.lean"
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/PhaseTime/Audit.lean"
	$(MAKE) -f "$(REPO_ROOT)/Makefile" lean-dsp-codegen-test

PERF_DIR := $(REPO_ROOT)/build/tools/perf/Release
.PHONY: perf-configure perf-build perf-test perf-run perf-sgrec
perf-configure:
	cmake -S "$(REPO_ROOT)/private/test/perf" -B "$(PERF_DIR)" -DCMAKE_BUILD_TYPE=Release $(CMAKE_CACHE_ARGS) $(CMAKE_ARGS)

perf-build: perf-configure
	$(call cmake_build,$(PERF_DIR),Release)

.PHONY: perf-responses-build perf-responses
perf-responses-build: perf-configure
	$(call cmake_build,$(PERF_DIR),Release,--target perf_responses)

perf-responses: perf-responses-build
	"$(PERF_DIR)/perf_responses" $(PERF_ARGS)

.PHONY: perf-ladder-build perf-ladder
perf-ladder-build: perf-configure
	$(call cmake_build,$(PERF_DIR),Release,--target perf_ladder)

perf-ladder: perf-ladder-build
	"$(PERF_DIR)/perf_ladder" $(PERF_ARGS)

.PHONY: perf-comb-build perf-comb
perf-comb-build: perf-configure
	$(call cmake_build,$(PERF_DIR),Release,--target perf_comb)

perf-comb: perf-comb-build
	"$(PERF_DIR)/perf_comb" $(PERF_ARGS)

perf-test: perf-build
	ctest --test-dir "$(PERF_DIR)" --build-config Release --output-on-failure

perf-run: perf-build
	python3 "$(REPO_ROOT)/private/test/perf/run.py" --bin-dir "$(PERF_DIR)" $(PERF_ARGS)

PERF_IPAD_DIR := $(REPO_ROOT)/build/tools/perf-ipad/Release
PERF_TEAM ?= 84DCGNY7JA
.PHONY: perf-ipad-build perf-ipad
perf-ipad-build:
	juce_root=$$(python3 "$(REPO_ROOT)/scripts/build_cache.py" juce) && cmake -S "$(REPO_ROOT)/private/test/perf/device" -B "$(PERF_IPAD_DIR)" -G Xcode \
		-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=18.0 \
		-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$(PERF_TEAM)" -DSMARTGRID_JUCE_ROOT="$$juce_root"
	cmake --build "$(PERF_IPAD_DIR)" --config Release --parallel "$(JOBS)" -- -allowProvisioningUpdates

perf-ipad: perf-ipad-build
	@test -x "$(REPO_ROOT)/scripts/.venv-ios/bin/python" || $(MAKE) -C "$(REPO_ROOT)" ios-setup
	"$(REPO_ROOT)/scripts/.venv-ios/bin/python" "$(REPO_ROOT)/scripts/ipad_perf.py" $(PERF_ARGS)

perf-sgrec: perf-build
	python3 "$(REPO_ROOT)/private/test/perf/run_sgrec.py" --bin-dir "$(PERF_DIR)" $(SGREC_ARGS)
