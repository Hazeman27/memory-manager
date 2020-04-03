#ifndef MMAN_H
#define MMAN_H

#include <stdint.h>

extern uint8_t byte_size(uint32_t n);

extern void run_mman_internal_tests();
extern void *memory_alloc(uint32_t size);
extern void memory_init(void *ptr, uint32_t size);

extern _Bool memory_free(void *ptr);
extern _Bool memory_check(void *ptr);

#endif