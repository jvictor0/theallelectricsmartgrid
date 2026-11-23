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

.PHONY: ladder-periodic-grid-build ladder-periodic-grid
LADDER_GRID_LIBRARY := $(REPO_ROOT)/build/tools/ladder-periodic-grid/libladder_periodic_grid.so
$(LADDER_GRID_LIBRARY): $(REPO_ROOT)/scripts/ladder_periodic_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fPIC -shared -ffp-contract=off "$<" -o "$@"

ladder-periodic-grid-build: $(LADDER_GRID_LIBRARY)

ladder-periodic-grid: ladder-periodic-grid-build
	OPENBLAS_NUM_THREADS=1 python3 "$(REPO_ROOT)/scripts/analyze_ladder_periodic_grid.py" $(LADDER_GRID_ARGS)

.PHONY: ladder-energy-grid-build
LADDER_ENERGY_BINARY := $(REPO_ROOT)/build/tools/ladder-energy-grid/ladder-energy-grid
$(LADDER_ENERGY_BINARY): $(REPO_ROOT)/scripts/ladder_energy_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -pthread -framework Accelerate "$<" -o "$@"

ladder-energy-grid-build: $(LADDER_ENERGY_BINARY)

.PHONY: ladder-gpu-bench-build
LADDER_GPU_BINARY := $(REPO_ROOT)/build/tools/ladder-gpu-bench/ladder-gpu-bench
$(LADDER_GPU_BINARY): $(REPO_ROOT)/scripts/ladder_gpu_bench.mm $(REPO_ROOT)/scripts/ladder_gpu_host.hpp $(REPO_ROOT)/scripts/ladder_gpu_common.hpp $(REPO_ROOT)/scripts/ladder_gpu.metal $(REPO_ROOT)/scripts/ladder_energy_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -fobjc-arc -pthread -framework Foundation -framework Metal -framework Accelerate "$<" -o "$@"

ladder-gpu-bench-build: $(LADDER_GPU_BINARY)

.PHONY: ladder-gpu-grid-build
LADDER_GPU_GRID_BINARY := $(REPO_ROOT)/build/tools/ladder-gpu-grid/ladder-gpu-grid
$(LADDER_GPU_GRID_BINARY): $(REPO_ROOT)/scripts/ladder_gpu_grid.mm $(REPO_ROOT)/scripts/ladder_gpu_host.hpp $(REPO_ROOT)/scripts/ladder_gpu_common.hpp $(REPO_ROOT)/scripts/ladder_gpu.metal $(REPO_ROOT)/scripts/ladder_energy_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -fobjc-arc -pthread -framework Foundation -framework Metal -framework Accelerate "$<" -o "$@"

ladder-gpu-grid-build: $(LADDER_GPU_GRID_BINARY)

.PHONY: ladder-gpu-long-build
LADDER_GPU_LONG_BINARY := $(REPO_ROOT)/build/tools/ladder-gpu-long/ladder-gpu-long
$(LADDER_GPU_LONG_BINARY): $(REPO_ROOT)/scripts/ladder_gpu_long.mm $(REPO_ROOT)/scripts/ladder_gpu_long.metal $(REPO_ROOT)/scripts/ladder_gpu_host.hpp $(REPO_ROOT)/scripts/ladder_gpu_common.hpp $(REPO_ROOT)/scripts/ladder_energy_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -fobjc-arc -pthread -framework Foundation -framework Metal -framework Accelerate "$<" -o "$@"

ladder-gpu-long-build: $(LADDER_GPU_LONG_BINARY)

.PHONY: ladder-gpu-capture-build
LADDER_GPU_CAPTURE_BINARY := $(REPO_ROOT)/build/tools/ladder-gpu-capture/ladder-gpu-capture
$(LADDER_GPU_CAPTURE_BINARY): $(REPO_ROOT)/scripts/ladder_gpu_capture.mm $(REPO_ROOT)/scripts/ladder_gpu_capture.metal $(REPO_ROOT)/scripts/ladder_gpu_long.mm $(REPO_ROOT)/scripts/ladder_gpu_long.metal $(REPO_ROOT)/scripts/ladder_gpu_host.hpp $(REPO_ROOT)/scripts/ladder_gpu_common.hpp $(REPO_ROOT)/scripts/ladder_energy_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -fobjc-arc -pthread -framework Foundation -framework Metal -framework Accelerate "$<" -o "$@"

