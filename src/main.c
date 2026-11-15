#include <unistd.h>

#define EF_IMPLEMENTATION
#include "../ef.c"

int main(void) {
	ef_breakpoint();
	ef_note("hi from %d", 6 * 7);
	ef_info("hi from %d", 6 * 7);
	ef_warn("hi from %d", 6 * 7);
	ef_fail("hi from %d", 6 * 7);

	while(true) {
		ef_info("bla");
		sleep(1);
	}
}
