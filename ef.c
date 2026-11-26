#pragma once

/** ef - extra functions (for C)
This is a header-only, single-file support library for C
which includes many useful utility functions and data structures.

You can use it in your project in two different ways:

1. Copy it into your codebase and compile it
   together with your other source files

2. Include it from anywhere (e.g. your system's standard include path)
   and define EF_IMPLEMENTATION in front of a *single* inclusion site
   (users of STB-style libraries will be familiar with this scheme)

Modules included in this version of ef:
- Short numerics (int -> i32, …)
- Environment variable helper (ef_env)
- Logging system (ef_log, ef_<level>)
- Log-and-abort (ef_die, ef_diex)
- Conditional breakpoints (ef_breakpoint)
- Resource cleanup (ef_free, ef_auto_free, …)
- Block defer (ef_defer, ef_defer_if)
- Dynamic arrays (ef_list_new, …)

This software is released into the public domain.
For more information, see the end of this file.
*/

////////////////////////////////////////////////////////////////////////////////

/** Short numerics
Short, Rust-style names for numeric types.

EF provides both signed (prefix 'i') and unsigned (prefix 'u')
integers of 8, 16, 32, 64 and 128 bits
as well as 32- and 64-bit floating point numbers (prefix 'f').
*/

#include <stdint.h>

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

__extension__ typedef __int128 i128;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

__extension__ typedef unsigned __int128 u128;

typedef float  f32;
typedef double f64;

////////////////////////////////////////////////////////////////////////////////

/** Some helper macros */

/** Macro symbol concatenation */
#define ef_concat(a, b) ef_concat1(a, b)
#define ef_concat1(a, b) a##b

/** Helper for a single optional macro argument
Call this as ef_pick##__VA_OPT__(1)(<default value>, __VA_ARGS__)
to return either the first vararg or <default value>. */
#define ef_pick(x, _) (x)
#define ef_pick1(_, x) (x)

/** Zero-initialisation helper */
#define zalloc(x) calloc((x), 1)

/** Block expression */
#define ef_expr(...) __extension__({__VA_ARGS__})

/** Zero-size dummy var that triggers a cleanup function */
#define ef_cleanup(f)                    \
	__extension__                        \
		__attribute__((cleanup(f))) char \
		ef_concat(_ef_cleanup, __LINE__)[0]

////////////////////////////////////////////////////////////////////////////////

/** Environment variable helper
ef_env returns the value of the requested environment variable if it is set to a
non-empty string, otherwise it returns the optional second parameter or NULL.

Examples:
..char* user = ef_env("USER");
..char* home = ef_env("HOME", "/homeless-shelter");
*/

#define ef_env(name, ...) _ef_env((name), ef_pick##__VA_OPT__(1)(NULL, __VA_ARGS__))
char* _ef_env(const char* name, char* val);

////////////////////////////////////////////////////////////////////////////////

/** Logging system
In most circumstances, just call ef_<level>(fmt, ...).
Additionally, ef_log(EF_LL_<LEVEL>, fmt, ...) is provided
if you need to define the log level programmatically.

Available log levels are:
- 'note' (level 0; for debug messages; default log level in debug builds)
- 'info' (level 1; for success; default log level in release builds)
- 'warn' (level 2; for non-critical failure)
- 'fail' (level 3; for critical failure; also used by ef_die)

The minimum log level can be controlled by setting the environment variable
EF_LOG to a (case-insensitive) name of a log level, its corresponding numeric
value or one of -1, 'false' or 'no' to disable logging entirely.

Every log line has the format:
[LEVEL] [YYYY-MM-DD hh:mm:ss] file:line: func: <fmt...>
The timestamp can be changed by setting the environment variable EF_LOG_TS
to a valid format string as defined by strftime(3).

Examples:
  ef_info("the answer is %d", 6 * 7);
  ef_log(EF_LL_FAIL, "oh no!");
*/

#include <stddef.h>

typedef struct {
	char*       file;
	size_t      line;
	const char* func;
} EF_Loc;

