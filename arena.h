#ifndef ARENA_H
# define ARENA_H

# include <stdint.h>
# include <stddef.h>
# include <stdlib.h>
# include <stdalign.h>
# include <assert.h>
# include <stdbool.h>
# include <errno.h>

# define MAX_ARENA_SIZE 1024 * 1	 								// 1 MB
# define ARENA_ALIGNMENT 16												// 8 or 16 bytes
# define ARENA_ALIGN_UP(size) (((size) + ((ARENA_ALIGNMENT) - 1)) & ~((ARENA_ALIGNMENT) - 1))
# define MAX_FREE_COUNT 10
# define MAX_BLOCK_SIZE ((1U << 31) - 1)

// Data types
typedef uint8_t		u8;
typedef uint16_t 	u16;
typedef uint32_t 	u32;
typedef uint64_t 	u64;

typedef int8_t		i8;
typedef int16_t 	i16;
typedef int32_t 	i32;
typedef int64_t 	i64;

typedef float		f32;
typedef double		f64;

// Structure of the arena
typedef struct arena_t{
	u8 				*memory;// Pointer to the allocated memory for the arena
	u64 			space;	// Remaining free space in the arena
	u64 			size;	// Total size of the arena
	u64 			offset;	// Current offset in the arena
	u8				free_count;
	struct arena_t	*child;	// Pointer to a child arena in case of overflow
} Arena;

typedef struct{
	u32 data_size;
	u32 block_used : 1;
	u32 block_size : 31;
} metadata;

/**
 * Creates a new arena with the specified size.
 * @param size The size of the arena in bytes.
 * @return A pointer to the created arena.
 */
Arena* arena_create(u64 size);

/**
 * Allocates a block of memory of the specified size in the arena.
 * @param arena A pointer to the arena.
 * @param size The size of the block to allocate in bytes.
 * @return A pointer to the allocated block.
 */
void* aalloc(Arena *arena, u64 size);

/**
 * Frees a previously allocated block of memory.
 * @param ptr A pointer to the block to free.
 */
void afree(Arena *arena, void *ptr);

/**
 * Resets the arena, freeing all allocations made.
 * @param arena A pointer to the arena.
 */
void arena_reset(Arena *arena);

/**
 * Deletes an arena and frees all associated memory.
 * @param arena A pointer to the arena to delete.
 */
void arena_delete(Arena *arena);

/**
 * Finds a free block of sufficient size in the arena.
 * @param arena A pointer to the arena.
 * @param size The size of the block to find in bytes.
 * @return A pointer to the found free block, or NULL if no free block is found.
 */
void* find_free_block(Arena *arena, u64 size);

/**
 *Mmerges adjacent free blocks in the arena into a single larger block.
 * @param arena A pointer to the arena.
 */
void merge_free_blocks(Arena *arena);

/**
 * Checks if a block of memory is free.
 * @param ptr A pointer to the block to check.
 * @return true if the block is free, false otherwise.
 */
bool is_block_free(void *ptr);

/**
 * Sets the used status of a block in the arena.
 * @param ptr A pointer to the block.
 * @param used A boolean indicating whether the block should be marked as used (true) or free (false).
 */
void set_block_used(void *ptr, bool used);

/**
 * Sets the size of a memory block while preserving its status.
 * @param ptr A pointer to the data portion of the memory block.
 * @param new_size The new size to assign to the block.
 */
void set_block_size(void *ptr, u64 new_size);

/**
 * Gets the size of an allocated block of memory.
 * @param ptr A pointer to the block.
 * @return The size of the block in bytes.
 */
u64 get_block_size(void *ptr);

/**
 * Sets the data size of a memory block.
 * @param ptr A pointer to the data portion of the memory block.
 * @param size The new data size to assign.
 */
void set_data_size(void *ptr, u32 size);

/**
 * Retrieves the data size of a memory block.
 * @param ptr A pointer to the data portion of the memory block.
 * @return The size of the data portion as an unsigned 64-bit integer.
 */
u64 get_data_size(void *ptr);


/**
 * Prints the current state of the arena, including the proportion of free and used memory.
 * @param arena A pointer to the arena.
 * @param content An indicator to print or not the content of the blocks.
 */
void print_arena(Arena *arena, bool content);

#endif