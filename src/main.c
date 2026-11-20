#include <unistd.h>

#define EF_IMPLEMENTATION
#include "../ef.c"

#include <fcntl.h>

int main(void) {
	ef_breakpoint();

	ef_defer(printf("defer called\n"));

	bool foo = true;
	ef_defer_if(
		foo,
		printf("defer_if called\n");
		printf("foo = %d\n", foo);
	);

	for(size_t i = 0; i < 10; i++) {
		ef_free void* mem = malloc(1024);

		ef_close int fd = creat("foo", 0644);
		if(fd < 0) ef_die("failed to open foo");

		ef_fclose FILE* file = fopen("foo", "w");
		if(!file) ef_die("failed to open foo");
	}
}
