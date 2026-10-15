.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
GDAL_CONFIG ?= gdal-config
GDAL_CFLAGS = $(shell $(GDAL_CONFIG) --cflags 2>/dev/null)
GDAL_LIBS = $(shell $(GDAL_CONFIG) --libs 2>/dev/null)
CPPFLAGS += -Iinterferogram/include
MODELS := $(wildcard interferogram/src/models/*.cpp)
KERNEL := interferogram/src/interferogram.cpp interferogram/src/cpu_info.cpp $(MODELS)
COMMON := interferogram/src/config.cpp interferogram/src/slc_reader.cpp $(KERNEL)
HEADERS := $(wildcard interferogram/include/interferogram/*.hpp interferogram/include/interferogram/models/*.hpp)
TARGET := build/interferogram

.PHONY: all run test clean check-gdal test-benchmark smt plots amdahl rebase
all: $(TARGET)

check-gdal:
	@command -v "$(GDAL_CONFIG)" >/dev/null || { echo "Instala GDAL de desarrollo: sudo apt install libgdal-dev"; exit 1; }

build:
	mkdir -p build

$(TARGET): interferogram/src/main.cpp $(COMMON) $(HEADERS) Makefile | check-gdal build
	$(CXX) $(CPPFLAGS) $(GDAL_CFLAGS) $(CXXFLAGS) -pthread -std=c++17 $(filter %.cpp,$^) $(LDFLAGS) $(GDAL_LIBS) $(LDLIBS) -o $@

build/test_interferogram: interferogram/tests/test_interferogram.cpp $(KERNEL) $(HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread -std=c++17 $(filter %.cpp,$^) $(LDFLAGS) $(LDLIBS) -o $@

build/test_slc_reader: interferogram/tests/test_slc_reader.cpp interferogram/src/slc_reader.cpp $(HEADERS) Makefile | check-gdal build
	$(CXX) $(CPPFLAGS) $(GDAL_CFLAGS) $(CXXFLAGS) -pthread -std=c++17 $(filter %.cpp,$^) $(LDFLAGS) $(GDAL_LIBS) $(LDLIBS) -o $@

run: $(TARGET)
	./$(TARGET) $(ARGS)

test: $(TARGET) build/test_interferogram build/test_slc_reader build/benchmark_fixture
	./build/test_interferogram
	./build/test_slc_reader
	bash interferogram/tests/test_cli.sh ./$(TARGET)
	bash interferogram/tests/test_profile_perf.sh
	$(PYTHON) -m unittest discover -s interferogram/tests -p 'test_benchmark*.py'
	$(PYTHON) interferogram/tests/test_smt_control.py

clean:
	rm -rf build

# Visualización de la imagen de datos de SLC_A y SLC_B.
IMAGE_OUTPUT ?= results/imagenes-20240608-20240620
IMAGE_X ?= 0
IMAGE_Y ?= 0
IMAGE_WIDTH ?= 24444
IMAGE_HEIGHT ?= 1500

.PHONY: images view-images export-result
IMAGE_MODEL ?= sequential
RESULT_OUTPUT ?= $(IMAGE_OUTPUT)/$(IMAGE_MODEL).tif

export-result: $(TARGET)
	./$(TARGET) --a "$(SLC_A)" --b "$(SLC_B)" \
		--x $(IMAGE_X) --y $(IMAGE_Y) --width $(IMAGE_WIDTH) --height $(IMAGE_HEIGHT) \
		--allow-nodata --mode $(IMAGE_MODEL) $(ARGS) --output "$(RESULT_OUTPUT)"

images: export-result
	python3 scripts/render_results.py --a "$(SLC_A)" --b "$(SLC_B)" \
		--result "$(RESULT_OUTPUT)" --out "$(IMAGE_OUTPUT)" --x $(IMAGE_X) --y $(IMAGE_Y) \
		--width $(IMAGE_WIDTH) --height $(IMAGE_HEIGHT)

view-images:
	xdg-open "$(IMAGE_OUTPUT)/index.html"

# Métricas: no cambian los modelos ni el procesamiento del interferograma.
.PHONY: benchmark profile run-sequential run-fine-grained run-coarse-grained run-smt run-cmp
PYTHON ?= python3
REPETITIONS ?= 200
WARMUPS ?= 1
RESULTS_DIR ?= resultsen
OUTPUT ?= $(RESULTS_DIR)
THREADS ?= 8
THREAD_COUNTS ?= auto
CMP_THREAD_COUNTS ?= auto
SMT_THREAD_COUNTS ?= auto
SMT_STATES ?= on,off
SMT ?= keep
BENCHMARK_MODELS ?= sequential fine-grained coarse-grained smt cmp
BENCHMARK_MAKE ?= $(MAKE)
BASELINE_CSV ?=
CSV ?= $(RESULTS_DIR)/*.csv
PLOT_DIR ?= $(RESULTS_DIR)/plots/comparison
MPLCONFIGDIR ?= /tmp/sentinel-matplotlib
export MPLCONFIGDIR
SLC_A ?= data/coregistered/P09-R2-20240608-20240620/A.tif
SLC_B ?= data/coregistered/P09-R2-20240608-20240620/B.tif
WINDOW_X ?= 0
WINDOW_Y ?= 0
WINDOW_WIDTH ?= 24444
WINDOW_HEIGHT ?= 1499
TILE_WIDTH ?= 32
TILE_HEIGHT ?= 32
QUANTUM ?= 2
COMMON_ARGS = --a "$(SLC_A)" --b "$(SLC_B)" --x $(WINDOW_X) --y $(WINDOW_Y) \
              --width $(WINDOW_WIDTH) --height $(WINDOW_HEIGHT) --allow-nodata \
              --tile-width $(TILE_WIDTH) --tile-height $(TILE_HEIGHT) --quantum $(QUANTUM)
BENCHMARK_ARGS ?=

benchmark: $(TARGET) smt
	BENCHMARK_COMPILER="$(CXX)" BENCHMARK_FLAGS="$(CPPFLAGS) $(GDAL_CFLAGS) $(CXXFLAGS) -pthread -std=c++17 $(LDFLAGS)" \
	$(PYTHON) scripts/benchmark.py ./$(TARGET) --results-dir "$(OUTPUT)" \
		--models $(BENCHMARK_MODELS) --repetitions "$(REPETITIONS)" --warmups "$(WARMUPS)" \
		--thread-counts "$(THREAD_COUNTS)" --cmp-thread-counts "$(CMP_THREAD_COUNTS)" --smt-thread-counts "$(SMT_THREAD_COUNTS)" \
		--smt-states "$(SMT_STATES)" --make-command "$(BENCHMARK_MAKE)" \
		$(if $(BASELINE_CSV),--baseline-csv "$(BASELINE_CSV)") $(COMMON_ARGS) $(BENCHMARK_ARGS)

run-smt: THREADS = 12
run-sequential run-fine-grained run-coarse-grained run-smt run-cmp: $(TARGET) smt
	$(PYTHON) scripts/benchmark.py ./$(TARGET) --results-dir "$(OUTPUT)" \
		--model "$(@:run-%=%)" --repetitions "$(REPETITIONS)" --warmups "$(WARMUPS)" --threads "$(THREADS)" \
		--smt-states "$(SMT_STATES)" --make-command "$(BENCHMARK_MAKE)" \
		$(if $(BASELINE_CSV),--baseline-csv "$(BASELINE_CSV)") $(COMMON_ARGS) $(BENCHMARK_ARGS)

rebase:
	$(PYTHON) scripts/rebase_reference.py --results-dir "$(RESULTS_DIR)" \
		$(if $(BASELINE_CSV),--baseline-csv "$(BASELINE_CSV)")

plots:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)"

amdahl:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)" --amdahl-only

test-benchmark: $(TARGET) build/benchmark_fixture
	$(PYTHON) -m unittest discover -s interferogram/tests -p 'test_benchmark*.py'
	$(PYTHON) interferogram/tests/test_smt_control.py

build/benchmark_fixture: interferogram/tests/benchmark_fixture.cpp Makefile | check-gdal build
	$(CXX) $(GDAL_CFLAGS) $(CXXFLAGS) -std=c++17 $< $(GDAL_LIBS) -o $@

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


PROFILE_REPETITIONS ?= 3
PROFILE_CASES ?= sequential:1 fine-grained:1 \
                 coarse-grained:1 coarse-grained:2 coarse-grained:4 coarse-grained:8 \
                 smt:8 smt:12 cmp:1 cmp:2 cmp:4 cmp:8
PROFILE_OUTPUT ?= auto
PROFILE_ARGS ?= $(COMMON_ARGS) $(BENCHMARK_ARGS)
profile: $(TARGET)
	SLC_A="$(SLC_A)" SLC_B="$(SLC_B)" \
		bash scripts/profile_all.sh "$(PROFILE_OUTPUT)" "$(PROFILE_REPETITIONS)" \
		"$(PROFILE_CASES)" $(PROFILE_ARGS)
