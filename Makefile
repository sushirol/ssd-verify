CXX = g++
CXXFLAGS = -static -pthread -std=c++11

all: ssd_verify

ssd_verify: threaded_ssd_verify.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f ssd_verify
