CXX := g++
CXXFLAGS := -std=c++14 -flto -march=native -O2
CXXLIBS := -lboost_program_options -lpthread

BIN_DIR := bin
SRC_DIR := src
INC_DIR := include
SRC_OBJ_DIR := obj
ACTION_DIR := src/actions
ACTION_OBJ_DIR := obj/actions
SRC_SRCS := $(wildcard $(SRC_DIR)/*.cpp)
ACTION_SRCS := $(wildcard $(ACTION_DIR)/*.cpp)
HEADERS := $(wildcard $(INC_DIR)/*.hpp)
SRC_OBJS := $(subst $(SRC_DIR),$(SRC_OBJ_DIR),$(patsubst %.cpp,%.o,$(SRC_SRCS)))
ACTION_OBJS := $(subst $(ACTION_DIR),$(ACTION_OBJ_DIR),$(patsubst %.cpp,%.o,$(ACTION_SRCS)))

$(BIN_DIR)/xmlparser: $(SRC_OBJS) $(ACTION_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC_OBJS) $(ACTION_OBJS) $(CXXLIBS)

$(ACTION_OBJ_DIR)/%.o: $(ACTION_DIR)/%.cpp $(INC_DIR)/*.hpp | $(ACTION_OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I $(INC_DIR) -c -o $@ $<

$(SRC_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(INC_DIR)/*.hpp | $(SRC_OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I $(INC_DIR) -c -o $@ $<

$(SRC_OBJ_DIR):
	mkdir $(SRC_OBJ_DIR)

$(ACTION_OBJ_DIR): | $(SRC_OBJ_DIR)
	mkdir $(ACTION_OBJ_DIR)

$(BIN_DIR):
	mkdir $(BIN_DIR)

.PHONY: install
install:
	rm -rf /usr/local/bin/xmlparser 2>/dev/null
	cp $$(pwd)/$(BIN_DIR)/xmlparser /usr/local/bin

.PHONY: uninstall
uninstall:
	rm -rf /usr/local/bin/xmlparser 2>/dev/null

.PHONY: clean
clean:
	rm -rf $(BIN_DIR) 2>/dev/null
	rm -rf $(SRC_OBJ_DIR) 2>/dev/null
