# ==========================================
# CONFIGURACIÓN Y VARIABLES DE COMPILACIÓN LINUX
# ==========================================

CXX = g++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17 -pthread
EIGEN = -I/usr/include/eigen3

SRCS = $(wildcard *.cpp)

TARGETS = $(SRCS:.cpp=)

# ==========================================a
# REGLAS
# ==========================================

all: $(TARGETS)

# Enlazado de ejecutable dinamico @ a partir de su archivo fuente <
%: %.cpp
	$(CXX) $(CXXFLAGS) $(EIGEN) $< -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean