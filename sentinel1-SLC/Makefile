CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
GDAL_CONFIG ?= gdal-config
GDAL_CFLAGS = $(shell $(GDAL_CONFIG) --cflags)
GDAL_LIBS = $(shell $(GDAL_CONFIG) --libs)
CPPFLAGS += -Iinterferogram/include
COMMON = interferogram/src/config.cpp interferogram/src/slc_reader.cpp interferogram/src/interferogram.cpp
HEADERS = $(wildcard interferogram/include/interferogram/*.hpp)
TARGET = build/interferogram

.PHONY: all run test clean check-gdal
all: $(TARGET)

check-gdal:
	@command -v $(GDAL_CONFIG) >/dev/null || { echo "Instala GDAL de desarrollo: sudo apt install libgdal-dev"; exit 1; }

build:
	mkdir -p build

$(TARGET): interferogram/src/main.cpp $(COMMON) $(HEADERS) Makefile | check-gdal build
	$(CXX) $(CPPFLAGS) $(GDAL_CFLAGS) $(CXXFLAGS) -std=c++17 interferogram/src/main.cpp $(COMMON) $(LDFLAGS) $(GDAL_LIBS) $(LDLIBS) -o $@

build/test_interferogram: interferogram/tests/test_interferogram.cpp interferogram/src/interferogram.cpp $(HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 interferogram/tests/test_interferogram.cpp interferogram/src/interferogram.cpp $(LDFLAGS) $(LDLIBS) -o $@

build/test_slc_reader: interferogram/tests/test_slc_reader.cpp interferogram/src/slc_reader.cpp $(HEADERS) Makefile | check-gdal build
	$(CXX) $(CPPFLAGS) $(GDAL_CFLAGS) $(CXXFLAGS) -std=c++17 interferogram/tests/test_slc_reader.cpp interferogram/src/slc_reader.cpp $(LDFLAGS) $(GDAL_LIBS) $(LDLIBS) -o $@

run: $(TARGET)
	./$(TARGET) $(ARGS)

test: $(TARGET) build/test_interferogram build/test_slc_reader
	./build/test_interferogram
	./build/test_slc_reader
	bash interferogram/tests/test_cli.sh ./$(TARGET)
	python3 interferogram/tests/test_coregister.py

clean:
	rm -rf build
