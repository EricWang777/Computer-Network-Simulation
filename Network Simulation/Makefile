TARGET ?= pa6
SRC_DIRS ?= .

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp' -or -name '*.c' -or -name '*.s')
OBJS := $(addsuffix .o,$(basename $(SRCS)))
DEPS := $(OBJS:.o=.d)

CXXFLAGS=-g -MMD -MP -std=c++11 -Wall -Werror -pedantic
LDLIBS ?= -lglog -lgflags -lboost_system -lboost_thread -lpthread -lssl -lcrypto

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o $@ $(LOADLIBES) $(LDLIBS)

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)

run:
	./pa6 -reset homedirs/pa5-12016.cfg