# ==========================================
# CONFIGURACIÓN Y VARIABLES DE COMPILACIÓN MACOS
# ==========================================

CXX = clang++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17 -pthread
EIGEN = -I/opt/homebrew/include/eigen3

SRCS = $(wildcard *.cpp)
TARGETS = demo

# ==========================================
# REGLAS
# ==========================================

all: $(TARGETS)

demo: demo.cpp lector.cpp mnist.h
	$(CXX) $(CXXFLAGS) $(EIGEN) demo.cpp lector.cpp -o demo

# Enlazado de ejecutable dinamico @ a partir de su archivo fuente <
%: %.cpp
	$(CXX) $(CXXFLAGS) $(EIGEN) $< -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean