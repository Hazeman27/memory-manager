#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#define set_mhead_vars(ptr, size, type) {			\
	*(uint8_t *) ptr = sizeof(type);				\
													\
	type *msize = (type *) (ptr + 1);				\
	*msize = size;									\
													\
	type *mfree_size = msize + 1;					\
	*mfree_size = size - (sizeof(type) << 2) - 1;	\
													\
	type *mblock_list_head = mfree_size + 1;		\
	*mblock_list_head = 0;							\
													\
	type *mbreak = mblock_list_head + 1;			\
	*mbreak = *(uint8_t *) ptr;						\
}

#define MPTR_SIZE 				(*(uint8_t *) mstart)

#define MSIZE_PTR 				(mstart + 1)
#define MFREE_SIZE_PTR 			(MSIZE_PTR + MPTR_SIZE)
#define MBLOCK_LIST_HEAD_PTR 	(MFREE_SIZE_PTR + MPTR_SIZE)
#define MBREAK_PTR 				(MBLOCK_LIST_HEAD_PTR + MPTR_SIZE)

#define get(ptr, type) 			(*(type *) (ptr))
#define set(ptr, value, type) 	(*(type *) (ptr) = (value))

#define mvar(macro, ...) (MPTR_SIZE == sizeof(uint8_t) ? macro(__VA_ARGS__, uint8_t) :	\
	MPTR_SIZE == sizeof(uint16_t) ? macro(__VA_ARGS__, uint16_t) :						\
	macro(__VA_ARGS__, uint32_t)														\
)

#define MHEAD_SIZE 		((MPTR_SIZE << 2) + 1)
#define MBLOCK_MINMAX_SIZE 	((MPTR_SIZE << 1) + 2)

#define MAX_MBLOCK_SIZE 	(mvar(get, MFREE_SIZE_PTR) + MBLOCK_MINMAX_SIZE)
#define MEMORY_END_PTR 		(mstart + mvar(get, MSIZE_PTR) - 1)

struct var_size_t {
	_Bool read_next: 1;
	uint8_t data: 7;
};

struct mblock_meta {
	_Bool isfree: 1;
	_Bool read_next: 1;
	uint8_t size: 6;
};

static void *mstart = NULL;

static inline void *mbreak(uint32_t size)
{
	if (MBREAK_PTR + size > MEMORY_END_PTR)
		return NULL;

	void *last_break = MBREAK_PTR + mvar(get, MBREAK_PTR);
	mvar(set, MBREAK_PTR, mvar(get, MBREAK_PTR) + size);

	return last_break;
}

static inline void *uint32_to_var_size(void *ptr, uint32_t value)
{
	struct var_size_t *var_size = ptr;

	var_size->read_next = false;
	var_size->data = value;

	while (value >>= 7) {
		var_size->read_next = true;
		var_size++;

		var_size->read_next = false;
		var_size->data = value;
	}

	return var_size;
}

static inline uint32_t var_size_to_uint32(void *ptr)
{
	struct var_size_t *var_size = ptr;

	uint32_t value = 0;
	uint8_t bytes_read = 0;

	while (var_size->read_next) {
		value |= var_size->data << 7 * bytes_read++;
		var_size++;
	}

	return value |= var_size->data << 7 * bytes_read;
}

static inline void *write_mblock_meta(void *ptr, _Bool isfree, uint32_t size)
{
	struct mblock_meta *meta = ptr;

	meta->isfree = isfree;
	meta->read_next = false;
	meta->size = size;

	if (size >>= 6) {
		meta->read_next = true;
		return uint32_to_var_size(meta + 1, size);
	}

	return meta;
}


static inline struct mblock_meta *mblock_meta(void *ptr)
{
	uint8_t *meta_size = ptr - 1;
	return (struct mblock_meta *) ((void *) meta_size - *meta_size);
}

static inline uint32_t mblock_size(void *ptr)
{
	struct mblock_meta *meta = mblock_meta(ptr);
	uint32_t size = meta->size;

	if (meta->read_next)
		return size |= var_size_to_uint32(meta + 1) << 6;
	return size;
}

uint8_t byte_size(uint32_t n)
{
	uint8_t count = 0;

	while (n >>= 1)
		count++;

	count |= count >> 1;
	count |= count >> 2;
	count |= count >> 4;

	return ++count >> 3;
}