ladder-gpu-capture-build: $(LADDER_GPU_CAPTURE_BINARY)

.PHONY: ladder-gpu-basins-build ladder-rational-period-build
LADDER_GPU_BASINS_BINARY := $(REPO_ROOT)/build/tools/ladder-gpu-basins/ladder-gpu-basins
$(LADDER_GPU_BASINS_BINARY): $(REPO_ROOT)/scripts/ladder_gpu_basins.mm $(REPO_ROOT)/scripts/ladder_gpu_host.hpp $(REPO_ROOT)/scripts/ladder_gpu_common.hpp $(REPO_ROOT)/scripts/ladder_gpu.metal $(REPO_ROOT)/scripts/ladder_energy_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -fobjc-arc -pthread -framework Foundation -framework Metal -framework Accelerate "$<" -o "$@"

ladder-gpu-basins-build: $(LADDER_GPU_BASINS_BINARY)

LADDER_RATIONAL_LIBRARY := $(REPO_ROOT)/build/tools/ladder-rational-period/libladder_rational_period.so
$(LADDER_RATIONAL_LIBRARY): $(REPO_ROOT)/scripts/ladder_rational_period.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -shared -fPIC "$<" -o "$@"

ladder-rational-period-build: $(LADDER_RATIONAL_LIBRARY)

LADDER_BREAK_LIBRARY := $(REPO_ROOT)/build/tools/ladder-break-replay/libladder_break_replay.so
$(LADDER_BREAK_LIBRARY): $(REPO_ROOT)/scripts/ladder_break_replay.mm $(REPO_ROOT)/scripts/ladder_gpu_host.hpp $(REPO_ROOT)/scripts/ladder_gpu_common.hpp $(REPO_ROOT)/scripts/ladder_energy_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -fobjc-arc -shared -fPIC -framework Foundation -framework Metal -framework Accelerate "$<" -o "$@"

.PHONY: ladder-break-replay-build
ladder-break-replay-build: $(LADDER_BREAK_LIBRARY) $(LADDER_RATIONAL_LIBRARY)

.PHONY: ladder-gpu-continuation-build
LADDER_CONTINUATION_BINARY := $(REPO_ROOT)/build/tools/ladder-gpu-continuation/ladder-gpu-continuation
$(LADDER_CONTINUATION_BINARY): $(REPO_ROOT)/scripts/ladder_gpu_continuation.mm $(REPO_ROOT)/scripts/ladder_continuation_common.hpp $(REPO_ROOT)/scripts/ladder_continuation.metal $(REPO_ROOT)/scripts/ladder_gpu_host.hpp $(REPO_ROOT)/scripts/ladder_gpu_common.hpp $(REPO_ROOT)/scripts/ladder_energy_grid.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -fobjc-arc -pthread -framework Foundation -framework Metal -framework Accelerate "$<" -o "$@"

ladder-gpu-continuation-build: $(LADDER_CONTINUATION_BINARY)

.PHONY: ladder-survey-sql-build
LADDER_SQL_VERIFY := $(REPO_ROOT)/build/tools/ladder-survey-sql/libladder_survey_verify.so
$(LADDER_SQL_VERIFY): $(REPO_ROOT)/scripts/ladder_survey_verify.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -shared -fPIC -pthread "$<" -o "$@"

ladder-survey-sql-build: $(LADDER_SQL_VERIFY)

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
	DSP.Filters.LinkwitzRileySplitter DSP.Filters.Coupled DSP.Filters.NaiveReuppingLadder
DSP_HEADERS := OnePoleZOHLowPass OnePoleTPTLowPass OnePoleTPTHighPass TPTBaseWidth TPTSVF \
	MorphingTPTSVF TPTSVF4PoleHighPass TPTSVF4PoleLowPass TPTSVF4PoleBandPass TPTLadder \
	CoupledProcessor NormalizedTanh BandPassedEnvelopeFollowerCore NaiveReuppingLadderCore

DSP_GENERIC_HEADERS := $(addsuffix Generic,$(DSP_HEADERS)) LinkwitzRileySplitter4BandGeneric

