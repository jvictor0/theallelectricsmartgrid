# Run from the repository root.
include Makefile
NOISE_GRID_DIR := $(REPO_ROOT)/build/tools/ladder-noise-grid
NOISE_GRID_SRC := $(REPO_ROOT)/scripts/ladder_noise_grid

.PHONY: noise-grid-build
noise-grid-build: $(NOISE_GRID_DIR)/stock.so $(NOISE_GRID_DIR)/metal.so

$(NOISE_GRID_DIR)/stock.so: $(NOISE_GRID_SRC)/stock.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -DNDEBUG -fno-fast-math -ffp-contract=off -fPIC -shared -MMD -MP -MF "$@.d" -I"$(REPO_ROOT)/private/src" -I"$(REPO_ROOT)/formal/DSP/generated" -I"$(REPO_ROOT)/build/core/RelWithDebInfo/generated" "$<" -o "$@"

$(NOISE_GRID_DIR)/metal.so: $(NOISE_GRID_SRC)/metal.mm
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fobjc-arc -fPIC -shared -framework Foundation -framework Metal "$<" -o "$@"

-include $(NOISE_GRID_DIR)/stock.so.d
