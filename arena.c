/**
* Arena Allocator for Memory Management.
* 
* This file implements an arena allocator that allows efficient allocation and
* deallocation of memory blocks. The allocator uses a primary arena and can
* create child arenas in case of overflow. Allocated blocks are tracked using
* a metadata struct containing information about the block size, alloc size
* and status (free or used).
*/

#include "arena.h"
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>

metadata *
arena_get_block_metadata(void *restrict ptr){
	if (ptr == NULL) {
		fprintf(stderr, "Error: Null pointer received in arena_get_block_metadata function.\n");
		return 0;
	}
	metadata *meta = (metadata *)((u8 *)ptr - sizeof(metadata));
	if (!meta){
		fprintf(stderr, "Error: Failed to retrieve metadata. Metadata pointer is null.\n");
		return NULL;
	}
	return meta;
}

u64
arena_get_block_size(void *restrict ptr) {
	if (ptr == NULL) {
		fprintf(stderr, "Error: Null pointer received in arena_get_block_size function.\n");
		return 0;
	}
	metadata *meta = arena_get_block_metadata(ptr);
	if (meta->block_size == 0) {
		fprintf(stderr, "Warning: Block size is zero.\n");
	}
	return meta->block_size;
}

u64
arena_strlen(void *restrict ptr) {
	if (ptr == NULL) {
		fprintf(stderr, "Error: Null pointer received in arena_get_str_size function.\n");
		return 0;
	}
	metadata *meta = arena_get_block_metadata(ptr);
	return meta->data_size - 1;
}

bool
arena_is_block_free(void *restrict ptr) {
	if (ptr == NULL) {
		fprintf(stderr, "Error: Null pointer received in arena_is_block_free function.\n");
		return false;
	}
	metadata *meta = arena_get_block_metadata(ptr);
	return meta->block_used == 0;
}

void
arena_set_block_used(void *restrict ptr, bool used) {
	if (ptr == NULL) {
		fprintf(stderr, "Error: Null pointer received in arena_set_block_used function.\n");
		return;
	}
	metadata *meta = arena_get_block_metadata(ptr);
	meta->block_used = used ? 1 : 0;
}

void
arena_set_block_size(void *restrict ptr, u64 new_size) {
	if (ptr == NULL) {
		fprintf(stderr, "Error: Null pointer received in arena_set_block_size function.\n");
		return;
	}
	metadata *meta = arena_get_block_metadata(ptr);
	meta->block_size = new_size;
}

void
arena_set_data_size(void *restrict ptr, u32 size) {
	if (ptr == NULL) {
		fprintf(stderr,"Error: Null pointer received in arena_set_data_size function.\n");
		return;
	}
	metadata *meta = arena_get_block_metadata(ptr);
	meta->data_size = size;
}

u64
arena_get_data_size(void *restrict ptr) {
	if (ptr == NULL) {
		fprintf(stderr, "Error: Null pointer received in arena_get_data_size function.\n");
		return 0;
	}
	metadata *meta = arena_get_block_metadata(ptr);
	return meta->data_size;
}

void
arena_merge_free_blocks(Arena *arena) {
	u64 offset = 0;
	while (offset < arena->offset) {
		void *block_ptr = arena->memory + offset;
		void *data_ptr = block_ptr + sizeof(metadata);
		u64 block_size = arena_get_block_size(data_ptr);
		if (offset + block_size >= arena->offset)
			break;
		void *next_block_ptr = block_ptr + block_size;
		void *next_data_ptr = next_block_ptr + sizeof(metadata);
		u64 next_block_size = arena_get_block_size(next_data_ptr);
		if (block_size < MAX_BLOCK_SIZE && arena_is_block_free(data_ptr) && arena_is_block_free(next_data_ptr)) {
			bzero(next_block_ptr, sizeof(metadata));
			arena_set_block_size(data_ptr, block_size + next_block_size);
			arena_set_block_used(data_ptr, false);
			continue;
		}
		offset += block_size;
	}
}

Arena *
arena_create(u64 size) {
	if (size == 0 || size > MAX_ARENA_SIZE) {
		fprintf(stderr, "Error: invalid arena size.\n");
		return NULL;
	}
	Arena *arena = (Arena *)malloc(sizeof(Arena));
	if (!arena) {
		fprintf(stderr, "Error: Failed to allocate arena. Reason: %s\n", strerror(errno));
		return NULL;
	}
	arena->memory = (u8 *)malloc(size);
	if (!arena->memory) {
		fprintf(stderr, "Error: Failed to allocate arena memory. Reason: %s\n", strerror(errno));
		free(arena);
		return NULL;
	}
	arena->size = size;
	arena->offset = 0;
	arena->space = size;
	arena->child = NULL;
	return arena;
}