lean-dsp-codegen:
	"$(REPO_ROOT)/scripts/lean.sh" build DSP.Codegen.Cpp $(DSP_RUNTIME_MODULES)
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/GenerateDSP.lean"

lean-dsp-generate: lean-dsp-codegen
	mkdir -p "$(REPO_ROOT)/formal/DSP/generated"
	for header in $(DSP_HEADERS) $(DSP_GENERIC_HEADERS); do \
		cp "$(REPO_ROOT)/build/lean/generated/$$header.hpp" "$(REPO_ROOT)/formal/DSP/generated/$$header.hpp" || exit 1; \
	done

lean-dsp-codegen-test: lean-dsp-codegen
	"$(REPO_ROOT)/scripts/lean.sh" build DSP.Tests.CodegenFixtures dsp_codegen_reference
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/TestDSPCodegen.lean"
	python3 "$(REPO_ROOT)/scripts/test_dsp_codegen.py"

lean-dsp-test: lean-dsp-codegen-test
	"$(REPO_ROOT)/scripts/lean.sh" build DSP dsp_filter_smoke
	"$(REPO_ROOT)/scripts/lean.sh" env lean -DwarningAsError=true "$(REPO_ROOT)/formal/DSP/Audit.lean"
	"$(REPO_ROOT)/scripts/lean.sh" exe dsp_filter_smoke

.PHONY: lean-dsp-floor-free-probe
lean-dsp-floor-free-probe:
	"$(REPO_ROOT)/scripts/lean.sh" build dsp_floor_free_probe
	"$(REPO_ROOT)/scripts/lean.sh" exe dsp_floor_free_probe $(FLOOR_FREE_PROBE_ARGS)

.PHONY: lean-dsp-finite-level-probe
lean-dsp-finite-level-probe:
	"$(REPO_ROOT)/scripts/lean.sh" build dsp_finite_level_probe
	"$(REPO_ROOT)/scripts/lean.sh" exe dsp_finite_level_probe $(FINITE_LEVEL_PROBE_ARGS)

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
	cmake -S "$(REPO_ROOT)/private/test/perf" -B "$(PERF_DIR)" -DCMAKE_BUILD_TYPE=Release $(CMAKE_ARGS)

perf-build: perf-configure
	cmake --build "$(PERF_DIR)" --config Release --parallel "$(JOBS)"

.PHONY: perf-responses-build perf-responses
perf-responses-build: perf-configure
	cmake --build "$(PERF_DIR)" --config Release --target perf_responses --parallel "$(JOBS)"

perf-responses: perf-responses-build
	"$(PERF_DIR)/perf_responses" $(PERF_ARGS)

.PHONY: perf-ladder-build perf-ladder
perf-ladder-build: perf-configure
	cmake --build "$(PERF_DIR)" --config Release --target perf_ladder --parallel "$(JOBS)"

perf-ladder: perf-ladder-build
	"$(PERF_DIR)/perf_ladder" $(PERF_ARGS)

perf-test: perf-build
	ctest --test-dir "$(PERF_DIR)" --build-config Release --output-on-failure

perf-run: perf-build
	python3 "$(REPO_ROOT)/private/test/perf/run.py" --bin-dir "$(PERF_DIR)" $(PERF_ARGS)

PERF_IPAD_DIR := $(REPO_ROOT)/build/tools/perf-ipad/Release
PERF_TEAM ?= 84DCGNY7JA
.PHONY: perf-ipad-build perf-ipad
perf-ipad-build:
	cmake -S "$(REPO_ROOT)/private/test/perf/device" -B "$(PERF_IPAD_DIR)" -G Xcode \
		-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=18.0 \
		-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$(PERF_TEAM)"
	cmake --build "$(PERF_IPAD_DIR)" --config Release --parallel "$(JOBS)" -- -allowProvisioningUpdates

perf-ipad: perf-ipad-build
	@test -x "$(REPO_ROOT)/scripts/.venv-ios/bin/python" || $(MAKE) -C "$(REPO_ROOT)" ios-setup
	"$(REPO_ROOT)/scripts/.venv-ios/bin/python" "$(REPO_ROOT)/scripts/ipad_perf.py" $(PERF_ARGS)

perf-sgrec: perf-build
	python3 "$(REPO_ROOT)/private/test/perf/run_sgrec.py" --bin-dir "$(PERF_DIR)" $(SGREC_ARGS)
