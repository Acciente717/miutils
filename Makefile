CXX := g++
CXXFLAGS := -std=c++14 -flto -march=native -O2
CXXLIBS := -lboost_program_options -lpthread

BINDIR := bin
OBJDIR := obj
SRCDIR := src
INCDIR := include
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
HEADERS := $(wildcard $(INCDIR)/*.hpp)
OBJS :=  $(subst $(SRCDIR),$(OBJDIR),$(patsubst %.cpp,%.o,$(SOURCES)))

$(BINDIR)/xmlparser: $(OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(CXXLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(INCDIR)/*.hpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I $(INCDIR) -c -o $@ $<

$(OBJDIR):
	mkdir $(OBJDIR)

$(BINDIR):
	mkdir $(BINDIR)

.PHONY: install
install:
	rm -rf /usr/local/bin/xmlparser 2>/dev/null
	ln -s $$(pwd)/$(BINDIR)/xmlparser /usr/local/bin/xmlparser

.PHONY: uninstall
uninstall:
	rm -rf /usr/local/bin/xmlparser 2>/dev/null

.PHONY: clean
clean:
	rm -rf $(BINDIR) 2>/dev/null
	rm -rf $(OBJDIR) 2>/dev/null
