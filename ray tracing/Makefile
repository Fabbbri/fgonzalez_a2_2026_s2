CXX ?= g++
CPPFLAGS += -Isrc
CXXFLAGS ?= -O2 -g -std=c++17 -Wall -Wextra -Wpedantic
CXXFLAGS += -pthread
WITH_SDL ?= 1
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
BUILD := build/$(if $(filter 1,$(WITH_SDL)),sdl,headless)
CORE_SOURCES := \
    src/scene/geometry.cpp src/scene/scene.cpp src/scene/camera.cpp \
    src/render/image.cpp src/render/tiles.cpp src/render/transport.cpp src/render/raytracer.cpp \
    src/execution/sequential.cpp src/execution/fine.cpp src/execution/coarse.cpp \
    src/execution/cpu.cpp src/execution/parallel.cpp src/metrics/metrics.cpp
APP_SOURCES := src/main.cpp src/app/options.cpp src/app/display.cpp \
    src/metrics/benchmark.cpp $(CORE_SOURCES)
APP_OBJECTS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(APP_SOURCES))
HEADERS := $(wildcard src/*.hpp src/*/*.hpp)
PYTHON ?= python3
CSV ?= results/metrics.csv
PLOT_DIR ?= results/plots
PERF ?= perf
PROFILE_REPETITIONS ?= 5
PROFILE_SCOPE ?= process
PROFILE_EVENTS ?= task-clock,context-switches,cpu-migrations,cycles,instructions,cache-misses
PROFILE_DIR ?= results/perf
PROFILE_CAPTURE ?=

ifeq ($(WITH_SDL),1)
CPPFLAGS += -DRT_WITH_SDL $(SDL_CFLAGS)
LDLIBS += $(SDL_LIBS)
endif

.PHONY: all build run benchmark plots benchmark-plots profile profile-plots test test-plots test-profile test-display clean
all: $(BUILD)/raytracer

build: all

run: $(BUILD)/raytracer
	./$(BUILD)/raytracer $(ARGS)

benchmark: $(BUILD)/raytracer
	$(PYTHON) scripts/benchmark_plots.py --csv "$(CSV)" --plot-dir "$(PLOT_DIR)" ./$(BUILD)/raytracer $(ARGS)

plots:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)"

test-plots:
	$(PYTHON) tests/test_plot_metrics.py

# Compatibilidad: benchmark ya mide y grafica, fuera del intervalo medido.
benchmark-plots: benchmark

profile: $(BUILD)/raytracer
	$(PYTHON) scripts/profile_perf.py --perf "$(PERF)" --scope "$(PROFILE_SCOPE)" --events "$(PROFILE_EVENTS)" --repetitions "$(PROFILE_REPETITIONS)" --output-dir "$(PROFILE_DIR)" ./$(BUILD)/raytracer $(ARGS)

test-profile:
	$(PYTHON) tests/test_profile_perf.py

profile-plots:
	$(PYTHON) scripts/plot_profile.py "$(PROFILE_CAPTURE)"

$(BUILD)/raytracer: $(APP_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/tests: tests/test_execution.cpp src/app/options.cpp src/metrics/benchmark.cpp $(CORE_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) -Isrc $(CXXFLAGS) tests/test_execution.cpp src/app/options.cpp src/metrics/benchmark.cpp $(CORE_SOURCES) -o $@

test: build/tests
	./build/tests

build/test-display: tests/test_display.cpp src/app/display.cpp $(CORE_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) -Isrc -DRT_WITH_SDL $(SDL_CFLAGS) $(CXXFLAGS) tests/test_display.cpp src/app/display.cpp $(CORE_SOURCES) $(SDL_LIBS) -o $@

test-display: build/test-display
	SDL_VIDEODRIVER=dummy ./build/test-display

clean:
	rm -rf build

-include $(APP_OBJECTS:.o=.d)
