CXX = g++
CXXFLAGS = -std=c++23 -Wall  -pedantic -pthread
TARGET = Fibo_Sequencer

SRCS = Fibo_Sequencer.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
