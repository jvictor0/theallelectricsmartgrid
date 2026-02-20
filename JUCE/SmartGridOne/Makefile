APP_NAME = SmartGridOne
SCHEME = SmartGridOne - App
CONFIG = Release
PROJECT = Builds/MacOSX/$(APP_NAME).xcodeproj
BUILD_DIR = Builds/MacOSX/build

.PHONY: all build clean run build-sanitized run-sanitized clang-tidy

all: build

build:
	xcodebuild -project $(PROJECT) -scheme "$(SCHEME)" -configuration $(CONFIG) -destination "generic/platform=macOS" -allowProvisioningUpdates

clean:
	xcodebuild -project $(PROJECT) -scheme "$(SCHEME)" -configuration $(CONFIG) clean
	rm -rf $(BUILD_DIR)

run: build
	$(BUILD_DIR)/$(CONFIG)/$(APP_NAME).app/Contents/MacOS/$(APP_NAME)

run-sanitized: build-sanitized
	$(BUILD_DIR)/Debug/$(APP_NAME).app/Contents/MacOS/$(APP_NAME)

build-sanitized:
	xcodebuild -project $(PROJECT) -scheme "$(SCHEME)" -configuration Debug -destination "generic/platform=macOS" -allowProvisioningUpdates

clang-tidy:
	clang-tidy Source/MainComponent.cpp Source/IOUtils.cpp \
		--checks='-*,cppcoreguidelines-pro-type-member-init' \
		--header-filter='private/src/.*' \
		-- -std=c++17 \
		-I"$(shell pwd)/JuceLibraryCode" \
		-I"$(shell pwd)/../../private/src" \
		-I"$${HOME}/JUCE/modules" \
		-I/opt/homebrew/include \
		-isysroot $$(xcrun --sdk macosx --show-sdk-path) \
		-arch arm64 \
		-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
		-DJUCE_STANDALONE_APPLICATION=1 2>&1 | tee clang-tidy-report.txt
