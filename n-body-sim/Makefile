# Construcción del ejecutable activo y coordinación de simulación, campañas y ensayos.
# SOURCES incluye main, viewer, common y models; la interfaz anterior de include/nbody
# se conserva en el repositorio, pero no se enlaza en estos objetivos.
# := evalúa al asignar; ?= admite overrides; $@ es el objetivo y $^ sus dependencias.
# make ejecuta el flujo completo; make build solo compila.
# El objetivo sin argumentos calcula y reproduce; build solo compila.
.DEFAULT_GOAL := run

CXX ?= c++
# C++17, optimización O2 y advertencias del compilador, sustituibles por el llamante.
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
TARGET := build/nbody
# Física y captura compartidas; cada estrategia ocupa un único .cpp en models/.
# Física y captura activas usan src/nbody.hpp, no la implementación secuencial anterior.
CORE := src/common/physics.cpp src/common/trajectory.cpp
# Incluye todas las estrategias de models para que cambie solo la política de ejecución.
MODELS := $(wildcard src/models/*.cpp)
SOURCES := src/main.cpp src/viewer.cpp $(CORE) $(MODELS)
HEADER := src/nbody.hpp
# Ruta de cabeceras activa y soporte pthread en compilación y enlace.
PROJECT_CFLAGS := -Isrc -pthread
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

# En esta máquina se pueden usar los encabezados extraídos localmente y la
# biblioteca SDL2 de ejecución existente. En otras, se usa pkg-config.
# Alternativa de encabezados SDL locales cuando pkg-config no encuentra biblioteca de desarrollo.
ifeq ($(strip $(SDL_LIBS)),)
SDL_ROOT := .local/sdl2/usr
SDL_ARCH_INCLUDE := $(patsubst %/SDL2/_real_SDL_config.h,%,$(firstword $(wildcard $(SDL_ROOT)/include/*/SDL2/_real_SDL_config.h)))
ifneq ($(wildcard $(SDL_ROOT)/include/SDL2/SDL.h),)
SDL_CFLAGS := -I$(SDL_ROOT)/include/SDL2 -I$(SDL_ROOT)/include -I$(SDL_ARCH_INCLUDE)
SDL_LIBS := -l:libSDL2-2.0.so.0
endif
endif

# Objetivos de acción que no se confunden con archivos del mismo nombre.
.PHONY: run build sec fine coarse smt cmp smt-control benchmark plots amdahl profile test clean

