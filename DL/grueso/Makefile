# ==========================================
# CONFIGURACIÓN Y VARIABLES DE COMPILACIÓN
# ==========================================

TARGET = grueso

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
EIGEN = -I/usr/include/eigen3

SRCS = grueso.cpp

# ==========================================
# REGLAS
# ==========================================

all: $(TARGET)

# Enlazado
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(EIGEN) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean