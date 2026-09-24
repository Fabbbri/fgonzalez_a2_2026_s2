# ==========================================
# CONFIGURACIÓN Y VARIABLES DE COMPILACIÓN
# ==========================================

TARGET = mlp

CXX = clang++
CXXFLAGS = -Wall -Wextra -std=c++17
EIGEN = -I/opt/homebrew/include/eigen3

SRCS = dummie.cpp

# ==========================================
# REGLAS
# ==========================================

all: $(TARGET)

# Enlazado
$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) $(EIGEN) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean