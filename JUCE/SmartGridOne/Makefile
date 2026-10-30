APP_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(APP_DIR)/../..)
APP_NAME = SmartGridOne
SCHEME = SmartGridOne - App
CONFIG = Release
PROJECT = $(APP_DIR)/Builds/MacOSX/$(APP_NAME).xcodeproj
BUILD_DIR = $(REPO_ROOT)/build/xcode/macos
APP = $(BUILD_DIR)/$(CONFIG)/Products/$(APP_NAME).app
XCODEBUILD ?= xcodebuild
XCODE_FLAGS ?=

IOS_PROJECT = $(APP_DIR)/Builds/iOS/$(APP_NAME).xcodeproj
IOS_BUILD_DIR = $(REPO_ROOT)/build/xcode/ios
IOS_APP = $(IOS_BUILD_DIR)/$(CONFIG)/Products/$(APP_NAME).app
IOS_VENV = $(REPO_ROOT)/scripts/.venv-ios
IOS_PYTHON = $(IOS_VENV)/bin/python
IOS_REQUIREMENTS = $(REPO_ROOT)/scripts/requirements-ios.txt
IOS_DEVICE ?=
IPAD_HOST ?=

.PHONY: all juce-setup juce-deps build clean run build-sanitized run-sanitized clang-tidy ios-setup ios-build ios-install ios-deploy deploy-ios list-ios-devices

all: build

juce-setup:
	git -C "$(REPO_ROOT)" submodule update --init --recursive -- third_party/JUCE

juce-deps:
	@test -f "$(REPO_ROOT)/third_party/JUCE/modules/juce_core/juce_core.h" || $(MAKE) -C "$(APP_DIR)" juce-setup

build build-sanitized clang-tidy ios-build: juce-deps

define xcode_build
	$(XCODEBUILD) -project "$(1)" -scheme "$(SCHEME)" -configuration "$(3)" \
		-destination "generic/platform=$(4)" -allowProvisioningUpdates \
		-derivedDataPath "$(2)/$(3)/DerivedData" \
		"SYMROOT=$(2)/$(3)/Products" "OBJROOT=$(2)/$(3)/Intermediates" \
		"CONFIGURATION_BUILD_DIR=$(2)/$(3)/Products" \
		"SHARED_PRECOMPS_DIR=$(2)/$(3)/Intermediates/PrecompiledHeaders" \
		$(XCODE_FLAGS) build
endef

build:
	$(call xcode_build,$(PROJECT),$(BUILD_DIR),$(CONFIG),macOS)

clean:
	python3 "$(REPO_ROOT)/scripts/clean_builds.py"

run: build
	"$(APP)/Contents/MacOS/$(APP_NAME)"

run-sanitized: build-sanitized
	"$(BUILD_DIR)/Debug/Products/$(APP_NAME).app/Contents/MacOS/$(APP_NAME)"

build-sanitized:
	$(call xcode_build,$(PROJECT),$(BUILD_DIR),Debug,macOS)

clang-tidy:
	mkdir -p "$(REPO_ROOT)/build/clang-tidy"
	cd "$(APP_DIR)" && clang-tidy Source/MainComponent.cpp Source/IOUtils.cpp \
		--checks='-*,cppcoreguidelines-pro-type-member-init' \
		--header-filter='private/src/.*' \
		-- -std=c++17 \
		-I"$(APP_DIR)/JuceLibraryCode" \
		-I"$(REPO_ROOT)/private/src" \
		-I"$(REPO_ROOT)/third_party/JUCE/modules" \
		-I/opt/homebrew/include \
		-isysroot $$(xcrun --sdk macosx --show-sdk-path) \
		-arch arm64 \
		-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
		-DJUCE_STANDALONE_APPLICATION=1 2>&1 | tee "$(REPO_ROOT)/build/clang-tidy/report.txt"

list-ios-devices:
	xcrun devicectl list devices

ios-setup:
	python3 -m venv "$(IOS_VENV)"
	"$(IOS_VENV)/bin/pip" install -r "$(IOS_REQUIREMENTS)"

ios-build:
	$(call xcode_build,$(IOS_PROJECT),$(IOS_BUILD_DIR),$(CONFIG),iOS)

ios-install:
	test -d "$(IOS_APP)" || (echo "Missing $(IOS_APP); run make ios-build first."; exit 1)
	@test -x "$(IOS_PYTHON)" || $(MAKE) -C "$(APP_DIR)" ios-setup
	IOS_DEVICE="$(IOS_DEVICE)" IPAD_HOST="$(IPAD_HOST)" "$(IOS_PYTHON)" "$(REPO_ROOT)/scripts/ios_deploy.py" --app "$(IOS_APP)" --install-only

ios-deploy: ios-build
	@test -x "$(IOS_PYTHON)" || $(MAKE) -C "$(APP_DIR)" ios-setup
	IOS_DEVICE="$(IOS_DEVICE)" IPAD_HOST="$(IPAD_HOST)" "$(IOS_PYTHON)" "$(REPO_ROOT)/scripts/ios_deploy.py" --app "$(IOS_APP)"

deploy-ios: ios-deploy
