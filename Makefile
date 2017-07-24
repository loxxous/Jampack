CXX = g++
CXXFLAGS = -std=c++14 -fopenmp -O3
LDFLAGS = -s -static
SOURCES = $(wildcard *.cpp)

Jampack:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SOURCES) -o $@

clean:
	rm -f Jampack
