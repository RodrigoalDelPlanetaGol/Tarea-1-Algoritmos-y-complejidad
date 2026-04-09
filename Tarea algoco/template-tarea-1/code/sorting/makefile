CXX ?= g++ CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

SRC := sorting.cpp
algorithms/merge_sort.cpp
algorithms/quicksort.cpp
algorithms/sort.cpp

OBJ := $(SRC:.cpp=.o)

ifeq ($(OS),Windows_NT) BIN := sorting.exe LDFLAGS += -lpsapi RM := del /Q /F RUN_CMD := .\sorting.exe else BIN := sorting LDFLAGS += RM := rm -f RUN_CMD := ./sorting endif

.PHONY: all build run clean

all: build

build: $(BIN)

$(BIN): $(OBJ) $(CXX) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp $(CXX) $(CXXFLAGS) -c $< -o $@

run: $(BIN) $(RUN_CMD)

clean: -$(RM) $(OBJ) $(BIN)