extern EF_Loc ef_tmp_loc;

void ef_tmp_loc_reset(void*);

#define ef_loc(...) ef_expr(                              \
	ef_tmp_loc = (EF_Loc) {__FILE__, __LINE__, __func__}; \
	ef_cleanup(ef_tmp_loc_reset);                         \
	__VA_ARGS__                                           \
)

#define EF_LOGLEVELS \
	X(NOTE, 37)      \
	X(INFO, 32)      \
	X(WARN, 33)      \
	X(FAIL, 31)

typedef enum {
#define X(x, _) EF_LL_##x,
	EF_LOGLEVELS EF_LL_Count,
#undef X
} EF_LogLevel;

#define ef_log(level, fmt, ...) _ef_log(              \
	(level), (EF_Loc) {__FILE__, __LINE__, __func__}, \
	fmt __VA_OPT__(, ) __VA_ARGS__                    \
)

#define ef_note(...) ef_log(EF_LL_NOTE, __VA_ARGS__)
#define ef_info(...) ef_log(EF_LL_INFO, __VA_ARGS__)
#define ef_warn(...) ef_log(EF_LL_WARN, __VA_ARGS__)
#define ef_fail(...) ef_log(EF_LL_FAIL, __VA_ARGS__)

void _ef_log(EF_LogLevel level, EF_Loc loc, const char* fmt, ...);

////////////////////////////////////////////////////////////////////////////////

/** Log-and-abort
Both ef_die and ef_diex forward their args to ef_fail.
ef_die additionally appends the message and symbol corresponding to
the current errno.

Examples:
  // appends current errno, e.g. "No such file or directory (ENOENT)"
  int fd = open("foo", O_RDONLY);
  if(fd < 0) ef_die("failed to open foo");

  // doesn't append current error
  if(!someCondition()) ef_diex("something bad happened");
*/

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define ef_die(fmt, ...)                                \
	do {                                                \
		const char* strerrorname_np(int errnum);        \
		ef_fail(                                        \
			fmt ": %s (%s)" __VA_OPT__(, ) __VA_ARGS__, \
			strerror(errno),                            \
			strerrorname_np(errno)                      \
		);                                              \
		exit(1);                                        \
	} while(0)

#define ef_diex(fmt, ...)                        \
	do {                                         \
		ef_fail(fmt __VA_OPT__(, ) __VA_ARGS__); \
		exit(1);                                 \
	} while(0)

////////////////////////////////////////////////////////////////////////////////

/** Conditional breakpoints
ef_breakpoint will emit a debug trap (interrupt 3 aka SIGTRAP)
if and only if all of the following conditions hold:

- The program is built in debug mode (NDEBUG is not defined)

- It is being executed by a debugger/tracer (/proc/self/status: TracerPid != 0)

- The debugger is not listed in the environment variable EF_BREAKPOINT_IGNORE,
  which is a comma-separated list of command names according to
  proc_pid_comm(5). By default, strace is ignored.

If the program is built in release mode, all calls to ef_breakpoint
and its supporting logic will be a no-op (introduce no extra code).
*/

#include <stdbool.h>

#ifdef NDEBUG
#define ef_breakpoint()
#else
extern bool ef_in_debugger;
#define ef_breakpoint()                     \
	do {                                    \
		if(ef_in_debugger) __asm__("int3"); \
	} while(0)
#endif

////////////////////////////////////////////////////////////////////////////////

/** Resource cleanup
These macros come in pairs of immediate & deferred cleanup of a resource,
whereas the latter are variable declaration modifiers.
They deinitialize their respective resource and set the variable
to an invalid value, like NULL for pointers or -1 for file descriptors.

Currently implemented:
  - ef_free: for heap allocations
  - ef_list_free: for ef_lists (allocated with ef_list_new)
  - ef_close: for kernel file descriptors (int)
..- ef_fclose: for POSIX file objects (FILE*)

Examples:
  void* mem = malloc(1024);
  ef_free(mem);
  // mem was freed & is now NULL

  ef_auto_free void* mem = malloc(1024);
  // mem will be freed at scope exit
*/

