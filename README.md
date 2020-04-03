# FIIT STU - Data structures and algorithms
## Memory manager project
### Author: Nizomiddin Toshpulatov
#### Date: March, 2020
 
## Content
1. **About**
2. **Implementation**
   1. Memory header
   2. Memory block and variable size headers
   3. Block allocation
   4. Block freeing
3. **Fragmentation**
4. **Conclusion** 
 
## About
 
Memory manager is a university project at FIIT STU, Bratislava. It is oriented towards deepening our knowledge as students, on how dynamic memory allocation works. To ensure our success, we were presented with several ways on how to implement this project.
 
This implementation of memory manager is based on method of *explicit list of free memory blocks*. Each time memory block is freed, it is added to the linked list of free memory blocks. Memory addressing is realized with offsets.
 
Project has 4 main functions:
 
```
void *memory_alloc(uint32_t size);
_Bool memory_free(void *valid_ptr);
_Bool memory_check(void *ptr);
void memory_init(void *ptr, uint32_t size);
```
 
## Implementation
### Memory header
 
When `memory_init` function is called, **5** variables are written to the beginning of the memory pointed by `ptr`:
 
1. `MPTR_SIZE` - size of the pointer type. `uint8_t` type number determined by the value of `size` parameter. This value dictates what pointer type will be used to store other 4 memory variables and offsets. Possible values are:
   * **1** - `uint8_t`; 
   * **2** - `uint16_t`; 
   * **4** - `uint32_t`.
2. `MSIZE` - total memory size.
3. `MFREE_SIZE` - current available memory size.
4. `MBLOCK_LIST_HEAD` - offset to the first free memory block.
5. `MBREAK` - memory break. Similar to the [`sbrk`](https://linux.die.net/man/2/sbrk) (system break) in C, which defines the end of the process's data segment (i.e., the program break is the first location after the end of the uninitialized data segment). And in traditional fashion, this variable has its shadow `mbreak` function, which acts same as as `sbrk`.
 
To reduce code duplication, initialization process is implemented in macro. And since `MPTR_SIZE` is determined during runtime, accessing memory header variables is done via macros as well.

```
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
```

Getting or setting memory variable:

```

#define get(ptr, type) (*(type *) (ptr))
#define set(ptr, value, type) (*(type *) (ptr) = (value))

#define mvar(macro, ...) (
	MPTR_SIZE == sizeof(uint8_t) ? macro(__VA_ARGS__, uint8_t) :	\
	MPTR_SIZE == sizeof(uint16_t) ? macro(__VA_ARGS__, uint16_t) :	\
	macro(__VA_ARGS__, uint32_t)									\
)
```
 
### Memory block and variable size headers
 
Each memory block has its header, which holds meta information about that block. Header structure looks like this:
```
struct mblock_meta {
    _Bool isfree: 1;
    _Bool read_next: 1;
    uint8_t size: 6;
};
```
6 bits for the size of the block seems quite small. And it is. But we have a special indicator `read_next`, which tells us whether we have to read another byte to get the full size of the block. But block size may exceed two bytes or even three...
 
To accommodate that issue, we have a special type `var_size_t` that can hold a variable amount of bytes, and it acts as a stream, rather than being a fixed sized chunk of memory. So when we see `read_next` being set to `1` and we read next byte, we read `var_size_t` byte, which looks like this:
```
struct var_size_t {
    _Bool read_next: 1;
    uint8_t data: 7;
};
```
Similar to `mblock_meta` this type has a flag to tell whether to continue reading bytes or not. This type of flexibility allows us to create variable size headers for memory blocks.
 
Reading this type of variables can be done with bit shifting. Here is a function that takes a pointer to the first `var_size_t` byte and returns `uint32_t` type value.
 
```
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
```
 
### Block allocation
 
Block allocation process consists of these steps:
* **check** - whether memory is initialized and whether requested size is a positive value and it does not exceed allowed maximum;
* **adjust size** - if requested size is barely the size of the biggest possible block at the moment, then change it to max possible block size;
* **find best fit** - go through the list of free blocks and find *best fit* for the requested size;
* **allocate block** - if no best fit was found, allocated new memory block.
 
Finding *best fit* is straightforward, go through the list of free memory blocks and compare their sizes to find the smallest block that can hold the requested size.
 
```
while (mvar(get, current)) {
 
    uint32_t current_size = mblock_size(current);
    
    if (current_size < best_fit_size && current_size >= size) {
        best_fit = current;
        best_fit_size = current_size;
    }
 
    current = MBREAK_PTR + mvar(get, current);
}
```
 
Allocating a new block is simply writing metadata and returning a pointer next to it. But since our meta header has a variable size, we also need to store its size somewhere. Size of the meta header (`meta_size`) is written right after the last byte of the block size. So the user receives the pointer `meta_size + 1`.
 
### Block freeing
 
To free memory block is basically to set its `isfree` value to `1`. But before we can do it, we have to check whether the pointer `ptr` passed to `memory_free` function is a valid pointer that has been previously allocated by `memory_alloc` function. This check is done in `memory_check` function. This function checks whether given pinter is in range of allocated memory and returns `1` if it is.
 
Ideally, after this step, we would check neighbor blocks and see if any of them is free as well. And if one or both of them are free, merge them into one block with bigger size. This would highly reduce external memory fragmentation. But sadly, this implementation does not have block merging, nor does it have block splitting. So instead, freed block is just prepended to the beginning of the list of free blocks.
 
## Fragmentation
 
Thanks to our variable size headers, internal fragmentation inside memory blocks is quite small. If block size is `<= 63` bytes, then we require just `1` byte to store all meta information. With `meta_size` this gives us a meta header with size of `2` bytes. For blocks with size from `64` to `65,535` meta header size is going to be `3` bytes. For blocks with size from `65,536` to `‭16,777,215‬` meta header size is going to be `4` bytes. And for the blocks with bigger sizes, it will require `5` byte header to store meta information.
 
External fragmentation is addressed only by looking for *best fit* block during allocation.
 
## Conclusion
 
Dynamic memory allocation is an intricate balance of speed and efficiency. Fast does not mean efficient and small header size does not mean faster access. Less fragmentation should be a priority, but with modern day hardware we can afford to *lose* few (*millions*) bytes to achieve higher speeds. This type of philosophy can be seen in standard functions such as `mman`, which allocates memory in page sizes of the system. `malloc` which utilizes `mman` for big allocations, aligns memory size to processor's word size to increase memory access speed. These small optimizations add up and make our programs run much faster. Understanding underlying principles behind dynamic memory allocation helps us to better understand how this system works and how we can use them properly or perhaps improve them even further.