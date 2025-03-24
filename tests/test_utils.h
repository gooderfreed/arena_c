#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Глобальные счетчики для тестов
static int tests_total = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Цвета для вывода
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Макрос для проверки условия (полный вывод)
#define ASSERT(condition, message) do { \
    tests_total++; \
    if (condition) { \
        tests_passed++; \
        printf(ANSI_COLOR_GREEN "[PASS] " ANSI_COLOR_RESET "%s\n", message); \
    } else { \
        tests_failed++; \
        printf(ANSI_COLOR_RED "[FAIL] " ANSI_COLOR_RESET "%s\n", message); \
        printf("    at %s:%d\n", __FILE__, __LINE__); \
    } \
} while (0)

// Макрос для проверки условия (тихий режим - сообщения только при ошибке)
#define ASSERT_QUIET(condition, message) do { \
    tests_total++; \
    if (condition) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf(ANSI_COLOR_RED "[FAIL] " ANSI_COLOR_RESET "%s\n", message); \
        printf("    at %s:%d\n", __FILE__, __LINE__); \
    } \
} while (0)

// Макрос для начала нового теста
#define TEST_CASE(name) do { \
    printf(ANSI_COLOR_BLUE "\n=== TEST CASE: %s ===\n" ANSI_COLOR_RESET, name); \
} while (0)

// Макрос для начала новой фазы теста
#define TEST_PHASE(name) do { \
    printf(ANSI_COLOR_YELLOW "\n--- Phase: %s ---\n" ANSI_COLOR_RESET, name); \
} while (0)

// Функция для вывода итогов тестирования
static void print_test_summary() {
    printf("\n");
    printf(ANSI_COLOR_BLUE "=== Test Results ===\n" ANSI_COLOR_RESET);
    printf("Total tests: %d\n", tests_total);
    printf(ANSI_COLOR_GREEN "Passed: %d\n" ANSI_COLOR_RESET, tests_passed);
    
    if (tests_failed > 0) {
        printf(ANSI_COLOR_RED "Failed: %d\n" ANSI_COLOR_RESET, tests_failed);
    } else {
        printf("Failed: 0\n");
    }
    
    if (tests_failed == 0) {
        printf(ANSI_COLOR_GREEN "\nAll tests passed!\n" ANSI_COLOR_RESET);
    } else {
        printf(ANSI_COLOR_RED "\nSome tests failed!\n" ANSI_COLOR_RESET);
    }
}

// Функции для проверки целостности арены
// Проверяет, что указатели не перекрываются
static bool pointers_overlap(void *p1, size_t size1, void *p2, size_t size2) {
    char *c1 = (char*)p1;
    char *c2 = (char*)p2;
    return (c1 < (c2 + size2)) && (c2 < (c1 + size1));
}

// Проверка целостности массива указателей
static void check_pointers_integrity(void **pointers, size_t *sizes, int count) {
    for (int i = 0; i < count; i++) {
        if (!pointers[i]) continue;
        
        for (int j = i + 1; j < count; j++) {
            if (!pointers[j]) continue;
            
            ASSERT_QUIET(!pointers_overlap(pointers[i], sizes[i], pointers[j], sizes[j]), 
                   "Allocated memory blocks should not overlap");
        }
    }
}

// Заполнение выделенного блока памяти тестовым шаблоном
static void fill_memory_pattern(void *ptr, size_t size, int pattern) {
    memset(ptr, pattern & 0xFF, size);
}

// Проверка сохранности шаблона в памяти
static bool verify_memory_pattern(void *ptr, size_t size, int pattern) {
    unsigned char expected = pattern & 0xFF;
    unsigned char *bytes = (unsigned char*)ptr;
    
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != expected) {
            return false;
        }
    }
    
    return true;
}

#endif // TEST_UTILS_H 