# Makefile for compiling and running arena allocator tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I.
DEBUG_FLAGS = -DDEBUG

TEST_DIR = tests
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(TEST_SRCS:.c=)

.PHONY: all clean run tests tests_full list

# Default goal: show available commands
all: clean list

# Compilation of each test without debug information
$(TEST_DIR)/%_silent: $(TEST_DIR)/%.c arena.h $(TEST_DIR)/test_utils.h
	$(CC) $(CFLAGS) $< -o $@

# Compilation of each test with debug information
$(TEST_DIR)/%_debug: $(TEST_DIR)/%.c arena.h $(TEST_DIR)/test_utils.h
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $< -o $@

# Pattern rule for running individual tests (always with debug)
test_%: $(TEST_DIR)/%_test_debug
	@echo "\n--- Running $< (debug mode) ---"
	@./$<
	@if [ $$? -ne 0 ]; then \
		echo "\nTest $< FAILED!"; \
		exit 1; \
	else \
		echo "\nTest $< PASSED!"; \
	fi

# Compilation of all tests without debug
build_silent: $(TEST_SRCS:%.c=%_silent)

# Compilation of all tests with debug information
build_debug: $(TEST_SRCS:%.c=%_debug)

# Memory leak check using valgrind
valgrind: build_silent
	@echo "Running valgrind memory check on all tests..."
	@for test in $(TEST_SRCS:%.c=%_silent) ; do \
		echo "\n--- Checking $$test ---" ; \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$$test ; \
	done
	@echo "\nAll memory checks completed."

# Testing: run all tests without debug info
tests: build_silent
	@echo "Running all tests (normal mode)..."
	@for test in $(TEST_SRCS:%.c=%_silent) ; do \
		echo "\n--- Running $$test ---" ; \
		./$$test ; \
		if [ $$? -ne 0 ]; then \
			echo "\nTest $$test FAILED with exit code $$?"; \
			exit_code=1; \
		fi; \
	done; \
	if [ "$$exit_code" = "1" ]; then \
		echo "\nSome tests FAILED!"; \
		exit 1; \
	else \
		echo "\nAll tests PASSED!"; \
	fi

# Testing: run all tests with debug info
tests_full: build_debug
	@echo "Running all tests (debug mode)..."
	@for test in $(TEST_SRCS:%.c=%_debug) ; do \
		echo "\n--- Running $$test ---" ; \
		./$$test ; \
		if [ $$? -ne 0 ]; then \
			echo "\nTest $$test FAILED with exit code $$?"; \
			exit_code=1; \
		fi; \
	done; \
	if [ "$$exit_code" = "1" ]; then \
		echo "\nSome tests FAILED!"; \
		exit 1; \
	else \
		echo "\nAll tests PASSED!"; \
	fi

# Cleaning binary files
clean:
	rm -f $(TEST_SRCS:%.c=%_silent) $(TEST_SRCS:%.c=%_debug)

# Show available tests
list:
	@echo "Available commands:"
	@echo "  make tests      - run all tests without debug output"
	@echo "  make tests_full - run all tests with debug output"
	@echo "\nAvailable individual tests (always with debug output):"
	@for test in $(TEST_SRCS) ; do \
		basename=$$(basename $${test%.c} _test); \
		echo "  make test_$$basename" ; \
	done