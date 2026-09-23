CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
TARGET := build/nbody
SOURCES := main.cpp config.cpp simulation.cpp diagnostics.cpp output.cpp native_view.cpp
HEADERS := config.hpp simulation.hpp diagnostics.hpp output.hpp native_view.hpp bitmap_text.hpp
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
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) $(SOURCES) $(LDFLAGS) $(SDL_LIBS) $(LDLIBS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

build/test_physics: tests/test_physics.cpp simulation.cpp diagnostics.cpp output.cpp $(HEADERS)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I. tests/test_physics.cpp simulation.cpp diagnostics.cpp output.cpp $(LDFLAGS) $(LDLIBS) -o $@

build/test_native: tests/test_native.cpp native_view.cpp simulation.cpp $(HEADERS) | check-sdl
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) -I. tests/test_native.cpp native_view.cpp simulation.cpp $(LDFLAGS) $(SDL_LIBS) $(LDLIBS) -o $@

test: $(TARGET) build/test_physics build/test_native
	./build/test_physics
	SDL_VIDEODRIVER=dummy ./build/test_native
	bash tests/test_cli.sh ./$(TARGET)

clean:
	rm -f $(TARGET) build/test_physics build/test_native
