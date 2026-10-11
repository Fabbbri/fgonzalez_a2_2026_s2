CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
GDAL_CONFIG ?= gdal-config
GDAL_CFLAGS = $(shell $(GDAL_CONFIG) --cflags)
GDAL_LIBS = $(shell $(GDAL_CONFIG) --libs)
CPPFLAGS += -Iinterferogram/include
MODELS = $(wildcard interferogram/src/models/*.cpp)
KERNEL = interferogram/src/interferogram.cpp interferogram/src/cpu_info.cpp $(MODELS)
COMMON = interferogram/src/config.cpp interferogram/src/slc_reader.cpp $(KERNEL)
HEADERS = $(wildcard interferogram/include/interferogram/*.hpp interferogram/include/interferogram/models/*.hpp)
TARGET = build/interferogram

.PHONY: all run test clean check-gdal
all: $(TARGET)

check-gdal:
	@command -v $(GDAL_CONFIG) >/dev/null || { echo "Instala GDAL de desarrollo: sudo apt install libgdal-dev"; exit 1; }

build:
	mkdir -p build

$(TARGET): interferogram/src/main.cpp $(COMMON) $(HEADERS) Makefile | check-gdal build
	$(CXX) $(CPPFLAGS) $(GDAL_CFLAGS) $(CXXFLAGS) -pthread -std=c++17 interferogram/src/main.cpp $(COMMON) $(LDFLAGS) $(GDAL_LIBS) $(LDLIBS) -o $@

build/test_interferogram: interferogram/tests/test_interferogram.cpp $(KERNEL) $(HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread -std=c++17 interferogram/tests/test_interferogram.cpp $(KERNEL) $(LDFLAGS) $(LDLIBS) -o $@

build/test_slc_reader: interferogram/tests/test_slc_reader.cpp interferogram/src/slc_reader.cpp $(HEADERS) Makefile | check-gdal build
	$(CXX) $(CPPFLAGS) $(GDAL_CFLAGS) $(CXXFLAGS) -pthread -std=c++17 interferogram/tests/test_slc_reader.cpp interferogram/src/slc_reader.cpp $(LDFLAGS) $(GDAL_LIBS) $(LDLIBS) -o $@

run: $(TARGET)
	./$(TARGET) $(ARGS)

test: $(TARGET) build/test_interferogram build/test_slc_reader
	./build/test_interferogram
	./build/test_slc_reader
	bash interferogram/tests/test_cli.sh ./$(TARGET)

clean:
	rm -rf build

.PHONY: images
images:
	python3 scripts/render_results.py $(ARGS)
