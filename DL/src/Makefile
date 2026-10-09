# ==========================================
# CONFIGURACIÓN PARA macOS
# ==========================================

CXX = clang++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17 -pthread
EIGEN = -I/opt/homebrew/include/eigen3

TARGETS = main fino

# ==========================================
# REGLAS
# ==========================================

all: $(TARGETS)

main: main.cpp lector.cpp mnist.h
	$(CXX) $(CXXFLAGS) $(EIGEN) main.cpp lector.cpp -o main

fino: fino.cpp lector.cpp mnist.h
	$(CXX) $(CXXFLAGS) $(EIGEN) fino.cpp lector.cpp -o fino

clean:
	rm -f $(TARGETS)

.PHONY: all clean