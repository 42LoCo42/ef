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
- Deferred cleanup (ef_free, …)
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

/** Helper for macro symbol concatenation */

#define ef_concat(a, b) ef_concat1(a, b)
#define ef_concat1(a, b) a##b

////////////////////////////////////////////////////////////////////////////////

/** Helper for a single optional macro argument
Call this as ef_pick##__VA_OPT__(1)(<default value>, __VA_ARGS__)
to return either the first vararg or <default value>.
*/

#define ef_pick(x, _) x
#define ef_pick1(_, x) x

////////////////////////////////////////////////////////////////////////////////

/** Zero-initialisation helper */

#define zalloc(x) calloc(x, 1)

////////////////////////////////////////////////////////////////////////////////

/** Environment variable helper
ef_env returns the value of the requested environment variable if it is set to a
non-empty string, otherwise it returns the optional second parameter or NULL.

Examples:
..char* user = ef_env("USER");
..char* home = ef_env("HOME", "/homeless-shelter");
*/

#define ef_env(name, ...) _ef_env(name, ef_pick##__VA_OPT__(1)(NULL, __VA_ARGS__))
char* _ef_env(const char* name, char* val);

////////////////////////////////////////////////////////////////////////////////

/** Logging system
In most circumstances, just call ef_<level>(fmt, ...).
Additionally, ef_log(EF_LL_<LEVEL>, fmt, ...) is provided
if you need to define the log level programmatically.

Available log levels are:
- 'note' (level 0; for debug messages)
- 'info' (level 1; for success; this is the default log level)
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

#define EF_LOGLEVELS \
	X(NOTE, 37)      \
	X(INFO, 32)      \
	X(WARN, 33)      \
	X(FAIL, 31)

typedef enum {
#define X(x, _) EF_LL_##x,
	EF_LOGLEVELS
		EF_LL_Count,
#undef X
} EF_LogLevel;

#define ef_log(level, fmt, ...) _ef_log(           \
	level,                                         \
	"%s:%d: %s: " fmt "[m\n", __FILE__, __LINE__, \
	__func__ __VA_OPT__(, ) __VA_ARGS__            \
)

#define ef_note(...) ef_log(EF_LL_NOTE, __VA_ARGS__)
#define ef_info(...) ef_log(EF_LL_INFO, __VA_ARGS__)
#define ef_warn(...) ef_log(EF_LL_WARN, __VA_ARGS__)
#define ef_fail(...) ef_log(EF_LL_FAIL, __VA_ARGS__)

void _ef_log(EF_LogLevel level, const char* fmt, ...);

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

/** Deferred cleanup
These macros, when used as variable declaration modifiers, mark the variable
for automatic cleanup once the scope in which it was declared is exited.

Examples:
  ef_free      void* mem = malloc(1024);
  ef_list_free ef_list_new(int, numbers);
  ef_close     int fd = open("foo", O_RDONLY);
  ef_fclose    FILE* file = fopen("foo", "r");
*/

#include <stdio.h>

#define ef_free __attribute__((cleanup(_ef_free)))
void _ef_free(void** arg);

#define ef_list_free __attribute__((cleanup(_ef_list_free)))
void _ef_list_free(void* arg);

#define ef_close __attribute__((cleanup(_ef_close)))
void _ef_close(int* arg);

#define ef_fclose __attribute__((cleanup(_ef_fclose)))
void _ef_fclose(FILE** arg);

////////////////////////////////////////////////////////////////////////////////

/** Block defer
Pass statements to ef_defer to execute them once the current scope is exited.
Seperate defers execute in reverse registration order, so last-to-first.
They can also capture variables in their scope.

For a conditional defer, use ef_defer_if.
Note that the condition is checked once the defer actually runs,
not when it is being registered.

When compiling with clang, this module requires the compiler flags
'-fblocks' and '-lBlocksRuntime' to be passed.
Alternatively, define EF_NO_DEFER if you don't want to use defer.

Examples:
  ef_defer(puts("exiting..."));

  ef_defer_if(
    someCondition(),
    puts("conditional logic...");
    puts("multiple statements work!");
  );
*/

#ifndef EF_NO_DEFER
#ifdef __GNUC__
#ifdef __clang__
#define EF_CALL_DEFER
void ef_call_defer(void (^*blk)(void));

#define ef_defer(...)                                                            \
	__attribute__((cleanup(ef_call_defer))) void (^ef_concat(deferf, __LINE__))( \
		void                                                                     \
	) = ^{                                                                       \
	  __VA_ARGS__;                                                               \
	}
#else
#define ef_defer(...)                                              \
	__extension__ void ef_concat(deferf, __LINE__)(void*) {        \
		__VA_ARGS__;                                               \
	}                                                              \
	__extension__                                                  \
		__attribute__((cleanup(ef_concat(deferf, __LINE__)))) char \
		ef_concat(deferv, __LINE__)[0]
#endif // __clang__

#define ef_defer_if(cond, ...) ef_defer(if(!cond) return; __VA_ARGS__)
#else
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

ef_list_cap(name), ef_list_len(name): Access a list's capacity or length.
These are lvalues, meaning you can assign to & read from them.
Don't forget to call ef_list_grow(name) when you increase length!

ef_list_hdr(name): Access the header of the list.

ef_list_get(name, i): Get the value of the i'th element of the list.

ef_list_add(name, val): Add a value to the end of the list.
Returns the address of the element just addedd.

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
	size_t cap;
	size_t len;
} EF_List_Hdr;

#define ef_list_hdr(ptr) (&((EF_List_Hdr*) ptr)[-1])
#define ef_list_cap(ptr) ef_list_hdr(ptr)->cap
#define ef_list_len(ptr) ef_list_hdr(ptr)->len

