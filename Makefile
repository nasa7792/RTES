# Makefile for building the sequencer sample code.
# Build with: g++ --std=c++23 -Wall -Werror -pedantic
# This Makefile compiles Sequencer.cpp (which includes main()) and the Sequencer.hpp header.
# I learned about Makefile syntax and ensuring that tabs (not spaces) are used for recipe commands.

CXX = g++
CXXFLAGS = --std=c++23 -Wall -Werror -pedantic

# Target executable name
TARGET = sequencer_app

# Source files
SRCS = Sequencer.cpp

all: $(TARGET)

$(TARGET): $(SRCS) Sequencer.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
