SHELL := /bin/sh

CXX := clang++
PKG_CONFIG ?= pkg-config
CLANG_FORMAT ?= clang-format

CONFIG ?= Debug
VALID_CONFIGS := Debug Release
ifeq ($(filter $(CONFIG),$(VALID_CONFIGS)),)
$(error CONFIG must be one of: $(VALID_CONFIGS))
endif

SDL3_PKG ?= sdl3
SDL3_VERSION ?= 3.4.14

CPPFLAGS := -Isrc
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
DEPFLAGS := -MMD -MP
LDFLAGS :=
LDLIBS :=

ifeq ($(CONFIG),Debug)
CONFIG_FLAGS := -O0 -g3
else
CONFIG_FLAGS := -O2 -DNDEBUG
endif

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(SDL3_PKG) 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs $(SDL3_PKG) 2>/dev/null)
BGFX_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags bgfx 2>/dev/null)
BGFX_LIBS ?= $(shell $(PKG_CONFIG) --libs bgfx 2>/dev/null)
BGFX_PLATFORM_LIBS :=
ifeq ($(shell uname -s),Darwin)
BGFX_PLATFORM_LIBS += -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreMedia -framework VideoToolbox
endif
ifneq ($(strip $(BGFX_CFLAGS)$(BGFX_LIBS)),)
CPPFLAGS += -DROOM_ENGINE_USE_BGFX $(BGFX_CFLAGS)
endif
BUILD_DIR := out/$(shell printf '%s' $(CONFIG) | tr '[:upper:]' '[:lower:]')

CORE_SOURCES := src/room_engine/application.cpp src/room_engine/renderer/renderer.cpp src/room_engine/renderer/debug_draw.cpp
APP_SOURCES := $(CORE_SOURCES) src/main.cpp
TEST_SOURCES := src/room_engine/renderer/renderer.cpp src/room_engine/renderer/debug_draw.cpp tests/smoke_test.cpp
FORMAT_SOURCES := $(APP_SOURCES) src/room_engine/application.hpp src/room_engine/renderer/renderer.hpp src/room_engine/renderer/camera.hpp src/room_engine/renderer/math.hpp src/room_engine/renderer/debug_draw.hpp tests/smoke_test.cpp
APP_OBJECTS := $(APP_SOURCES:%.cpp=$(BUILD_DIR)/%.o)
TEST_OBJECTS := $(TEST_SOURCES:%.cpp=$(BUILD_DIR)/%.o)
APP := $(BUILD_DIR)/room_engine_app
TEST := $(BUILD_DIR)/room_engine_smoke_tests

.PHONY: all debug release build test check format format-check clean help verify-tools verify-sdl

all: debug

debug:
	$(MAKE) CONFIG=Debug build

release:
	$(MAKE) CONFIG=Release build

build: verify-tools verify-sdl $(APP) $(TEST)

$(APP): $(APP_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CONFIG_FLAGS) $(CXXFLAGS) $(LDFLAGS) $^ $(SDL_LIBS) $(BGFX_LIBS) $(BGFX_PLATFORM_LIBS) $(LDLIBS) -o $@

$(TEST): $(TEST_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CONFIG_FLAGS) $(CXXFLAGS) $(LDFLAGS) $^ $(SDL_LIBS) $(BGFX_LIBS) $(BGFX_PLATFORM_LIBS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CONFIG_FLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) $(SDL_CFLAGS) -c $< -o $@

test: CONFIG=Debug
test: $(TEST)
	$(TEST)

check: test format-check

format:
	$(CLANG_FORMAT) -i $(FORMAT_SOURCES)

format-check:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo "error: clang-format is required" >&2; exit 1; }
	@$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SOURCES)

verify-tools:
	@command -v $(CXX) >/dev/null 2>&1 || { echo "error: $(CXX) not found" >&2; exit 1; }
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo "error: $(PKG_CONFIG) not found" >&2; exit 1; }

verify-sdl:
	@$(PKG_CONFIG) --exists $(SDL3_PKG) || { echo "error: SDL3 was not found through $(PKG_CONFIG)" >&2; exit 1; }
	@test "$$($(PKG_CONFIG) --modversion $(SDL3_PKG))" = "$(SDL3_VERSION)" || { \
		echo "error: expected SDL3 $(SDL3_VERSION), found $$($(PKG_CONFIG) --modversion $(SDL3_PKG))" >&2; \
		echo "       set SDL3_VERSION=... only when intentionally updating the lock" >&2; exit 1; }

clean:
	rm -rf out

help:
	@echo "make [debug|release|test|check|format|clean]"
	@echo "  CONFIG=Debug|Release selects out/debug or out/release"

-include $(APP_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)
