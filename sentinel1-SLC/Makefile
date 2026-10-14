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

.PHONY: all run test clean check-gdal
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

test: $(TARGET) build/test_interferogram build/test_slc_reader
	./build/test_interferogram
	./build/test_slc_reader
	bash interferogram/tests/test_cli.sh ./$(TARGET)
	bash interferogram/tests/test_profile_perf.sh

clean:
	rm -rf build

.PHONY: images
images:
	python3 scripts/render_results.py $(ARGS)

# Matriz fija: secuencial, Fine 1, Coarse/CMP 1-2-4-8 y SMT 8-12.
# Los resultados van a un directorio nuevo bajo results/ si OUTPUT=auto.
.PHONY: benchmark profile run-sequential run-fine-grained run-coarse-grained run-smt run-cmp
REPETITIONS ?= 200
OUTPUT ?= auto
THREADS ?= 8
SLC_A ?= data/coregistered/P09-R2-20240608-20240620/A.tif
SLC_B ?= data/coregistered/P09-R2-20240608-20240620/B.tif
BENCHMARK_ARGS ?= --a $(SLC_A) --b $(SLC_B) --x 0 --y 0 --width 24451 --height 1499 --allow-nodata --quantum 2

build/benchmark: interferogram/metrics/benchmark.cpp Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 $< $(LDFLAGS) $(LDLIBS) -o $@

benchmark: $(TARGET) build/benchmark
	bash scripts/benchmark.sh "$(OUTPUT)" "$(REPETITIONS)" $(BENCHMARK_ARGS)

# Una campaña por modelo, con N mediciones secuenciales de referencia.
# THREADS puede sobrescribirse desde la terminal; Fine y secuencial usan 1.
run-smt: THREADS = 12
run-sequential run-fine-grained run-coarse-grained run-smt run-cmp: $(TARGET) build/benchmark
	BENCHMARK_MODEL="$(@:run-%=%)" BENCHMARK_THREADS="$(THREADS)" \
		bash scripts/benchmark.sh "$(OUTPUT)" "$(REPETITIONS)" $(BENCHMARK_ARGS)

PROFILE_REPETITIONS ?= 3
PROFILE_CASES ?= sequential:1 fine-grained:1 \
                 coarse-grained:1 coarse-grained:2 coarse-grained:4 coarse-grained:8 \
                 smt:8 smt:12 cmp:1 cmp:2 cmp:4 cmp:8
PROFILE_OUTPUT ?= auto
PROFILE_ARGS ?= $(BENCHMARK_ARGS)
profile: $(TARGET)
	SLC_A="$(SLC_A)" SLC_B="$(SLC_B)" \
		bash scripts/profile_all.sh "$(PROFILE_OUTPUT)" "$(PROFILE_REPETITIONS)" \
		"$(PROFILE_CASES)" $(PROFILE_ARGS)
