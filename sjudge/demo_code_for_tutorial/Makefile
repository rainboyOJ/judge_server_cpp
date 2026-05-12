CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O0 -g

TARGETS := \
	01_fork_exec \
	02_redirect_io \
	03_wait4_usage \
	04_realtime_limit \
	05_rlimit_memory \
	06_mini_judger

.PHONY: all clean

all: $(TARGETS)

01_fork_exec: 01_fork_exec.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

02_redirect_io: 02_redirect_io.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

03_wait4_usage: 03_wait4_usage.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

04_realtime_limit: 04_realtime_limit.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

05_rlimit_memory: 05_rlimit_memory.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

06_mini_judger: 06_mini_judger.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)
	rm -rf demo_tmp
