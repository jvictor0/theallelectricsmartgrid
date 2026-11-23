# Invoke from the worktree root.
include Makefile
BRANCH_DIR := $(REPO_ROOT)/build/tools/ladder-branch
.PHONY: ladder-branch-reference ladder-branch-metal
ladder-branch-reference: $(BRANCH_DIR)/reference.so
$(BRANCH_DIR)/reference.so: scripts/ladder_branch/reference.cpp
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O3 -fno-fast-math -ffp-contract=off -fPIC -shared -MMD -MP -MF "$@.d" -Iprivate/src -Iformal/DSP/generated -Ibuild/core/RelWithDebInfo/generated "$<" -o "$@"
-include $(BRANCH_DIR)/reference.so.d
ladder-branch-metal: $(BRANCH_DIR)/metal-return
$(BRANCH_DIR)/metal-return: scripts/ladder_branch/return.mm
	mkdir -p "$(@D)"
	$(CXX) -std=c++17 -O2 -fobjc-arc -framework Foundation -framework Metal "$<" -o "$@"