#define ef_list_new(type, name) type* name = _ef_list_new(sizeof(type), EF_LIST_INIT, ef_list_zero);
void* _ef_list_new(size_t typeSize, size_t cap, bool zero);

#define ef_list_grow(ptr) (ptr = _ef_list_grow(ptr, sizeof(*ptr), ef_list_zero))
void* _ef_list_grow(void* ptr, size_t typeSize, bool zero);

#define ef_list_bounds(i, len) \
	if(i >= len) ef_diex("index out of range: %zu >= %zu", i, len)

#define ef_list_get(ptr, i) __extension__({ \
	size_t len = ef_list_len(ptr);          \
	ef_list_bounds(i, len);                 \
	ptr[i];                                 \
})

#define ef_list_add(ptr, val) __extension__({ \
	size_t len = ef_list_len(ptr)++;          \
	ef_list_grow(ptr);                        \
	ptr[len] = val;                           \
	&ptr[len];                                \
})

#define ef_list_pop(ptr) __extension__({ \
	size_t len = ef_list_len(ptr)--;     \
	ef_list_bounds(0, len);              \
	typeof(*ptr) val = ptr[len - 1];     \
	val;                                 \
})

#define ef_list_ins(ptr, i, val) __extension__({ \
	size_t len = ef_list_len(ptr)++;             \
	ef_list_bounds(i, len + 1);                  \
	ef_list_grow(ptr);                           \
	memmove(                                     \
		&ptr[i + 1], &ptr[i],                    \
		(len - i) * sizeof(*ptr)                 \
	);                                           \
	ptr[i] = val;                                \
	&ptr[i];                                     \
})

#define ef_list_del(ptr, i) __extension__({ \
	size_t len = ef_list_len(ptr)--;        \
	ef_list_bounds(i, len);                 \
	typeof(*ptr) val = ptr[i];              \
	memmove(                                \
		&ptr[i], &ptr[i + 1],               \
		(len - i - 1) * sizeof(*ptr)        \
	);                                      \
	val;                                    \
})

#define ef_list_swp(ptr, i) __extension__({ \
	typeof(*ptr) val = ef_list_get(ptr, i); \
	ptr[i]           = ef_list_pop(ptr);    \
	val;                                    \
})

#define ef_list_for(ptr, ...) ef_list_for1(ptr, ef_pick##__VA_OPT__(1)(i, __VA_ARGS__))
#define ef_list_for1(ptr, i) for(size_t i = 0; i < ef_list_len(ptr); i++)

////////////////////////////////////////////////////////////////////////////////

#if __INCLUDE_LEVEL__ == 0 || defined(EF_IMPLEMENTATION) ///////////////////////

#include <linux/limits.h>
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>

static EF_LogLevel logLevel = EF_LL_INFO;
static char*       logTimeFmt;

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

	size_t getline(char** restrict lineptr, size_t* restrict n, FILE* restrict stream);

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

void _ef_log(EF_LogLevel level, const char* fmt, ...) {
	if(level < logLevel) return;

	va_list va;
	va_start(va, fmt);

	char   buf[256] = {0};
	time_t now      = time(NULL);
	strftime(buf, sizeof(buf), logTimeFmt, localtime(&now));

	const char* levelStr = NULL;
	switch(level) {
#define X(x, c)                     \
	case EF_LL_##x:                 \
		levelStr = "[" #c "m[" #x; \
		break;
		EF_LOGLEVELS
#undef X
	default:
	}

	fprintf(stderr, "%s] [%s] ", levelStr, buf);
	vfprintf(stderr, fmt, va);

	va_end(va);
}

static void printValidLogLevels(void) {
	fprintf(stderr, "Valid log levels are:\n");
#define X(x, c) fprintf(stderr, "  [%dm%s[m (%d)\n", c, #x, EF_LL_##x);
	EF_LOGLEVELS
#undef X
	fprintf(stderr, "Or specify -1, 'false' or 'no' to disable logging\n");
}

////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>

void _ef_free(void** arg) {
	free(*arg);
}

void _ef_list_free(void* arg) {
	free(ef_list_hdr(*((void**) arg)));
}

void _ef_close(int* arg) {
	close(*arg);
}

void _ef_fclose(FILE** arg) {
	fclose(*arg);
}

////////////////////////////////////////////////////////////////////////////////

#ifdef EF_CALL_DEFER
void ef_call_defer(void (^*blk)(void)) {
	(*blk)();
}
#endif

////////////////////////////////////////////////////////////////////////////////

void* _ef_list_new(size_t typeSize, size_t cap, bool zero) {
	size_t off  = sizeof(EF_List_Hdr);
	size_t size = off + typeSize * cap;

	void* base = zero ? zalloc(size) : malloc(size);
	if(!base) ef_die("allocation of %zu bytes failed", size);

	EF_List_Hdr* hdr = base;
	hdr->cap         = cap;
	hdr->len         = 0;

	return (u8*) base + off;
}

void* _ef_list_grow(void* ptr, size_t typeSize, bool zero) {
	EF_List_Hdr* hdr = ef_list_hdr(ptr);
	if(hdr->len <= hdr->cap) return ptr;

	size_t oldCap = hdr->cap;
	while(hdr->cap < hdr->len)
		hdr->cap <<= 1;
	size_t newCap = hdr->cap;

	size_t off  = sizeof(*hdr);
	size_t size = off + typeSize * hdr->cap;

	u8* base = realloc(hdr, size);
	if(!base) ef_die("reallocation of %zu bytes failed", size);

	if(zero) memset(
		base + off + oldCap * typeSize,
		0, (newCap - oldCap) * typeSize
	);

	return base + off;
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
