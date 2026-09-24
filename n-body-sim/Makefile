CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
TARGET := build/nbody
SOURCES := src/main.cpp src/config.cpp src/simulation.cpp src/diagnostics.cpp src/output.cpp src/native_view.cpp
HEADERS := include/nbody/config.hpp include/nbody/simulation.hpp include/nbody/diagnostics.hpp \
	include/nbody/output.hpp include/nbody/native_view.hpp include/nbody/bitmap_text.hpp
PROJECT_CFLAGS := -Iinclude
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

.PHONY: all run test check-sdl clean

all: $(TARGET)

check-sdl:
	@test -n "$(SDL_LIBS)" || { printf 'Falta SDL2: instale libsdl2-dev y pkg-config (Linux), o sdl2 y pkg-config con Homebrew (macOS).\n'; exit 1; }

$(TARGET): $(SOURCES) $(HEADERS) | check-sdl
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(SDL_CFLAGS) $(SOURCES) $(LDFLAGS) $(SDL_LIBS) $(LDLIBS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

build/test_physics: tests/test_physics.cpp src/simulation.cpp src/diagnostics.cpp src/output.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) tests/test_physics.cpp src/simulation.cpp src/diagnostics.cpp src/output.cpp $(LDFLAGS) $(LDLIBS) -o $@

build/test_native: tests/test_native.cpp src/native_view.cpp src/simulation.cpp $(HEADERS) | check-sdl
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_CFLAGS) $(SDL_CFLAGS) tests/test_native.cpp src/native_view.cpp src/simulation.cpp $(LDFLAGS) $(SDL_LIBS) $(LDLIBS) -o $@

test: $(TARGET) build/test_physics build/test_native
	./build/test_physics
	SDL_VIDEODRIVER=dummy ./build/test_native
	bash tests/test_cli.sh ./$(TARGET)

clean:
	rm -f $(TARGET) build/test_physics build/test_native
