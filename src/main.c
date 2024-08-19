#include "../include/arena.h"

int main(){
	Arena *arena = arena_create(MAX_ARENA_SIZE);
	int *ptr[16];
	for (int i = 0; i < 16; i++) {
		int *p = arena_alloc(arena, 8);
		*p = i;
		ptr[i] = p;
	}

	arena_print_child(arena, false);

	for (int i = 0; i < 16; i++) {
		if (i == 15 || i == 14 || i == 13 || i == 12 || i == 11)
			arena_free(arena, ptr[i]);
	}


	arena_print_child(arena, false);
	
	// arena_free(arena, ptr[10]);

	arena_print_child(arena, false);

	arena_delete(arena);
	return 0;
}