#include <stdio.h>

#define ef_free(val) _ef_free(&(val))
#define ef_auto_free __attribute__((cleanup(_ef_free)))
void _ef_free(void* val);

#define ef_list_free(val) _ef_list_free(&(val))
#define ef_auto_list_free __attribute__((cleanup(_ef_list_free)))
void _ef_list_free(void* val);

#define ef_close(val) _ef_close(&(val))
#define ef_auto_close __attribute__((cleanup(_ef_close)))
void _ef_close(int* val);

#define ef_fclose(val) _ef_fclose(&(val))
#define ef_auto_fclose __attribute__((cleanup(_ef_fclose)))
void _ef_fclose(FILE** val);

////////////////////////////////////////////////////////////////////////////////

/** Block defer
Pass statements to ef_defer to execute them once the current scope is exited.
Seperate defers execute in reverse registration order, so last-to-first.
They can also capture variables in their scope.

When compiling with clang, this module requires the compiler flags
'-fblocks' and '-lBlocksRuntime' to be passed.
Alternatively, define EF_NO_DEFER if you don't want to use defer.

Examples:
  ef_defer {
    ef_info("hi from defer!");
  };
*/

#ifndef EF_NO_DEFER
#ifdef EF_LSP
#define ef_defer while(0)
#elifdef __GNUC__
#ifdef __clang__
#define EF_CALL_DEFER
void ef_call_defer(void (^*blk)(void));

#define ef_defer               \
	__attribute__((            \
		cleanup(ef_call_defer) \
	)) void (^ef_concat(_ef_defer, __LINE__))(void) = ^
#else // __clang__
#define ef_defer ef_defer1(ef_concat(_ef_defer, __LINE__))
#define ef_defer1(f)                  \
	__extension__ auto void f(void*); \
	ef_cleanup(f);                    \
	__extension__ void f(void*)
#endif // __clang__
#else  // __GNUC__
#error "ef_defer: unsupported compiler; need gcc or clang!"
#endif // __GNUC__
#endif // !EF_NO_DEFER

////////////////////////////////////////////////////////////////////////////////

/** Dynamic arrays
ef_list_new(type, name): Create a new list 'type* name'
Its initial capacity will be EF_LIST_INIT, by default 256.
If you define EF_LIST_ZERO, it will also be zero-initialised.

ef_list_grow(name): Double the list's capacity until it fits the length.
As with list creation, if EF_LIST_ZERO is defined,
the newly allocated region will be zero-initialised.

ef_list_ets(name)
ef_list_cap(name)
ef_list_len(name)
Access a list's element type size, capacity or length.
These are lvalues, meaning you can assign to & read from them.
However, you should only ever assign to length & then immediately
call ef_list_grow; everything else can mess up the list!

ef_list_hdr(name): Access the header of the list.

ef_list_get(name, i): Get the value of the i'th element of the list.
You can also take the address of the result.

ef_list_put(name, val): Add a value to the end of the list.
Returns the address of the element just added.

ef_list_pop(name): Pop the last element off the list.
Returns the value of the element just popped.

ef_list_ins(name, i, val): Insert a value into the list.
Moves everything after position 'i' to the right.
'i' can be equal to the list's length, the value is then appended.
Returns the address of the element just inserted.

ef_list_del(name, i): Delete an element from the list.
Moves everything after position 'i' to the left.
Returns the value of the element just deleted.

ef_list_swp(name, i): Swap-and-pop the last -> i'th element of the list.
Returns the value of the previous i'th element.

ef_list_for(name, i?) { body... }: Iterate over a list.
The name of the iteration parameter is optional and defaults to 'i'.

ef_list_lp(name), ef_list_pl(name): Pass length and pointer as arguments.
lp passes length, then pointer.
pl passes pointer, then length.
*/

#ifndef EF_LIST_INIT
#define EF_LIST_INIT 256
#endif