static inline void prepend_free_mblock(void *ptr)
{
	mvar(set, ptr, mvar(get, MBLOCK_LIST_HEAD_PTR));
	mvar(set, MBLOCK_LIST_HEAD_PTR, ptr - MBREAK_PTR);
}

_Bool memory_check(void *ptr)
{
	if (ptr == NULL || ptr <= MBREAK_PTR || ptr > MEMORY_END_PTR)
		return false;
	return true;
}

_Bool memory_free(void *ptr)
{
	if (ptr == NULL)
		goto exit_failure;

	struct mblock_meta *meta = mblock_meta(ptr);

	if (!memory_check(ptr) || meta->isfree)
		goto exit_failure;

	meta->isfree = true;
	prepend_free_mblock(ptr);

	return false;

exit_failure:
	return true;
}

void *best_fit_mblock(uint32_t size)
{
	if (!mvar(get, MBLOCK_LIST_HEAD_PTR))
		return NULL;

	void *current = MBREAK_PTR + mvar(get, MBLOCK_LIST_HEAD_PTR);
	void *best_fit = NULL;

	uint32_t best_fit_size = UINT32_MAX;

	while (mvar(get, current)) {

		uint32_t current_size = mblock_size(current);
		
		if (current_size < best_fit_size && current_size >= size) {
			best_fit = current;
			best_fit_size = current_size;
		}

		current = MBREAK_PTR + mvar(get, current);
	}

	return best_fit;
}

void *alloc_mblock(uint32_t size)
{
	void *meta = mbreak(0);
	uint8_t *meta_size = write_mblock_meta(meta, false, size) + 1;

	*meta_size = (void *) meta_size - meta; 
	void *userm = meta_size + 1;

	uint32_t total_allocated = *meta_size + 1 + size; 

	if (mbreak(total_allocated) == NULL)
		return NULL;

	mvar(set, MFREE_SIZE_PTR, mvar(get, MFREE_SIZE_PTR) - total_allocated);
	return userm;
}

void *memory_alloc(uint32_t size)
{
	if (mstart == NULL || size <= 0 || size > MAX_MBLOCK_SIZE)
		return NULL;

	if (MAX_MBLOCK_SIZE - size < MBLOCK_MINMAX_SIZE)
		size = MAX_MBLOCK_SIZE;

	void *mblock = best_fit_mblock(size);

	if (mblock == NULL)
		return alloc_mblock(size);

	return mblock;
}

void memory_init(void *ptr, uint32_t size)
{
	if (ptr == NULL || size <= 0 || size < (byte_size(size) * 6) + 3)
		return;
	
	mstart = ptr;

	switch (byte_size(size)) {
		case sizeof(uint8_t):
			set_mhead_vars(ptr, size, uint8_t);
			break;
		case sizeof(uint16_t):
			set_mhead_vars(ptr, size, uint16_t);
			break;
		default:
			set_mhead_vars(ptr, size, uint32_t);
			break;
	}
}


/**
 * *+++++++++++++++++++++*
 * |                     |
 * | Internal unit tests |
 * |                     |
 * *+++++++++++++++++++++*
 */

static void alloc_mblock_meta_test(uint32_t memory_size, uint32_t block_size)
{
	uint8_t *memory = (uint8_t *) calloc(memory_size, sizeof(uint8_t));

	memory_init(memory, memory_size);
	void *ptr = alloc_mblock(block_size);

	assert(mblock_meta(ptr)->isfree == false);
	assert(mblock_size(ptr) == block_size);
}

static void uint32_to_var_size_test(uint32_t n)
{
	uint8_t *memory = (uint8_t *) calloc(n << 1, sizeof(uint8_t));
	uint32_to_var_size(memory, n);

	assert(var_size_to_uint32(memory) == n);
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

void run_mman_internal_tests()
{
	run_test(uint32_to_var_size_test, 4, 52, 567, 50000, UINT32_MAX);

	alloc_mblock_meta_test(100, 25);
	alloc_mblock_meta_test(2351, 567);
	alloc_mblock_meta_test(500000, 50127);
	alloc_mblock_meta_test(11065432, 1065432);
}