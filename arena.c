/**
 * Arena Allocator for Memory Management.
 * 
 * This file implements an arena allocator that allows efficient allocation and
 * deallocation of memory blocks. The allocator uses a primary arena and can
 * create child arenas in case of overflow. Allocated blocks are tracked using
 * a u32 containing information about the size and status (free or used).
 */

#include "arena.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// get the arena ptr len
u64
get_block_size(void *ptr) {
	if (ptr == NULL) return 0;
	u8 *real_ptr = (u8 *)ptr - sizeof(u32);
	u32 size_with_bit = *((u32 *)real_ptr);
	return size_with_bit & ~(1 << 31); 
}

bool
is_block_free(void *ptr) {
	if (ptr == NULL) return false;

	u32 *real_ptr = (u32 *)((u8 *)ptr - sizeof(u32));
	return (*real_ptr & (1 << 31)) == 0;
}

void
set_used(void *ptr, bool used) {
	if (ptr == NULL) {
		return;
	}

	u8 *real_ptr = (u8 *)ptr - sizeof(u32);
	u32 size_with_bit = *((u32 *)real_ptr);

	if (used) {
		size_with_bit |= (1 << 31);
	} else {
		size_with_bit &= ~(1 << 31);
	}

	*((u32 *)real_ptr) = size_with_bit;
}

void merge_free_blocks(Arena *arena) {
	u64 offset = 0;
	
	while (offset < arena->offset) {
		void *block_ptr = arena->memory + offset;
		void *block_data = block_ptr + sizeof(u32);
		u64 block_size = get_block_size(block_data);
		
		u64 next_offset = offset + sizeof(u32) + block_size;
		next_offset = ALIGN_UP(next_offset, ARENA_ALIGNMENT);

		if (next_offset >= arena->offset) {
			break;
		}

		void *next_block_ptr = arena->memory + next_offset;
		void *next_block_data = next_block_ptr + sizeof(u32);
		u64 next_block_size = get_block_size(next_block_data);

		if (is_block_free(block_data) && is_block_free(next_block_data)) {
			u32 new_size = block_size + next_block_size + sizeof(u32);

			*((u32 *)block_ptr) = new_size | (1 << 31);

			block_size = new_size;
		} else {
			offset = next_offset;
		}
	}
}

Arena *
arena_create(u64 size){
	if (size == 0 || size > MAX_ARENA_SIZE) {
		fprintf(stderr, "Error: invalid arena size.\n");
		return NULL;
	}

	Arena *arena = (Arena *)malloc(sizeof(Arena));
	assert(arena != NULL);
	
	arena->memory = (u8 *)malloc(size);
	assert(arena->memory != NULL);
	arena->size = size;
	arena->offset = 0;
	arena->space = size;

	return arena;
}

void *
find_free_block(Arena *arena, u64 size) {
	u64 offset = 0;
	
	while (offset < arena->offset) {
		void *block_ptr = arena->memory + offset;
		void *tmp = block_ptr + sizeof(u32);

		u64 len = get_block_size(tmp);
		if (is_block_free(tmp) && len >= size) {
			return block_ptr;
		}

		offset += len + sizeof(u32);
		offset = ALIGN_UP(offset, ARENA_ALIGNMENT);
	}

	return NULL;
}

void *
aalloc(Arena *arena, u64 size) {
	u64 total_size = size + sizeof(u32);
	u64 aligned_offset = ALIGN_UP(arena->offset, ARENA_ALIGNMENT);

	if (total_size > arena->size) {
		fprintf(stderr, "Error: arena allocation too large.\n");
		return NULL;
	}

	void *free_block_ptr = find_free_block(arena, size);

	if (!free_block_ptr && (aligned_offset + total_size > arena->size)) {
		if (arena->child) {
			return aalloc(arena->child, size);
		} else {
			arena->child = arena_create(arena->size);
			return aalloc(arena->child, size);
		}
	}

	if (free_block_ptr) {
		u64 free_offset = (u8*)free_block_ptr - arena->memory;
		aligned_offset = ALIGN_UP(free_offset, ARENA_ALIGNMENT);
		if (aligned_offset + total_size > arena->size) {
			free_block_ptr = NULL;
		}
	}

	void *ptr = free_block_ptr ? free_block_ptr : arena->memory + aligned_offset;
	u32 size_with_bit = size | (1 << 31);
	*((u32 *)ptr) = size_with_bit;
	ptr = (u8 *)ptr + sizeof(u32);

	memset(ptr, 0, size);

	if (!free_block_ptr) {
		arena->offset = aligned_offset + total_size;
		arena->space = arena->size - arena->offset;
	}

	return ptr;
}

void
afree(void *ptr) {
	if (ptr == NULL) {
		return;
	}
	u8 *real_ptr = (u8 *)ptr - sizeof(u32);
	u32 size_with_bit = *((u32 *)real_ptr);
	u32 size = size_with_bit & ~(1 << 31);

	memset(real_ptr + sizeof(u32), 0, size);

	*((u32 *)real_ptr) = size;
}

void
arena_delete(Arena *arena) {
	while (arena != NULL) {
		Arena *next = arena->child;
		free(arena->memory);
		free(arena);
		arena = next;
	}
}

void
arena_reset(Arena *arena){
	arena->offset = 0;
	arena->space = arena->size;
}

void
print_arena(Arena *arena, bool content) {
	f32 free_percent = (arena->space * 100.0) / arena->size;
	f32 used_percent = 100.0 - free_percent;

	printf("|-------------->>>\n");
	printf("| Arena -> %p:\n", arena);
	printf("| Size: %llu\n", arena->size);
	printf("| Free: %.4f%% Used: %.4f%%\n| Free: %llu byte Used: %llu byte\n", free_percent, used_percent, arena->space, arena->offset);

	if (content){
		u64 offset = 0;
		while (offset < arena->offset) {
			void *block_ptr = arena->memory + offset;
			void *tmp = block_ptr + sizeof(u32);
			
			u64 len = get_block_size(tmp);
			bool free = is_block_free(tmp);

			printf("| Block at %p: size = %llu, status = %s, content = ", block_ptr, len, free ? "free" : "used");

			for (u64 i = 0; i < len; i++) {
				printf("%02x ", ((u8 *)tmp)[i]);
			}
			printf("\n");

			offset += len + sizeof(u32);
			offset = (offset + (ARENA_ALIGNMENT - 1)) & ~(ARENA_ALIGNMENT - 1);
		}
	}
	printf("|--------------<<<\n\n");
}

int
main(){
	Arena *arena = arena_create(MAX_ARENA_SIZE);

	for (int i =0; i < 110; i++) {
		char *str = (char *) aalloc(arena, sizeof(char) * 10018);
		if (str)
			sprintf(str, "allo\n");
	}

	Arena *tmp = arena;
	while (tmp != NULL) {
		Arena *next = tmp->child;
		print_arena(tmp, false);	
		tmp = next;
	}

	arena_delete(arena);
	return 0;
}