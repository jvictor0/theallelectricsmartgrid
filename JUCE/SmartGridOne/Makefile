APP_NAME = SmartGridOne
SCHEME = SmartGridOne - App
CONFIG = Release
PROJECT = Builds/MacOSX/$(APP_NAME).xcodeproj
BUILD_DIR = Builds/MacOSX/build

IOS_PROJECT = Builds/iOS/$(APP_NAME).xcodeproj
IOS_BUILD_DIR = Builds/iOS/build
IOS_APP = $(IOS_BUILD_DIR)/$(CONFIG)/$(APP_NAME).app

# devicectl: first device with an active Core Device tunnel (typically USB). Override: make deploy-ios IOS_DEVICE=00008122-...
#
IOS_DEVICE ?= $(shell xcrun devicectl list devices --json-output - -q 2>/dev/null | python3 -c "import json,sys;d=json.load(sys.stdin);xs=[x['hardwareProperties']['udid'] for x in d.get('result',{}).get('devices',[]) if x.get('connectionProperties',{}).get('tunnelState')=='connected'];print(xs[0] if xs else '')")

.PHONY: all build clean run build-sanitized run-sanitized clang-tidy ios-build ios-install deploy-ios list-ios-devices

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

list-ios-devices:
	xcrun devicectl list devices

ios-build:
	@test -n "$(IOS_DEVICE)" || (echo "No tunnel-connected iOS device found. Connect USB, unlock, trust Mac, then retry or set IOS_DEVICE to the UDID from: make list-ios-devices"; exit 1)
	xcodebuild -project $(IOS_PROJECT) -scheme "$(SCHEME)" -configuration $(CONFIG) -destination "id=$(IOS_DEVICE)" -allowProvisioningUpdates build

ios-install:
	@test -n "$(IOS_DEVICE)" || (echo "No tunnel-connected iOS device found; set IOS_DEVICE or connect device."; exit 1)
	test -d "$(IOS_APP)" || (echo "Missing $(IOS_APP); run make ios-build first."; exit 1)
	xcrun devicectl device install app --device "$(IOS_DEVICE)" "$(IOS_APP)"

deploy-ios: ios-build ios-install
