# ==========================================
# CONFIGURACIÓN PARA UBUNTU
# ==========================================

CXX = g++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17 -pthread
EIGEN = -I/usr/include/eigen3

TARGETS = main fino grueso smt cmp
HEADERS = mnist.h checksum.h

# ==========================================
# REGLAS
# ==========================================

all: $(TARGETS)

# Cada ejecutable X se construye desde X.cpp + lector.cpp
$(TARGETS): %: %.cpp lector.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(EIGEN) $< lector.cpp -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean