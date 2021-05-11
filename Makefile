CXX=clang++
CXXFLAGS=-I include -std=c++17 -g3 -O2 -Wall
HDRS=$(wildcard include/photesthesis/*.h)
CPPS=$(wildcard src/*.cpp)

%.o: %.cpp $(HDRS) Makefile
	$(CXX) $(CXXFLAGS) -o $@ -c $<

test_photesthesis: test/test_photesthesis.cpp $(CPPS:.cpp=.o)
	$(CXX) $(CXXFLAGS) -fsanitize-coverage=inline-8bit-counters $^ -o $@

format:
	clang-format -i $(HDRS) $(CPPS) test/test_photesthesis.cpp

clean:
	rm -f src/*.o test/*.o test_photesthesis