JUCE_DIR := JUCE/SmartGridOne
JUCE_TARGETS := build clean run build-sanitized run-sanitized clang-tidy \
	ios-setup ios-build ios-install ios-deploy deploy-ios list-ios-devices

.PHONY: all $(JUCE_TARGETS)

all: build

$(JUCE_TARGETS):
	$(MAKE) -C $(JUCE_DIR) $@
