# Compilador (forzado a MinGW64)
CXX = /mingw64/bin/g++

# Flags de compilación
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic

# Nombre del ejecutable
TARGET = matrix_multiplication.exe

# Archivo fuente
SRC = matrix_multiplication.cpp

# Reglas
.PHONY: all run clean rebuild

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

rebuild: clean all