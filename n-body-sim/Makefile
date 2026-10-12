# make ejecuta el flujo completo; make build solo compila.
.DEFAULT_GOAL := run

CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
TARGET := build/nbody
# Física y captura compartidas; cada estrategia ocupa un único .cpp en models/.
CORE := src/common/physics.cpp src/common/trajectory.cpp
MODELS := $(wildcard src/models/*.cpp)
SOURCES := src/main.cpp src/viewer.cpp $(CORE) $(MODELS)
HEADER := src/nbody.hpp
PROJECT_CFLAGS := -Isrc -pthread
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

# En esta máquina se pueden usar los encabezados extraídos localmente y la
# biblioteca SDL2 de ejecución existente. En otras, se usa pkg-config.
ifeq ($(strip $(SDL_LIBS)),)
SDL_ROOT := .local/sdl2/usr
SDL_ARCH_INCLUDE := $(patsubst %/SDL2/_real_SDL_config.h,%,$(firstword $(wildcard $(SDL_ROOT)/include/*/SDL2/_real_SDL_config.h)))
ifneq ($(wildcard $(SDL_ROOT)/include/SDL2/SDL.h),)
SDL_CFLAGS := -I$(SDL_ROOT)/include/SDL2 -I$(SDL_ROOT)/include -I$(SDL_ARCH_INCLUDE)
SDL_LIBS := -l:libSDL2-2.0.so.0
endif
endif

.PHONY: run build benchmark report profile test clean

REPETITIONS ?= 200
BENCHMARK_MODELS ?= sequential fine-grained coarse-grained smt cmp
BENCHMARK_ARGS ?=
REPORT_INPUT ?= output
REPORT_OUTPUT ?= output/benchmark-report.html
PROFILE_MODELS ?= sequential fine-grained coarse-grained smt cmp
PROFILE_REPETITIONS ?= 3
PROFILE_ARGS ?=
PERF ?= perf
PERF_EVENTS ?= task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,cache-references,cache-misses
PYTHON ?= python3
ARGS ?=

run: $(TARGET)
	./$(TARGET) $(ARGS)

build: $(TARGET)

profile: $(TARGET)
	@set -e; for model in $(PROFILE_MODELS); do \
		PERF="$(PERF)" PERF_EVENTS="$(PERF_EVENTS)" \
		PROFILE_COMPILER="$(CXX)" PROFILE_FLAGS="$(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(LDFLAGS)" \
		bash scripts/profile.sh ./$(TARGET) "$(PROFILE_REPETITIONS)" --model "$$model" $(PROFILE_ARGS); \
	done

benchmark: $(TARGET)
	@set -e; mkdir -p output; \
		suite=$$(mktemp -d "output/benchmark-suite-$$(date +%Y%m%d-%H%M%S)-XXXXXX"); \
		printf 'Sesión de benchmark: %s\n' "$$suite"; \
		for model in $(BENCHMARK_MODELS); do \
		printf '\nBenchmark: %s (%s repeticiones)\n' "$$model" "$(REPETITIONS)"; \
		BENCHMARK_OUTPUT_ROOT="$$suite" bash scripts/benchmark.sh ./$(TARGET) "$(REPETITIONS)" "$(PYTHON)" --model "$$model" $(BENCHMARK_ARGS); \
	done; \
	$(PYTHON) scripts/report.py "$$suite" --output "$$suite/report.html"

report:
	$(PYTHON) scripts/report.py "$(REPORT_INPUT)" --output "$(REPORT_OUTPUT)"

$(TARGET): $(SOURCES) $(HEADER)
	@test -n "$(SDL_LIBS)" || { printf 'Falta SDL2: instale libsdl2-dev y pkg-config (Linux), o sdl2 y pkg-config con Homebrew (macOS).\n'; exit 1; }
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(SDL_CFLAGS) $(SOURCES) $(LDFLAGS) $(SDL_LIBS) $(LDLIBS) -o $(TARGET)

build/test_physics: tests/test_physics.cpp src/common/physics.cpp $(MODELS) $(HEADER)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) tests/test_physics.cpp src/common/physics.cpp $(MODELS) $(LDFLAGS) $(LDLIBS) -o $@

build/test_native: tests/test_native.cpp src/viewer.cpp $(CORE) $(MODELS) $(HEADER)
	@test -n "$(SDL_LIBS)" || { printf 'Falta SDL2: instale libsdl2-dev y pkg-config (Linux), o sdl2 y pkg-config con Homebrew (macOS).\n'; exit 1; }
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(SDL_CFLAGS) tests/test_native.cpp src/viewer.cpp $(CORE) $(MODELS) $(LDFLAGS) $(SDL_LIBS) $(LDLIBS) -o $@

test: $(TARGET) build/test_physics build/test_native
	./build/test_physics
	SDL_VIDEODRIVER=dummy ./build/test_native
	bash tests/test_cli.sh ./$(TARGET)
	bash tests/test_profile.sh
	$(PYTHON) tests/test_analysis.py
	$(PYTHON) tests/test_report.py

clean:
	rm -f $(TARGET) build/test_physics build/test_native