void *
arena_find_free_block(Arena *arena, u64 size) {
	u64 offset = 0;
	while (offset < arena->offset) {
		void *block_ptr = arena->memory + offset;
		void *data_ptr = block_ptr + sizeof(metadata);
		u64 block_size = arena_get_block_size(data_ptr);

		if (arena_is_block_free(data_ptr) && block_size >= size) {
			return block_ptr;
		}

		offset += block_size;
	}
	return NULL;
}

void *
arena_alloc(Arena *arena, u64 size) {
	if (!arena) {
		fprintf(stderr, "Error: invalid arena ptr for aalloc.\n");
		return NULL;
	}
	if (size == 0) {
		fprintf(stderr, "Error: invalid size for aalloc.\n");
		return NULL;
	}
	u64 total_size = ARENA_ALIGN_UP(size + sizeof(metadata));
	if (total_size > arena->size) {
		fprintf(stderr, "Error: arena allocation too large.\n");
		return NULL;
	}
	if (total_size > MAX_BLOCK_SIZE) {
		fprintf(stderr, "Error: Block size exceeds the maximum allowable size.\n");
		return NULL;
	}
	void *free_block_ptr = arena_find_free_block(arena, total_size);
	if (free_block_ptr) {
		void *free_data_ptr = free_block_ptr + sizeof(metadata);
		u64 free_block_size = arena_get_block_size(free_data_ptr);
		if (free_block_size >= total_size + ARENA_ALIGN_UP(ARENA_ALIGNMENT + sizeof(metadata))) {
			arena_set_block_size(free_data_ptr, total_size);
			arena_set_block_used(free_data_ptr, true);
			void *split_block_ptr = free_block_ptr + total_size;
			void *split_data_ptr = split_block_ptr + sizeof(metadata);
			u64 split_block_size = free_block_size - total_size;
			arena_set_block_size(split_data_ptr, split_block_size);
			arena_set_block_used(split_data_ptr, false);
			arena_set_data_size(split_data_ptr, 0);
		} else {
			arena_set_block_used(free_data_ptr, true);
			arena_set_data_size(free_data_ptr, size);
		}
		arena->space -= arena_get_block_size(free_data_ptr);;
		return(free_data_ptr);
	}
	if (arena->offset + total_size > arena->size) {
		if (arena->child) {
			return arena_alloc(arena->child, size);
		} else {
			arena->child = arena_create(arena->size);
			return arena_alloc(arena->child, size);
		}
	}
	void *block_ptr = arena->memory + arena->offset;
	void *data_ptr = block_ptr + sizeof(metadata);
	bzero(block_ptr, total_size);
	arena_set_block_size(data_ptr, total_size);
	arena_set_block_used(data_ptr, true);
	arena_set_data_size(data_ptr, size);
	arena->offset += total_size;
	arena->space -= total_size;
	return data_ptr;
}

void
arena_free(Arena *arena, void *ptr) {
	if (ptr == NULL) {
		fprintf(stderr, "Error: invalid ptr for free.\n");
		return;
	}
	u64 block_size = arena_get_block_size(ptr);
	arena_set_data_size(ptr, 0);
	arena_set_block_used(ptr, false);
	bzero(ptr, block_size - sizeof(metadata));
	arena->space += block_size;
	arena->free_count++;
	if (arena->free_count == MAX_FREE_COUNT){
		arena_merge_free_blocks(arena);
		arena->free_count = 0;
	}
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
arena_reset(Arena *arena) {
	while (arena != NULL) {
		Arena *next = arena->child;	
		arena->offset = 0;
		arena->space = arena->size;
		arena = next;
	}
}

void
arena_print(Arena *arena, bool content) {
	f32 free_percent = (arena->space * 100.0) / arena->size;
	f32 used_percent = 100.0 - free_percent;
	printf("|-------------->>>\n");
	printf("| Arena -> %p:\n", arena);
	printf("| Size: %llu\n", arena->size);
	printf("| Free: %llu byte Used: %llu byte\n| Free: %.4f%% Used: %.4f%%\n", arena->space, arena->offset, free_percent, used_percent);
	if (content){
		u64 offset = 0;
		while (offset < arena->offset) {
			void *block_ptr = arena->memory + offset;
			void *data_ptr = block_ptr + sizeof(metadata);
			u64 block_size = arena_get_block_size(data_ptr);
			u64 data_size = arena_get_data_size(data_ptr);
			bool free = arena_is_block_free(data_ptr);
			printf("| Block at %p: data_size = %llu, block_size = %llu, block_status = %s, content = ", block_ptr, data_size, block_size, free ? "free" : "used");
			for (u64 i = 0; i < block_size - sizeof(metadata); i++) {
				printf("%02x ", ((u8 *)data_ptr)[i]);
			}
			printf("\n");
			offset += block_size;
		}
	}
	printf("|--------------<<<\n\n");
}