#ifdef EF_LIST_ZERO
#define ef_list_zero true
#else
#define ef_list_zero false
#endif

typedef struct {
	size_t ets; // element type size
	size_t cap;
	size_t len;
} EF_List_Hdr;

#define ef_list_bounds(...) ef_loc(_ef_list_bounds(__VA_ARGS__);)
void _ef_list_bounds(size_t i, size_t len);

EF_List_Hdr* ef_list_hdr(void* ptr);
#define ef_list_ets(ptr) ef_list_hdr(ptr)->ets
#define ef_list_cap(ptr) ef_list_hdr(ptr)->cap
#define ef_list_len(ptr) ef_list_hdr(ptr)->len

#define ef_list_new(type, name)                                 \
	type* name = ef_loc(                                        \
		_ef_list_new(sizeof(type), EF_LIST_INIT, ef_list_zero); \
	)
void* _ef_list_new(size_t ets, size_t cap, bool zero);

#define ef_list_grow(ptr) ef_loc(                 \
	_ef_list_grow((void**) &(ptr), ef_list_zero); \
)
void _ef_list_grow(void** ptr, bool zero);

#define ef_list_get(ptr, i) ((ptr)[ef_loc(_ef_list_get((ptr), (i));)])
size_t _ef_list_get(void* ptr, size_t i);

#define ef_list_put(ptr, val) ef_loc( \
	typeof(ptr) el = _ef_list_put(    \
		(void**) &(ptr)               \
	);                                \
	*el = (val); el;                  \
)
void* _ef_list_put(void** ptr);

#define ef_list_pop(ptr) ef_loc(        \
	*((typeof(ptr)) _ef_list_pop(ptr)); \
)
void* _ef_list_pop(void* ptr);

#define ef_list_ins(ptr, i, val) ef_loc( \
	typeof(ptr) el = _ef_list_ins(       \
		(void**) &(ptr), (i)             \
	);                                   \
	*el = (val); el;                     \
)
void* _ef_list_ins(void** ptr, size_t i);

#define ef_list_del(ptr, i) ef_loc(            \
	*((typeof(ptr)) _ef_list_del((ptr), (i))); \
)
void* _ef_list_del(void* ptr, size_t i);

#define ef_list_swp(ptr, i) ef_loc(            \
	*((typeof(ptr)) _ef_list_swp((ptr), (i))); \
)
void* _ef_list_swp(void* ptr, size_t i);

#define ef_list_for(ptr, ...) ef_list_for1((ptr), ef_pick##__VA_OPT__(1)(i, __VA_ARGS__))
#define ef_list_for1(ptr, i) for(size_t i = 0; i < ef_list_len(ptr); i++)

#define ef_list_lp(ptr) ef_list_len(ptr), (ptr)
#define ef_list_pl(ptr) (ptr), ef_list_len(ptr)

////////////////////////////////////////////////////////////////////////////////

#if __INCLUDE_LEVEL__ == 0 || defined(EF_IMPLEMENTATION) ///////////////////////

#include <linux/limits.h>
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>

static EF_LogLevel logLevel =
#ifdef NDEBUG
	EF_LL_INFO
#else
	EF_LL_NOTE
#endif
	;

static char* logTimeFmt;

static void printValidLogLevels(void);

#ifndef NDEBUG
bool ef_in_debugger = false;
#endif

