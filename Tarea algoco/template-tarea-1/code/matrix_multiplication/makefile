CXX ?= g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET = matrix_multiplication.exe
SRC = matrix_multiplication.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)