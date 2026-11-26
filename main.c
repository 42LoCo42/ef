#define _POSIX_C_SOURCE 199309L

#define EF_LIST_INIT 1
#define EF_LIST_ZERO
#include "ef.c"

#include <fcntl.h>
#include <time.h>

typedef struct Freq {
	char*  key;
	size_t count;
} Freq;

ef_key_impl(Freq) {
	ef_key(it->key, strlen(it->key));
}

int freqCompare(const void* a, const void* b) {
	const Freq* fa = a;
	const Freq* fb = b;
	return fa->count - fb->count;
}

i32 sum(i32* numbers, size_t len) {
	i32 result = 0;
	for(size_t i = 0; i < len; i++) {
		result += numbers[i];
	}
	return result;
}

int main(void) {
	const char*          path = "t8.shakespeare.txt";
	ef_auto_fclose FILE* file = fopen(path, "r");
	if(!file) ef_die("failed to open %s", path);

	size_t             cap = 1 << 30;
	ef_auto_free char* buf = malloc(cap);
	if(!buf) ef_die("failed to alloc buf");

	size_t len = fread(buf, 1, cap, file);
	ef_info("%zu bytes", len);
	buf[len] = 0;

	ef_auto_list_free ef_list_new(char*, tokens);
	for(char* str = buf;; str = NULL) {
		char* tok = strtok(str, "\n !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
		if(!tok) break;
		ef_list_put(tokens, tok);
	}
	ef_info("%zu tokens", ef_list_len(tokens));

	ef_auto_map_free ef_map_new(Freq, map);
	ef_info("%zu slots", ef_list_len(((EF_Map*) map)->ptr));

	struct timespec beg = {0};
	clock_gettime(CLOCK_MONOTONIC, &beg);

	ef_list_for(tokens, t) {
		char* tok  = tokens[t];
		Freq* freq = ef_map_get(map, .key = tok);
		freq->count++;
	}

	struct timespec end = {0};
	clock_gettime(CLOCK_MONOTONIC, &end);

	double elapsed = end.tv_sec + end.tv_nsec * 1e-9 - (beg.tv_sec + beg.tv_nsec * 1e-9);
	ef_info("took %f seconds", elapsed);

	char* key = "thy";
	ef_info("%s = %zu", key, ef_map_get(map, .key = key)->count);

#ifdef EF_MAP_STATS
	size_t cols = ef_map_collisions(map);
	ef_info("%zu collisions (%f%%)", cols, 100.0 * cols / ef_list_len(tokens));
	ef_info("load: %zu, factor %f", ef_map_load(map), ef_map_load_factor(map));
	ef_info("max slot len: %zu", ef_map_max_slot_len(map));
#endif

	ef_auto_list_free ef_list_new(Freq, hist);

	size_t empty = 0;
	ef_list_for(ef_map_ptr(map), s) {
		Freq* slot = ef_map_ptr(map)[s];
		if(!slot) {
			empty++;
			continue;
		}

#ifdef EF_LIST_STATS
		size_t reallocs = ef_list_reallocs(slot);
		if(reallocs > 0) ef_info("slot %zu reallocs %zu", s, reallocs);
#endif

		ef_list_for(slot, f) {
			Freq it = slot[f];
			ef_list_put(hist, it);
		}
	}

	ef_info("empty = %zu", empty);

	qsort(hist, ef_list_len(hist), ef_list_ets(hist), freqCompare);
	ef_info("unique tokens = %zu", ef_list_len(hist));

	/* ef_list_for(hist) { */
	/* 	Freq it = hist[i]; */
	/* 	ef_info("%5zu [%s]", it.count, it.key); */
	/* } */

	ef_auto_list_free ef_list_new(i32, numbers);

#ifdef EF_LIST_STATS
	ef_defer {
		ef_info("numbers reallocs %zu", ef_list_reallocs(numbers));
	};
#endif

	for(size_t i = 0; i < 10; i++)
		ef_list_put(numbers, i);

	int* x = ef_list_ins(numbers, 3, 69);
	int  y = ef_list_del(numbers, 1);
	int  z = ef_list_swp(numbers, 6);

	ef_note(
		"%p = %d",
		(u8*) &ef_list_get(numbers, 2),
		ef_list_get(numbers, 2)
	);

	ef_info(
		"hdr = %p, ptr = %p, [3] = %d, [1] was %d, [6] was %d, %zu/%zu",
		(u8*) ef_list_hdr(numbers),
		(u8*) numbers, *x, y, z,
		ef_list_len(numbers), ef_list_cap(numbers)
	);

	ef_list_for(numbers) ef_info("numbers[%zu] = %d", i, numbers[i]);

	ef_info("sum = %d", sum(ef_list_pl(numbers)));

	ef_breakpoint();

	ef_defer {
		ef_info("defer called");
	};

	for(size_t i = 0; i < 10; i++) {
		ef_auto_free void* mem = malloc(1024);

		ef_auto_close int fd = creat("foo", 0644);
		if(fd < 0) ef_die("failed to open foo");

		ef_auto_fclose FILE* file = fopen("foo", "w");
		if(!file) ef_die("failed to open foo");
	}
}
