# Compilación y coordinación experimental del ray tracer C++17.
# Las recetas ejecutan procesos; el reparto paralelo de tiles está en src/execution/.
# := evalúa una vez, ?= admite sustitución desde CLI y += agrega opciones.
# $@ es el objetivo, $< la primera dependencia y $^ todas las dependencias.
CXX ?= g++
CPPFLAGS += -Isrc
CXXFLAGS ?= -O2 -g -std=c++17 -Wall -Wextra -Wpedantic
# Habilita soporte de hilos tanto en compilación como en enlace.
CXXFLAGS += -pthread
# Selecciona variante gráfica; WITH_SDL=0 conserva el núcleo sin SDL.
WITH_SDL ?= 1
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
# Directorios separados impiden mezclar objetos compilados con y sin SDL.
BUILD := build/$(if $(filter 1,$(WITH_SDL)),sdl,headless)
# Núcleo compartido por aplicación y ensayos; no depende de presentación SDL.
CORE_SOURCES := \
    src/scene/geometry.cpp src/scene/scene.cpp src/scene/camera.cpp \
    src/render/image.cpp src/render/tiles.cpp src/render/transport.cpp src/render/raytracer.cpp \
    src/execution/sequential.cpp src/execution/fine.cpp src/execution/coarse.cpp \
    src/execution/cpu.cpp src/execution/parallel.cpp src/metrics/metrics.cpp
# La aplicación añade entrada, argumentos, presentación y campaña de métricas.
APP_SOURCES := src/main.cpp src/app/options.cpp src/app/display.cpp \
    src/metrics/benchmark.cpp $(CORE_SOURCES)
