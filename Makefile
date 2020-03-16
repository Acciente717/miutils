CXX := g++
CXXFLAGS := -std=c++14 -flto -march=native -O2
CXXLIBS := -lboost_program_options -lpthread

OBJDIR := obj
SOURCES := $(wildcard *.cpp)
HEADERS := $(wildcard *.hpp)
OBJS :=  $(patsubst %.cpp,%.o,$(SOURCES))
OBJS := $(addprefix $(OBJDIR)/, $(OBJS))

xmlparser: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(CXXLIBS)

$(addprefix $(OBJDIR)/,%.o): %.cpp *.hpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir $(OBJDIR)

.PHONY: clean
clean:
	rm -rf obj
	rm xmlparser
