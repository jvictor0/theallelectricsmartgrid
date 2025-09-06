APP_NAME = SmartGridOne
SCHEME = SmartGridOne - App
CONFIG = Release
PROJECT = Builds/MacOSX/$(APP_NAME).xcodeproj
BUILD_DIR = Builds/MacOSX/build

.PHONY: all build clean run

all: build

build:
	xcodebuild -project $(PROJECT) -scheme "$(SCHEME)" -configuration $(CONFIG)

clean:
	xcodebuild -project $(PROJECT) -scheme "$(SCHEME)" -configuration $(CONFIG) clean

run: build
	$(BUILD_DIR)/$(CONFIG)/$(APP_NAME).app/Contents/MacOS/$(APP_NAME)
