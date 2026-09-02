CC = gcc

CFLAGS = -Wall -O2 -fPIC
LDFLAGS = -shared
LIBS = -pthread

TARGET = libmymalloc.so
SRC = src/myMalloc.c

# ------------------------------------------------------------------ #
# Test suite                                                          #
#                                                                     #
# Every .c in tests/tests_src is compiled into its own executable in  #
# tests/tests_executables, linked directly against the allocator so   #
# malloc/free/etc. resolve to the custom implementation. `tests/testall`
# runs them all and prints a pass/fail summary.                       #
# ------------------------------------------------------------------ #
TEST_CFLAGS  = -Wall -g -O0 -Isrc
TEST_SRC_DIR = tests/tests_src
TEST_BIN_DIR = tests/tests_executables
TEST_UTILS   = $(TEST_SRC_DIR)/test_utils.h
TEST_SRCS    = $(wildcard $(TEST_SRC_DIR)/*.c)
TEST_BINS    = $(patsubst $(TEST_SRC_DIR)/%.c,$(TEST_BIN_DIR)/%,$(TEST_SRCS))
TESTALL      = tests/testall

.PHONY: all clean test tests

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Directory that holds the compiled test executables.
$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

# One executable per test source, each linked with the allocator.
$(TEST_BIN_DIR)/%: $(TEST_SRC_DIR)/%.c $(SRC) $(TEST_UTILS) | $(TEST_BIN_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ $< $(SRC) $(LIBS)

# The aggregate runner (does not use the custom allocator itself).
$(TESTALL): tests/testall.c
	$(CC) -Wall -g -O0 -DTESTS_EXEC_DIR='"$(CURDIR)/tests/tests_executables"' -o $@ $<

# Build every test executable plus the runner.
tests: $(TEST_BINS) $(TESTALL)

# Build everything and run the whole suite.
test: $(TEST_BINS) $(TESTALL)
	$(TESTALL)

clean:
	rm -f $(TARGET) $(TESTALL) $(TEST_BINS)
