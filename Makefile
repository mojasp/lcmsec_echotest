# This Makefile was tested with GNU Make
CXX=g++ --std=c++17

# Use pkg-config to lookup the proper compiler and linker flags for LCM
CFLAGS=`pkg-config --cflags lcm` -g
LDFLAGS=`pkg-config --libs lcm`

all: exlcm/msg.hpp demo_instance

%.o: %.cpp exlcm/example_t.hpp
	$(CXX) -g $(CFLAGS) -I. -o $@ -c $< 

demo_instance: demo_instance.o
	$(CXX) -o $@ $^ $(LDFLAGS) $(CFLAGS)

exlcm/%.hpp:
	lcm-gen -x msg.lcm

clean:
	rm -f demo_instance
	rm -f *.o exlcm/*.o
	rm -f exlcm/*.hpp
