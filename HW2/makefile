# Compiler
CXX := g++
# Compiler flags
CXXFLAGS := -Wall -Wextra -std=c++11

# Source files
SRCS := cacheSim.cpp

# Object files
OBJS := $(SRCS:.cpp=.o)

# Executable name
EXEC := cacheSim

# Main target
all: $(EXEC)

# Linking object files to generate executable
$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compiling source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean rule
clean:
	rm -f $(OBJS) $(EXEC)

