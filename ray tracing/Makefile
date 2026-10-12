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
PROFILE_COUNT_MODE ?= observed
PROFILE_LOGS ?= 0
PROFILE_EVENTS ?= task-clock,context-switches,cpu-migrations,cycles,instructions,cache-misses
PROFILE_DIR ?= results/perf
PROFILE_CAPTURE ?=
PROFILE_COMPARE_DIR ?= $(RESULTS_DIR)/perf
# Control global de Linux, independiente del modelo --model smt.
# keep conserva el estado; on/off requieren sudo si hay que cambiarlo.
SMT ?= keep
REPETITIONS ?= 200
BASELINE_REPETITIONS ?= $(REPETITIONS)
THREAD_COUNTS ?= auto
SMT_THREAD_COUNTS ?= auto
SMT_STATES ?= on,off
RESULTS_DIR ?= resultsrt
AMDAHL_CSV ?= $(RESULTS_DIR)/*.csv

ifeq ($(WITH_SDL),1)
CPPFLAGS += -DRT_WITH_SDL $(SDL_CFLAGS)
LDLIBS += $(SDL_LIBS)
endif

.PHONY: all build run run-all smt benchmark plots amdahl benchmark-plots profile profile-plots profile-plots-all profile-compare test test-plots test-profile test-display clean
all: $(BUILD)/raytracer

build: all

run: $(BUILD)/raytracer smt
	./$(BUILD)/raytracer $(ARGS)

# Campaña secuencial: no lanzar benchmarks simultáneos, incluso con make -j.
# Bash coordina los benchmarks; el script de perf captura y grafica contadores.
run-all: ARGS ?= --samples 8
run-all: $(BUILD)/raytracer
	+bash scripts/run_all.sh "$(MAKE)" "./$(BUILD)/raytracer" "$(PYTHON)" "$(REPETITIONS)" "$(THREAD_COUNTS)" "$(SMT_THREAD_COUNTS)" "$(SMT_STATES)" "$(RESULTS_DIR)" "$(PERF)" "$(PROFILE_REPETITIONS)" "$(PROFILE_SCOPE)" "$(PROFILE_EVENTS)" "$(BASELINE_REPETITIONS)" "$(PROFILE_COUNT_MODE)" "$(PROFILE_LOGS)" $(ARGS)

# No restaura el estado automáticamente: permite medir varios modelos con
# la misma configuración. El ejecutable sigue corriendo sin sudo.
smt:
	@set -eu; \
	case '$(SMT)' in \
	    keep) exit 0 ;; \
	    on|off) ;; \
	    *) echo 'SMT debe ser keep, on u off' >&2; exit 1 ;; \
	esac; \
	control=/sys/devices/system/cpu/smt/control; \
	active=/sys/devices/system/cpu/smt/active; \
	if [ ! -r "$$control" ] || [ ! -r "$$active" ]; then \
	    echo 'Este sistema no expone el control SMT de Linux' >&2; exit 1; \
	fi; \
	current=$$(cat "$$control"); \
	if [ "$$current" != '$(SMT)' ]; then \
	    case "$$current" in \
	        forceoff|notsupported|notimplemented) \
	            echo "No se puede cambiar SMT: $$current" >&2; exit 1 ;; \
	    esac; \
	    printf '%s\n' '$(SMT)' | sudo tee "$$control" >/dev/null; \
	fi; \
	state=$$(cat "$$active"); \
	expected=0; [ '$(SMT)' != on ] || expected=1; \
	if [ "$$state" != "$$expected" ]; then \
	    echo "No se alcanzó SMT=$(SMT): smt/active=$$state" >&2; exit 1; \
	fi; \
	echo "SMT=$(SMT), active=$$state (estado global; permanece después de ejecutar)"

benchmark: $(BUILD)/raytracer smt
	$(PYTHON) scripts/benchmark_plots.py --csv "$(CSV)" --plot-dir "$(PLOT_DIR)" ./$(BUILD)/raytracer $(ARGS)

plots:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)"

amdahl:
	$(PYTHON) scripts/plot_metrics.py "$(AMDAHL_CSV)" --output-dir "$(RESULTS_DIR)/plots/comparison" --amdahl-only

test-plots:
	$(PYTHON) tests/test_plot_metrics.py

# Compatibilidad: benchmark ya mide y grafica, fuera del intervalo medido.
benchmark-plots: benchmark

profile: $(BUILD)/raytracer smt
	$(PYTHON) scripts/profile_perf.py --perf "$(PERF)" --scope "$(PROFILE_SCOPE)" --count-mode "$(PROFILE_COUNT_MODE)" --logs "$(PROFILE_LOGS)" --events "$(PROFILE_EVENTS)" --repetitions "$(PROFILE_REPETITIONS)" --output-dir "$(PROFILE_DIR)" ./$(BUILD)/raytracer $(ARGS)

test-profile:
	$(PYTHON) tests/test_profile_perf.py

profile-plots:
	$(PYTHON) scripts/plot_profile.py "$(PROFILE_CAPTURE)"

# Comparación de todos los modelos por categoría; solo lee datos guardados.
profile-compare:
	$(PYTHON) scripts/plot_profile_comparison.py "$(PROFILE_COMPARE_DIR)"

# Solo releer capturas existentes; no renderiza ni necesita sudo/perf.
profile-plots-all:
	@set -eu; found=0; \
	for capture in "$(RESULTS_DIR)/perf/"*/; do \
	    if [ -f "$$capture/contadores.csv" ] && [ -f "$$capture/metadata.json" ]; then \
	        $(PYTHON) scripts/plot_profile.py "$$capture"; found=1; \
	    fi; \
	done; \
	if [ "$$found" = 0 ]; then echo 'No hay capturas perf en $(RESULTS_DIR)/perf/' >&2; exit 1; fi

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