static void __attribute__((constructor)) init(void) {
	_Static_assert(sizeof(f32) == 4, "we expect f32 to be 4 bytes");
	_Static_assert(sizeof(f64) == 8, "we expect f64 to be 8 bytes");

	////////////////////////////////////////

	const char* logLevelS = ef_env("EF_LOG");
	if(logLevelS != NULL) {
#define X(x, _)                               \
	else if(strcasecmp(logLevelS, #x) == 0) { \
		logLevel = EF_LL_##x;                 \
	}
		if(strcmp(logLevelS, "-1") == 0 ||
		   strcmp(logLevelS, "false") == 0 ||
		   strcmp(logLevelS, "no") == 0) {
			logLevel = EF_LL_Count;
		}
		EF_LOGLEVELS
		else {
			char*       end   = NULL;
			EF_LogLevel level = strtol(logLevelS, &end, 10);
			if(*end == 0 && level < EF_LL_Count) {
				logLevel = level;
			} else {
				ef_fail("invalid log level '%s'", logLevelS);
				printValidLogLevels();
				exit(1);
			}
		}
#undef X
	}

	logTimeFmt = ef_env("EF_LOG_TS", "%F %T");

	////////////////////////////////////////

#ifndef NDEBUG
	const char* statusPath = "/proc/self/status";
	FILE*       statusFile = fopen(statusPath, "r");
	if(!statusFile) ef_die("failed to open %s", statusPath);

	ssize_t getline(char** restrict lineptr, size_t* restrict n, FILE* restrict stream);

	char*   line = NULL;
	size_t  cap  = 0;
	ssize_t len  = 0;
	while((len = getline(&line, &cap, statusFile)) != -1) {
		pid_t pid = 0;
		if(sscanf(line, "TracerPid: %d", &pid) > 0 && pid != 0) {
			char commPath[PATH_MAX] = {0};
			snprintf(commPath, sizeof(commPath), "/proc/%d/comm", pid);

			FILE* commFile = fopen(commPath, "r");
			if(!commFile) ef_die("failed to open parent comm file %s", commPath);

			if((len = getline(&line, &cap, commFile)) == -1)
				ef_die("failed to read from parent comm file %s", commPath);

			line[len - 1] = 0; // remove trailing newline

			for(char* str = ef_env("EF_BREAKPOINT_IGNORE", "strace");; str = NULL) {
				const char* ignore = strtok(str, ",");
				if(!ignore) break;
				if(strcmp(line, ignore) == 0) goto end;
			}

			ef_in_debugger = true;

		end:
			break;
		}
	}

	free(line);
	fclose(statusFile);
#endif
}

////////////////////////////////////////////////////////////////////////////////

char* _ef_env(const char* name, char* val) {
	char* var = getenv(name);
	return (!var || strlen(var) == 0) ? val : var;
}

////////////////////////////////////////////////////////////////////////////////

#include <stdarg.h>
#include <time.h>

EF_Loc ef_tmp_loc;

void ef_tmp_loc_reset(void*) {
	ef_tmp_loc = (EF_Loc) {0};
}

void _ef_log(EF_LogLevel level, EF_Loc loc, const char* fmt, ...) {
	if(level < logLevel) return;

	char   buf[256] = {0};
	time_t now      = time(NULL);
	strftime(buf, sizeof(buf), logTimeFmt, localtime(&now));

	const char* levelStr = NULL;
	switch(level) {
#define X(x, c)                        \
	case EF_LL_##x:                    \
		levelStr = "\x1B[" #c "m[" #x; \
		break;
		EF_LOGLEVELS
#undef X
	default:
	}

	if(ef_tmp_loc.file) loc = ef_tmp_loc;

	fprintf(
		stderr,
		"%s] [%s] %s:%zu: %s: ",
		levelStr, buf, loc.file, loc.line, loc.func
	);

	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);

	fprintf(stderr, "\x1B[m\n");
}

static void printValidLogLevels(void) {
	fprintf(stderr, "Valid log levels are:\n");
#define X(x, c) fprintf(stderr, "  \x1B[%dm%s\x1B[m (%d)\n", c, #x, EF_LL_##x);
	EF_LOGLEVELS
#undef X
	fprintf(stderr, "Or specify -1, 'false' or 'no' to disable logging\n");
}

////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>

void _ef_free(void* _val) {
	void** val = _val;
	if(*val == NULL) return;
	free(*val);
	*val = 0;
}

void _ef_list_free(void* _val) {
	void** val = _val;
	if(*val == NULL) return;
	free(ef_list_hdr(*val));
	*val = NULL;
}

void _ef_close(int* val) {
	close(*val);
	*val = -1;
}

