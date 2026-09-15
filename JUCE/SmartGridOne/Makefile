APP_NAME = SmartGridOne
SCHEME = SmartGridOne - App
CONFIG = Release
PROJECT = Builds/MacOSX/$(APP_NAME).xcodeproj
BUILD_DIR = Builds/MacOSX/build

IOS_PROJECT = Builds/iOS/$(APP_NAME).xcodeproj
IOS_BUILD_DIR = Builds/iOS/build
IOS_APP = $(IOS_BUILD_DIR)/$(CONFIG)/$(APP_NAME).app
REPO_ROOT = ../..
IOS_VENV = $(REPO_ROOT)/scripts/.venv-ios
IOS_PYTHON = $(IOS_VENV)/bin/python
IOS_REQUIREMENTS = $(REPO_ROOT)/scripts/requirements-ios.txt
IOS_DEVICE ?=
IPAD_HOST ?=

.PHONY: all build clean run build-sanitized run-sanitized clang-tidy ios-setup ios-build ios-install ios-deploy deploy-ios list-ios-devices

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
		-I"$(shell pwd)/../../third_party/JUCE/modules" \
		-I/opt/homebrew/include \
		-isysroot $$(xcrun --sdk macosx --show-sdk-path) \
		-arch arm64 \
		-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
		-DJUCE_STANDALONE_APPLICATION=1 2>&1 | tee clang-tidy-report.txt

list-ios-devices:
	xcrun devicectl list devices

ios-setup:
	python3 -m venv $(IOS_VENV)
	$(IOS_VENV)/bin/pip install -r $(IOS_REQUIREMENTS)

ios-build:
	xcodebuild -project $(IOS_PROJECT) -scheme "$(SCHEME)" -configuration $(CONFIG) -destination "generic/platform=iOS" -allowProvisioningUpdates build

ios-install:
	@test -x "$(IOS_PYTHON)" || (echo "Missing iPad tools environment; run make ios-setup first."; exit 1)
	test -d "$(IOS_APP)" || (echo "Missing $(IOS_APP); run make ios-build first."; exit 1)
	IOS_DEVICE="$(IOS_DEVICE)" IPAD_HOST="$(IPAD_HOST)" $(IOS_PYTHON) $(REPO_ROOT)/scripts/ios_deploy.py --app "$(IOS_APP)" --install-only

ios-deploy: ios-build
	@test -x "$(IOS_PYTHON)" || (echo "Missing iPad tools environment; run make ios-setup first."; exit 1)
	IOS_DEVICE="$(IOS_DEVICE)" IPAD_HOST="$(IPAD_HOST)" $(IOS_PYTHON) $(REPO_ROOT)/scripts/ios_deploy.py --app "$(IOS_APP)"

deploy-ios: ios-deploy
