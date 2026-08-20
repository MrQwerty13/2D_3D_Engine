CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
CPPFLAGS := -Isrc
SDL_CFLAGS := $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl3 2>/dev/null)

CORE_SOURCES := src/room_engine/application.cpp
APP_SOURCES := $(CORE_SOURCES) src/main.cpp
TEST_SOURCES := $(CORE_SOURCES) tests/smoke_test.cpp

DEBUG_DIR := out/debug
RELEASE_DIR := out/release

.PHONY: all debug release test format clean

all: debug

debug:
	@mkdir -p $(DEBUG_DIR)
	$(CXX) $(CXXFLAGS) -O0 -g $(CPPFLAGS) $(SDL_CFLAGS) $(APP_SOURCES) $(SDL_LIBS) -o $(DEBUG_DIR)/room_engine_app
	$(CXX) $(CXXFLAGS) -O0 -g $(CPPFLAGS) $(TEST_SOURCES) -o $(DEBUG_DIR)/room_engine_smoke_tests

release:
	@mkdir -p $(RELEASE_DIR)
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG $(CPPFLAGS) $(SDL_CFLAGS) $(APP_SOURCES) $(SDL_LIBS) -o $(RELEASE_DIR)/room_engine_app
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG $(CPPFLAGS) $(TEST_SOURCES) -o $(RELEASE_DIR)/room_engine_smoke_tests

test: debug
	$(DEBUG_DIR)/room_engine_smoke_tests

format:
	clang-format -i src/main.cpp src/room_engine/application.cpp src/room_engine/application.hpp tests/smoke_test.cpp

clean:
	rm -rf out
