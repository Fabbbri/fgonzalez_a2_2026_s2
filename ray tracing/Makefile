CXX ?= g++
CPPFLAGS += -Isrc
CXXFLAGS ?= -O2 -g -std=c++17 -Wall -Wextra -Wpedantic
CXXFLAGS += -pthread
WITH_SDL ?= 1
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
BUILD := build/$(if $(filter 1,$(WITH_SDL)),sdl,headless)
CORE_SOURCES := src/geometry.cpp src/scene.cpp src/camera.cpp src/image.cpp src/tiles.cpp src/transport.cpp src/raytracer.cpp src/sequential.cpp src/fine.cpp src/coarse.cpp src/cpu.cpp src/parallel.cpp src/metrics.cpp
APP_SOURCES := src/main.cpp src/options.cpp src/display.cpp src/benchmark.cpp $(CORE_SOURCES)
APP_OBJECTS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(APP_SOURCES))
HEADERS := $(wildcard src/*.hpp)
PYTHON ?= python3
CSV ?= results/metrics.csv
PLOT_DIR ?= results/plots
PERF ?= perf
PROFILE_REPETITIONS ?= 5
PROFILE_SCOPE ?= process
PROFILE_EVENTS ?= task-clock,context-switches,cpu-migrations,cycles,instructions,cache-misses
PROFILE_DIR ?= results/perf

ifeq ($(WITH_SDL),1)
CPPFLAGS += -DRT_WITH_SDL $(SDL_CFLAGS)
LDLIBS += $(SDL_LIBS)
endif

.PHONY: all build run benchmark plots benchmark-plots profile test test-plots test-profile test-display clean
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

$(BUILD)/raytracer: $(APP_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/tests: tests/test_execution.cpp src/options.cpp src/benchmark.cpp $(CORE_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) -Isrc $(CXXFLAGS) tests/test_execution.cpp src/options.cpp src/benchmark.cpp $(CORE_SOURCES) -o $@

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