# Cantidad de simulaciones exportadas por configuración, separada del calentamiento.
REPETITIONS ?= 200
BENCHMARK_MODELS ?= sequential fine-grained coarse-grained smt cmp
BENCHMARK_ARGS ?=
RESULTS_DIR ?= resultnbody
# Barridos P de coarse, CMP y SMT; la campaña mantiene N,pasos,dt y semilla.
THREAD_COUNTS ?= auto
CMP_THREAD_COUNTS ?= 1,2,4,8
SMT_THREAD_COUNTS ?= auto
# Fases hardware de campaña; keep omite cambios globales.
SMT_STATES ?= on,off
BENCHMARK_MAKE ?= $(MAKE)
# Referencia externa opcional; vacío selecciona medición de una base nueva.
BASELINE_CSV ?=
# Patrón de observaciones crudas para generar gráficas sin volver a simular.
CSV ?= $(RESULTS_DIR)/*.csv
PLOT_DIR ?= $(RESULTS_DIR)/plots/comparison
# Caché de Matplotlib en ruta temporal escribible para las tareas de análisis.
MPLCONFIGDIR ?= /tmp/nbody-matplotlib
export MPLCONFIGDIR
PROFILE_MODELS ?= sequential fine-grained coarse-grained smt cmp
# Repeticiones instrumentadas separadas de las de benchmark sin perf.
PROFILE_REPETITIONS ?= 3
PROFILE_ARGS ?=
PERF ?= perf
PERF_EVENTS ?= task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,cache-references,cache-misses
PYTHON ?= python3
ARGS ?=
# Control global de Linux; keep no modifica el estado del equipo.
SMT ?= keep

# Compila el binario, aplica SMT si se pidió y ejecuta ARGS.
run: $(TARGET) smt-control
	./$(TARGET) $(ARGS)

# Variables específicas de objetivo seleccionan política sin cambiar la física.
sec: RUN_MODEL := sequential
fine: RUN_MODEL := fine-grained
coarse: RUN_MODEL := coarse-grained
smt: RUN_MODEL := smt
cmp: RUN_MODEL := cmp

# El ejecutable imprime la duración y después abre el reproductor SDL2.
# Receta compartida: añade --model después de ARGS, con precedencia de última aparición.
sec fine coarse smt cmp: $(TARGET) smt-control
	./$(TARGET) $(ARGS) --model $(RUN_MODEL)

build: $(TARGET)

# No restaura el estado automáticamente: permite medir varios modelos con
# la misma configuración. El ejecutable sigue corriendo sin sudo.
# Control Linux: valida estado y disponibilidad, escribe sysfs si cambia y comprueba active.
# Los dobles dólares preservan variables para el shell de la receta.
# Solo la escritura solicita sudo; el simulador se ejecuta como usuario.
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


# Recorre modelos instrumentados en secuencia y exporta metadatos del compilador.
profile: $(TARGET) smt-control
	@set -e; for model in $(PROFILE_MODELS); do \
		PERF="$(PERF)" PERF_EVENTS="$(PERF_EVENTS)" \
		PROFILE_COMPILER="$(CXX)" PROFILE_FLAGS="$(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(LDFLAGS)" \
		bash scripts/profile.sh ./$(TARGET) "$(PROFILE_REPETITIONS)" --model "$$model" $(PROFILE_ARGS); \
	done

# Python coordina fases y referencias; cada simulación C++ aporta un tiempo de pared.
benchmark: $(TARGET) smt-control
	BENCHMARK_COMPILER="$(CXX)" BENCHMARK_FLAGS="$(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(LDFLAGS)" \
	$(PYTHON) scripts/benchmark.py ./$(TARGET) --repetitions "$(REPETITIONS)" \
		--results-dir "$(RESULTS_DIR)" --models $(BENCHMARK_MODELS) \
		--thread-counts "$(THREAD_COUNTS)" --smt-thread-counts "$(SMT_THREAD_COUNTS)" \
		--cmp-thread-counts "$(CMP_THREAD_COUNTS)" \
		--smt-states "$(SMT_STATES)" --make-command "$(BENCHMARK_MAKE)" \
		$(if $(BASELINE_CSV),--baseline-csv "$(BASELINE_CSV)") $(BENCHMARK_ARGS)

# Relee CSV existentes para distribución, estadística y escalabilidad.
plots:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)"

# Solo ajusta la referencia idealizada de speedup; no ejecuta simulaciones.
amdahl:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)" --amdahl-only

# Compilación y enlace de la ruta activa, incluida presentación SDL.
$(TARGET): $(SOURCES) $(HEADER)
	@test -n "$(SDL_LIBS)" || { printf 'Falta SDL2: instale libsdl2-dev y pkg-config (Linux), o sdl2 y pkg-config con Homebrew (macOS).\n'; exit 1; }
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(SDL_CFLAGS) $(SOURCES) $(LDFLAGS) $(SDL_LIBS) $(LDLIBS) -o $(TARGET)

# Ensayos físicos enlazados sin SDL: kernel, estrategias y barreras de fase.
build/test_physics: tests/test_physics.cpp src/common/physics.cpp $(MODELS) $(HEADER)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) tests/test_physics.cpp src/common/physics.cpp $(MODELS) $(LDFLAGS) $(LDLIBS) -o $@

# Ensayos de archivo y controles gráficos con la ruta de reproducción actual.
build/test_native: tests/test_native.cpp src/viewer.cpp $(CORE) $(MODELS) $(HEADER)
	@test -n "$(SDL_LIBS)" || { printf 'Falta SDL2: instale libsdl2-dev y pkg-config (Linux), o sdl2 y pkg-config con Homebrew (macOS).\n'; exit 1; }
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(SDL_CFLAGS) tests/test_native.cpp src/viewer.cpp $(CORE) $(MODELS) $(LDFLAGS) $(SDL_LIBS) $(LDLIBS) -o $@

# Secuencia de pruebas: física, SDL dummy, CLI, orquestación y estadística con fixtures.
test: $(TARGET) build/test_physics build/test_native
	./build/test_physics
	SDL_VIDEODRIVER=dummy ./build/test_native
	bash tests/test_cli.sh ./$(TARGET)
	bash tests/test_profile.sh
	$(PYTHON) tests/test_analysis.py
	$(PYTHON) tests/test_smt_control.py
	$(PYTHON) tests/test_benchmark.py

# Elimina únicamente los binarios derivados de los objetivos de construcción.
clean:
	rm -f $(TARGET) build/test_physics build/test_native
