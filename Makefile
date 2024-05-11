CXX = g++
CXXFLAGS = -static -pthread -std=c++11

all: ssd_read_verify ssd_write_verify

ssd_read_verify: threaded_ssd_verify.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

ssd_write_verify: ssd_write_verify.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@
clean:
	rm -f ssd_read_verify ssd_write_verify
