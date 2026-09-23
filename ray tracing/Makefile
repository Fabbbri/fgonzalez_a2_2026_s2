CXX ?= g++
CPPFLAGS += -Isrc
CXXFLAGS ?= -O2 -g -std=c++17 -Wall -Wextra -Wpedantic
WITH_SDL ?= 1
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
BUILD := build/$(if $(filter 1,$(WITH_SDL)),sdl,headless)
CORE_SOURCES := src/geometry.cpp src/scene.cpp src/camera.cpp src/image.cpp src/tiles.cpp src/transport.cpp src/raytracer.cpp src/sequential.cpp
APP_SOURCES := src/main.cpp src/options.cpp src/display.cpp $(CORE_SOURCES)
APP_OBJECTS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(APP_SOURCES))
HEADERS := $(wildcard src/*.hpp)

ifeq ($(WITH_SDL),1)
CPPFLAGS += -DRT_WITH_SDL $(SDL_CFLAGS)
LDLIBS += $(SDL_LIBS)
endif

.PHONY: all build run test test-display clean
all: $(BUILD)/raytracer

build: all

run: $(BUILD)/raytracer
	./$(BUILD)/raytracer $(ARGS)

$(BUILD)/raytracer: $(APP_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/tests: tests/test_core.cpp $(CORE_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) -Isrc $(CXXFLAGS) tests/test_core.cpp $(CORE_SOURCES) -o $@

test: build/tests
	./build/tests

build/test-display: tests/test_display.cpp src/display.cpp $(CORE_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) -Isrc -DRT_WITH_SDL $(SDL_CFLAGS) $(CXXFLAGS) tests/test_display.cpp src/display.cpp $(CORE_SOURCES) $(SDL_LIBS) -o $@

test-display: build/test-display
	SDL_VIDEODRIVER=dummy ./build/test-display

clean:
	rm -rf build

-include $(wildcard $(BUILD)/*.d)
