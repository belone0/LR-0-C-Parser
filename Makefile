CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
TARGET = lr0_parser
SRC = lr0_parser.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
