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

.PHONY: run build sec fine coarse smt cmp smt-control benchmark plots amdahl profile test clean

REPETITIONS ?= 200
BENCHMARK_MODELS ?= sequential fine-grained coarse-grained smt cmp
BENCHMARK_ARGS ?=
RESULTS_DIR ?= resultnbody
THREAD_COUNTS ?= auto
CMP_THREAD_COUNTS ?= 1,2,4,8
SMT_THREAD_COUNTS ?= auto
SMT_STATES ?= on,off
BENCHMARK_MAKE ?= $(MAKE)
BASELINE_CSV ?=
CSV ?= $(RESULTS_DIR)/*.csv
PLOT_DIR ?= $(RESULTS_DIR)/plots/comparison
MPLCONFIGDIR ?= /tmp/nbody-matplotlib
export MPLCONFIGDIR
PROFILE_MODELS ?= sequential fine-grained coarse-grained smt cmp
PROFILE_REPETITIONS ?= 3
PROFILE_ARGS ?=
PERF ?= perf
PERF_EVENTS ?= task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,cache-references,cache-misses
PYTHON ?= python3
ARGS ?=
# Control global de Linux; keep no modifica el estado del equipo.
SMT ?= keep

run: $(TARGET) smt-control
	./$(TARGET) $(ARGS)

sec: RUN_MODEL := sequential
fine: RUN_MODEL := fine-grained
coarse: RUN_MODEL := coarse-grained
smt: RUN_MODEL := smt
cmp: RUN_MODEL := cmp

# El ejecutable imprime la duración y después abre el reproductor SDL2.
sec fine coarse smt cmp: $(TARGET) smt-control
	./$(TARGET) $(ARGS) --model $(RUN_MODEL)

build: $(TARGET)

# No restaura el estado automáticamente: permite medir varios modelos con
# la misma configuración. El ejecutable sigue corriendo sin sudo.
smt-control:
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


profile: $(TARGET) smt-control
	@set -e; for model in $(PROFILE_MODELS); do \
		PERF="$(PERF)" PERF_EVENTS="$(PERF_EVENTS)" \
		PROFILE_COMPILER="$(CXX)" PROFILE_FLAGS="$(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(LDFLAGS)" \
		bash scripts/profile.sh ./$(TARGET) "$(PROFILE_REPETITIONS)" --model "$$model" $(PROFILE_ARGS); \
	done

benchmark: $(TARGET) smt-control
	BENCHMARK_COMPILER="$(CXX)" BENCHMARK_FLAGS="$(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(LDFLAGS)" \
	$(PYTHON) scripts/benchmark.py ./$(TARGET) --repetitions "$(REPETITIONS)" \
		--results-dir "$(RESULTS_DIR)" --models $(BENCHMARK_MODELS) \
		--thread-counts "$(THREAD_COUNTS)" --smt-thread-counts "$(SMT_THREAD_COUNTS)" \
		--cmp-thread-counts "$(CMP_THREAD_COUNTS)" \
		--smt-states "$(SMT_STATES)" --make-command "$(BENCHMARK_MAKE)" \
		$(if $(BASELINE_CSV),--baseline-csv "$(BASELINE_CSV)") $(BENCHMARK_ARGS)

plots:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)"

amdahl:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)" --amdahl-only

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
	$(PYTHON) tests/test_smt_control.py
	$(PYTHON) tests/test_benchmark.py

clean:
	rm -f $(TARGET) build/test_physics build/test_native