void _ef_fclose(FILE** val) {
	if(*val == NULL) return;
	fclose(*val);
	*val = NULL;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef EF_CALL_DEFER
void ef_call_defer(void (^*blk)(void)) {
	(*blk)();
}
#endif

////////////////////////////////////////////////////////////////////////////////

static u8 ef_list_buf[1 << 30];

void _ef_list_bounds(size_t i, size_t len) {
	if(i >= len) ef_diex("index out of range: %zu >= %zu", i, len);
}

EF_List_Hdr* ef_list_hdr(void* ptr) {
	return &((EF_List_Hdr*) (ptr))[-1];
}

void* _ef_list_new(size_t ets, size_t cap, bool zero) {
	size_t off  = sizeof(EF_List_Hdr);
	size_t size = off + ets * cap;

	void* base = zero ? zalloc(size) : malloc(size);
	if(!base) ef_die("allocation of cap %zu (%zu bytes)failed", cap, size);

	EF_List_Hdr* hdr = base;
	hdr->ets         = ets;
	hdr->cap         = cap;
	hdr->len         = 0;

	return (u8*) base + off;
}

void _ef_list_grow(void** ptr, bool zero) {
	EF_List_Hdr* hdr = ef_list_hdr(*ptr);
	if(hdr->len <= hdr->cap) return;

	size_t ets = hdr->ets;

	size_t oldCap = hdr->cap;
	while(hdr->cap < hdr->len)
		hdr->cap <<= 1;
	size_t newCap = hdr->cap;

	size_t off  = sizeof(*hdr);
	size_t size = off + ets * hdr->cap;

	u8* base = realloc(hdr, size);
	if(!base) ef_die("reallocation to cap %zu (%zu bytes) failed", newCap, size);

	if(zero) memset(
		base + off + ets * oldCap,
		0, ets * (newCap - oldCap)
	);

	*ptr = base + off;
}

size_t _ef_list_get(void* ptr, size_t i) {
	_ef_list_bounds(i, ef_list_len(ptr));
	return i;
}

void* _ef_list_put(void** ptr) {
	size_t ets = ef_list_ets(*ptr);
	size_t len = ef_list_len(*ptr)++;
	_ef_list_grow(ptr, false);

	return (u8*) *ptr + ets * len;
}

void* _ef_list_pop(void* ptr) {
	size_t ets = ef_list_ets(ptr);
	size_t len = ef_list_len(ptr)--;
	_ef_list_bounds(0, len);

	return (u8*) ptr + ets * (len - 1);
}

void* _ef_list_ins(void** ptr, size_t i) {
	size_t ets = ef_list_ets(*ptr);
	size_t len = ef_list_len(*ptr)++;
	_ef_list_bounds(i, len);

	_ef_list_grow(ptr, false);

	u8* src = (u8*) (*ptr) + ets * i;
	u8* dst = src + ets;
	memmove(dst, src, ets * (len - i));
	return src;
}

void* _ef_list_del(void* ptr, size_t i) {
	size_t ets = ef_list_ets(ptr);
	size_t len = ef_list_len(ptr)--;
	_ef_list_bounds(i, len);

	memcpy(ef_list_buf, (u8*) ptr + ets * i, ets);

	u8* dst = (u8*) ptr + ets * i;
	u8* src = dst + ets;
	memmove(dst, src, ets * (len - i - 1));

	return ef_list_buf;
}

void* _ef_list_swp(void* ptr, size_t i) {
	size_t ets = ef_list_ets(ptr);
	size_t len = ef_list_len(ptr)--;
	_ef_list_bounds(i, len);

	u8* src = (u8*) ptr + ets * (len - 1);
	u8* dst = (u8*) ptr + ets * i;
	memcpy(ef_list_buf, dst, ets);
	memcpy(dst, src, ets);

	return ef_list_buf;
}

////////////////////////////////////////////////////////////////////////////////

#endif // __INCLUDE_LEVEL__/EF_IMPLEMENTATION guard

/**
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org>
*/