# Transforma src/modulo.cpp en build/variante/modulo.o conservando carpetas.
APP_OBJECTS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(APP_SOURCES))
# Cabeceras que invalidan ensayos cuando cambia una interfaz.
HEADERS := $(wildcard src/*.hpp src/*/*.hpp)
# Herramientas de análisis posteriores a las mediciones del ejecutable C++.
PYTHON ?= python3
CSV ?= results/metrics.csv
PLOT_DIR ?= results/plots
PERF ?= perf
# Parámetros de captura de contadores, independientes de las repeticiones del benchmark.
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
# Tamaños de campaña y barridos de trabajadores por modelo.
REPETITIONS ?= 200
BASELINE_REPETITIONS ?= $(REPETITIONS)
THREAD_COUNTS ?= auto
SMT_THREAD_COUNTS ?= auto
SMT_STATES ?= on,off
RESULTS_DIR ?= resultsrt
AMDAHL_CSV ?= $(RESULTS_DIR)/*.csv

# La macro de compilación selecciona la implementación SDL de Display.
ifeq ($(WITH_SDL),1)
CPPFLAGS += -DRT_WITH_SDL $(SDL_CFLAGS)
LDLIBS += $(SDL_LIBS)
endif

# Estos nombres son acciones, no archivos cuya fecha permita omitir la receta.
.PHONY: all build run run-all smt benchmark plots amdahl benchmark-plots profile profile-plots profile-plots-all profile-compare test test-plots test-profile test-display clean
# Objetivo predeterminado: construye el ejecutable de la variante seleccionada.
all: $(BUILD)/raytracer

build: all

# Compila dependencias, aplica SMT si se pidió y ejecuta el programa con ARGS.
run: $(BUILD)/raytracer smt
	./$(BUILD)/raytracer $(ARGS)

# Campaña secuencial: no lanzar benchmarks simultáneos, incluso con make -j.
# Bash coordina los benchmarks; el script de perf captura y grafica contadores.
# Carga reducida predeterminada para una campaña; CLI puede sustituir ARGS.
run-all: ARGS ?= --samples 8
run-all: $(BUILD)/raytracer
	+bash scripts/run_all.sh "$(MAKE)" "./$(BUILD)/raytracer" "$(PYTHON)" "$(REPETITIONS)" "$(THREAD_COUNTS)" "$(SMT_THREAD_COUNTS)" "$(SMT_STATES)" "$(RESULTS_DIR)" "$(PERF)" "$(PROFILE_REPETITIONS)" "$(PROFILE_SCOPE)" "$(PROFILE_EVENTS)" "$(BASELINE_REPETITIONS)" "$(PROFILE_COUNT_MODE)" "$(PROFILE_LOGS)" $(ARGS)

# No restaura el estado automáticamente: permite medir varios modelos con
# la misma configuración. El ejecutable sigue corriendo sin sudo.
# Acción global Linux: keep conserva estado, on/off escriben sysfs con sudo.
# Los dobles dólares preservan variables para el shell de la receta.
# Las barras inversas mantienen las líneas dentro de un mismo proceso shell.
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

# Ejecuta primero C++ y luego Python; el análisis no entra en loop_time.
benchmark: $(BUILD)/raytracer smt
	$(PYTHON) scripts/benchmark_plots.py --csv "$(CSV)" --plot-dir "$(PLOT_DIR)" ./$(BUILD)/raytracer $(ARGS)

# Solo lee CSV existentes y regenera presentación.
plots:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)"

# Ajuste descriptivo de fracción efectiva sin realizar renders nuevos.
amdahl:
	$(PYTHON) scripts/plot_metrics.py "$(AMDAHL_CSV)" --output-dir "$(RESULTS_DIR)/plots/comparison" --amdahl-only

# Ensayos estadísticos y de compatibilidad sin medir hardware.
test-plots:
	$(PYTHON) tests/test_plot_metrics.py

# Compatibilidad: benchmark ya mide y grafica, fuera del intervalo medido.
benchmark-plots: benchmark

# Instrumenta ejecuciones separadas del benchmark sin perf.
profile: $(BUILD)/raytracer smt
	$(PYTHON) scripts/profile_perf.py --perf "$(PERF)" --scope "$(PROFILE_SCOPE)" --count-mode "$(PROFILE_COUNT_MODE)" --logs "$(PROFILE_LOGS)" --events "$(PROFILE_EVENTS)" --repetitions "$(PROFILE_REPETITIONS)" --output-dir "$(PROFILE_DIR)" ./$(BUILD)/raytracer $(ARGS)

# Ensayos de parsing, cobertura y emparejamiento de contadores por CPU/PMU.
test-profile:
	$(PYTHON) tests/test_profile_perf.py

# Relee una captura almacenada; no inicia instrumentación.
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

# Enlace: reúne objetos y bibliotecas; $@ es la ruta del ejecutable.
$(BUILD)/raytracer: $(APP_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

# Regla por patrón: compila cada .cpp y genera dependencias automáticas .d.
$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Binario de propiedades del núcleo sin dependencia de una ventana.
build/tests: tests/test_execution.cpp src/app/options.cpp src/metrics/benchmark.cpp $(CORE_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) -Isrc $(CXXFLAGS) tests/test_execution.cpp src/app/options.cpp src/metrics/benchmark.cpp $(CORE_SOURCES) -o $@

# Ejecuta equivalencia de imagen, planificación, cancelación y estadística.
test: build/tests
	./build/tests

# Ensayo de integración enlazado a SDL; dummy sustituye un escritorio real.
build/test-display: tests/test_display.cpp src/app/display.cpp $(CORE_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) -Isrc -DRT_WITH_SDL $(SDL_CFLAGS) $(CXXFLAGS) tests/test_display.cpp src/app/display.cpp $(CORE_SOURCES) $(SDL_LIBS) -o $@

test-display: build/test-display
	SDL_VIDEODRIVER=dummy ./build/test-display

# Elimina productos derivados de compilación dentro de build/.
clean:
	rm -rf build

# Importa dependencias de cabeceras si existen; el guion permite compilación inicial.
-include $(APP_OBJECTS:.o=.d)
