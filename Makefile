# Makefile for compiling and running arena allocator tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I.
DEBUG_FLAGS = -DDEBUG

TEST_DIR = tests
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(TEST_SRCS:.c=)

.PHONY: all clean run run_debug test valgrind list

# Default goal: compile and run all tests in debug mode
all: clean list

# Compilation of each test without debug information
$(TEST_DIR)/%: $(TEST_DIR)/%.c arena.h $(TEST_DIR)/test_utils.h
	$(CC) $(CFLAGS) $< -o $@

# Compilation of each test with debug information
$(TEST_DIR)/%_debug: $(TEST_DIR)/%.c arena.h $(TEST_DIR)/test_utils.h
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $< -o $@

# Pattern rule for running individual tests
test_%: $(TEST_DIR)/%_test_debug
	@echo "\n--- Running $< ---"
	@./$<
	@if [ $$? -ne 0 ]; then \
		echo "\nTest $< FAILED!"; \
		exit 1; \
	else \
		echo "\nTest $< PASSED!"; \
	fi

# Compilation of all tests
build: $(TEST_BINS)

# Compilation of all tests with debug information
build_debug: $(TEST_SRCS:%.c=%_debug)

# Running all tests (without debug information)
run: build
	@echo "Running all tests..."
	@for test in $(TEST_BINS) ; do \
		echo "\n--- Running $$test ---" ; \
		./$$test ; \
		if [ $$? -ne 0 ]; then \
			echo "\nTest $$test FAILED!"; \
			failed_count=$$((failed_count + 1)); \
		fi; \
	done
	@echo "\nAll tests completed."

# Running all tests in debug mode (verbose)
run_debug: build_debug
	@echo "Running all tests in debug mode..."
	@for test in $(TEST_SRCS:%.c=%_debug) ; do \
		echo "\n--- Running $$test ---" ; \
		./$$test ; \
		if [ $$? -ne 0 ]; then \
			echo "\nTest $$test FAILED!"; \
			failed_count=$$((failed_count + 1)); \
		fi; \
	done
	@echo "\nAll tests completed."

# Memory leak check using valgrind
valgrind: build
	@echo "Running valgrind memory check on all tests..."
	@for test in $(TEST_BINS) ; do \
		echo "\n--- Checking $$test ---" ; \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$$test ; \
	done
	@echo "\nAll memory checks completed."

# Testing: compile and run all tests in debug mode
tests: build_debug
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
	rm -f $(TEST_BINS) $(TEST_SRCS:%.c=%_debug)

# Show available tests
list:
	@echo "Available tests:"
	@for test in $(TEST_SRCS) ; do \
		basename=$$(basename $${test%.c} _test); \
		echo "  make test_$$basename" ; \
	done 
	@echo "Or run all tests with 'make tests'"