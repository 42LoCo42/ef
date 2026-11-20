#pragma once

/** ef - extra functions (for C)
This is a header-only, single-file support library for C
which includes many useful utility functions and data structures.

You can use it in your project in two different ways:

1. Copy it into your codebase and compile it together with your other source files

2. Include it from anywhere (e.g. your system's standard include path)
   and define EF_IMPLEMENTATION in front of a *single* inclusion site
   (users of STB-style libraries will be familiar with this scheme)

Modules included in this version of ef:
- Short numerics (int -> i32 etc.)
- Environment variable helper (ef_env)
- Logging system (ef_log)
- Log-and-abort (ef_die)
- Conditional breakpoints (ef_breakpoint)
- Deferred cleanup (ef_free etc.)

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

/** Helper for a single optional macro argument
Call this as ef_pick##__VA_OPT__(1)(<default value>, __VA_ARGS__)
to return either the first vararg or <default value>.
*/

#define ef_pick(x, _) x
#define ef_pick1(_, x) x

////////////////////////////////////////////////////////////////////////////////

/** Environment variable helper
ef_env returns the value of the requested environment variable if it is set to a
non-empty string, otherwise it returns the optional second parameter or NULL.
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
*/

#include <errno.h>
#include <stdlib.h>

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

If the program is built in release mode, all calls to ef_breakpoint()
and its supporting logic will be a no-op (introduce no extra code).
*/

#ifdef NDEBUG
#define ef_breakpoint()
#else
void ef_breakpoint(void);
#endif

////////////////////////////////////////////////////////////////////////////////

/** Deferred cleanup
These macros, when used as variable declaration modifiers, mark the variable
for automatic cleanup once the scope in which it was declared is exited.
*/

#include <stdio.h>

#define ef_free __attribute__((cleanup(_ef_free)))
void _ef_free(void** arg);

#define ef_close __attribute__((cleanup(_ef_close)))
void _ef_close(int* arg);

#define ef_fclose __attribute__((cleanup(_ef_fclose)))
void _ef_fclose(FILE** arg);

////////////////////////////////////////////////////////////////////////////////

#if __INCLUDE_LEVEL__ == 0 || defined(EF_IMPLEMENTATION) ///////////////////////

#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

static EF_LogLevel logLevel = EF_LL_INFO;
static char*       logTimeFmt;

static void printValidLogLevels(void);

#ifndef NDEBUG
static bool inDebugger = false;
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

			inDebugger = true;

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
}

static void printValidLogLevels(void) {
	fprintf(stderr, "Valid log levels are:\n");
#define X(x, c) fprintf(stderr, "  [%dm%s[m (%d)\n", c, #x, EF_LL_##x);
	EF_LOGLEVELS
#undef X
	fprintf(stderr, "Or specify -1, 'false' or 'no' to disable logging\n");
}

////////////////////////////////////////////////////////////////////////////////

#ifndef NDEBUG
void ef_breakpoint(void) {
	if(inDebugger) __asm__("int3");
}
#endif

////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>

void _ef_free(void** arg) {
	free(*arg);
}

void _ef_close(int* arg) {
	close(*((int*) arg));
}

void _ef_fclose(FILE** arg) {
	fclose(*arg);
}

#endif

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
