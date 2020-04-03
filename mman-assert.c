#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "mman.h"

#define DEFAULT_MSIZE UINT16_MAX

#define mman_mhead_vars_assert(ptr, size, type) {			\
	assert(*(uint8_t *) ptr == sizeof(type));				\
															\
	type *msize = (type *) (ptr + 1);						\
	assert(*msize == size);									\
															\
	type *mfree_size = msize + 1;							\
	assert(*mfree_size == size - (sizeof(type) << 2) - 1);	\
															\
	type *mblock_list_head = mfree_size + 1;				\
	assert(*mblock_list_head == 0);							\
															\
	type *mbreak = mblock_list_head + 1;					\
	assert(*mbreak == *(uint8_t *) ptr);					\
}

#define alloc_memory(size, type) ((type *) calloc((size), sizeof(type)))
#define alloc_memory_uint8t(size) (alloc_memory(size, uint8_t))

static void mman_memory_init_mhead_vars_test(uint32_t memory_size)
{
	uint8_t *memory = alloc_memory_uint8t(memory_size);

	memory_init(memory, memory_size);

	switch (byte_size(memory_size)) {
		case sizeof(uint8_t):
			mman_mhead_vars_assert(memory, memory_size, uint8_t);
			break;
		case sizeof(uint16_t):
			mman_mhead_vars_assert(memory, memory_size, uint16_t);
			break;
		default:
			mman_mhead_vars_assert(memory, memory_size, uint32_t);
			break;
	}
}

static void mman_memory_alloc_test(uint32_t block_size)
{
	uint8_t *memory = alloc_memory_uint8t(UINT32_MAX);
	memory_init(memory, UINT32_MAX);

	assert(memory_alloc(UINT32_MAX - 1) == NULL);
	assert(memory_alloc(0) == NULL);
	assert(memory_alloc(-1) == NULL);
	assert(memory_alloc(block_size));
}

static void mman_sh_memory_alloc_test(uint32_t memory_size, size_t amount, ...)
{
	assert(amount > 0);

	uint8_t *memory = alloc_memory_uint8t(memory_size);
	memory_init(memory, memory_size);

	va_list args;
	va_start(args, amount);

	while (amount-- > 0)
		memory_alloc(va_arg(args, uint32_t));

	va_end(args);
}


static void mman_sh_memory_rand_free_test(size_t amount, ...)
{
	assert(amount > 0);

	uint8_t *memory = alloc_memory_uint8t(UINT32_MAX);
	void **mblocks = calloc(UINT32_MAX, sizeof(void *));

	memory_init(memory, UINT32_MAX);

	va_list args;
	va_start(args, amount);

	for (size_t i = 0; i < amount; i++) {

		uint32_t rand_value = rand() % UINT32_MAX;

		if (i && mblocks[rand_value])
			memory_free(mblocks[rand_value]);

		memory_alloc(va_arg(args, uint32_t));
	}

	va_end(args);
}

static void mman_sh_rand_test(
	uint32_t min_block_size, 
	uint32_t max_block_size, 
	size_t amount)
{
	assert(amount > 0);

	uint8_t *memory = alloc_memory_uint8t(UINT32_MAX);
	void **mblocks = calloc(UINT32_MAX, sizeof(void *));

	memory_init(memory, UINT32_MAX);

	for (size_t i = 0; i < amount; i++) {

		uint32_t mblock_index = rand() % UINT32_MAX;
		uint32_t mblock_size = 
			rand() % (max_block_size - min_block_size) + min_block_size;

		if (i && mblocks[mblock_index])
			memory_free(mblocks[mblock_index]);

		memory_alloc(mblock_size);
	}
}

static void mman_memory_free_test(uint32_t block_size)
{
	uint8_t *memory = alloc_memory_uint8t(UINT32_MAX);
	memory_init(memory, UINT32_MAX);

	assert(memory_free(NULL) == true);
	assert(memory_free(memory) == true);

	void *mblock = memory_alloc(block_size);

	assert(memory_free(mblock - 20) == true);
	assert(memory_free(mblock) == false);
}

static void run_test(void (*test)(uint32_t), size_t amount, ...)
{
	assert(amount > 0);

	va_list args;
	va_start(args, amount);

	while (amount-- > 0)
		test(va_arg(args, uint32_t));

	va_end(args);
}

int main(void)
{
	srand(time(NULL));
	run_mman_internal_tests();

	run_test(mman_memory_init_mhead_vars_test, 4, 100, 12563, 50000, 2143264);
	run_test(mman_memory_alloc_test, 4, 100, 23416, 453756, 1043245);
	run_test(mman_memory_free_test, 5, 1, 234, 50000, 678892);

	mman_sh_memory_alloc_test(100, 5, 8, 8, 8, 8, 8);
	mman_sh_memory_alloc_test(100, 10, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8);
	mman_sh_memory_alloc_test(100, 7, 13, 13, 13, 13);
	mman_sh_memory_alloc_test(100, 4, 24, 24, 24, 24);
	mman_sh_memory_alloc_test(200, 8, 24, 24, 24, 24, 24, 24, 24, 24);
	mman_sh_memory_alloc_test(200, 10, 20, 20, 20, 20, 20, 20, 20, 20);
	mman_sh_memory_alloc_test(200, 8, 24, 24, 24, 24, 24, 24, 24, 24);

	mman_sh_memory_alloc_test(50000, 8, 54, 245, 54, 12456, 23327, 1, 34, 53);
	
	mman_sh_memory_rand_free_test(20, 
		4324, 1237, 31267, 239491, 421,
		8, 12767, 237, 12491, 421,
		3221, 237, 367, 2991, 21,
		24, 2, 67, 3491, 4
	);

	mman_sh_rand_test(8, 50000, 7000);
	return 0;
}