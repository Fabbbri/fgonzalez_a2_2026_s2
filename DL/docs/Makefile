# ==========================================
# CONFIGURACIÓN Y VARIABLES DE COMPILACIÓN MACOS
# ==========================================

CXX = clang++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17 -pthread
EIGEN = -I/opt/homebrew/include/eigen3

SRCS = $(wildcard *.cpp)
TARGETS = $(SRCS:.cpp=)

# ==========================================
# REGLAS
# ==========================================

all: $(TARGET)

# Enlazado de ejecutable dinamico @ a partir de su archivo fuente <
%: %.cpp
	$(CXX) $(CXXFLAGS) $(EIGEN) $< -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean