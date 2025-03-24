# Makefile для компиляции и запуска тестов арена-аллокатора

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I.
DEBUG_FLAGS = -DDEBUG

TEST_DIR = tests
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(TEST_SRCS:.c=)

.PHONY: all clean run run_debug test valgrind list

# Цель по умолчанию: собрать и запустить все тесты в режиме отладки
all: clean test

# Компиляция каждого теста без отладочной информации
$(TEST_DIR)/%: $(TEST_DIR)/%.c arena.h $(TEST_DIR)/test_utils.h
	$(CC) $(CFLAGS) $< -o $@

# Компиляция каждого теста с отладочной информацией
$(TEST_DIR)/%_debug: $(TEST_DIR)/%.c arena.h $(TEST_DIR)/test_utils.h
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $< -o $@

# Компиляция всех тестов
build: $(TEST_BINS)

# Компиляция всех тестов с отладочной информацией
build_debug: $(TEST_SRCS:%.c=%_debug)

# Запуск всех тестов (без отладочной информации)
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

# Запуск всех тестов с отладочной информацией (verbose)
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

# Проверка на утечки памяти с помощью valgrind
valgrind: build
	@echo "Running valgrind memory check on all tests..."
	@for test in $(TEST_BINS) ; do \
		echo "\n--- Checking $$test ---" ; \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$$test ; \
	done
	@echo "\nAll memory checks completed."

# Тестирование: собрать и запустить все тесты в режиме отладки
test: build_debug
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

# Очистка бинарных файлов
clean:
	rm -f $(TEST_BINS) $(TEST_SRCS:%.c=%_debug)

# Показать доступные тесты
list:
	@echo "Available tests:"
	@for test in $(TEST_SRCS) ; do \
		echo "  $${test%.c}" ; \
	done 