#define EF_LIST_ZERO
#include "ef.c"

#include <fcntl.h>

int main(void) {
	ef_list_free ef_list_new(i32, numbers);

	for(size_t i = 0; i < 10; i++)
		ef_list_add(numbers, i);

	int* x = ef_list_ins(numbers, 1, 69);
	int  y = ef_list_del(numbers, 1);

	int z = ef_list_swp(numbers, 7);

	ef_info(
		"hdr = %p, numbers = %p, numbers[1] = %p %d (was %d), %d, %zu/%zu",
		(u8*) ef_list_hdr(numbers),
		(u8*) numbers, (u8*) x, *x, y,
		z,
		ef_list_len(numbers), ef_list_cap(numbers)
	);

	ef_list_for(numbers) ef_info("numbers[%zu] = %d", i, numbers[i]);

	ef_breakpoint();

	ef_defer(ef_info("defer called"));

	bool foo = true;
	ef_defer_if(
		foo,
		ef_info("defer_if called");
		ef_info("foo = %d", foo);
	);

	for(size_t i = 0; i < 10; i++) {
		ef_free void* mem = malloc(1024);

		ef_close int fd = creat("foo", 0644);
		if(fd < 0) ef_die("failed to open foo");

		ef_fclose FILE* file = fopen("foo", "w");
		if(!file) ef_die("failed to open foo");
	}
}
