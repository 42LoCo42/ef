#pragma once

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

/** "Death" aka log-and-abort
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
if and only if the program is built in debug mode (NDEBUG not defined)
and executed by a debugger (/proc/self/status: TracerPid != 0).
*/

#ifdef NDEBUG
#define ef_breakpoint()
#else
void ef_breakpoint(void);
#endif

////////////////////////////////////////////////////////////////////////////////

#if __INCLUDE_LEVEL__ == 0 || defined(EF_IMPLEMENTATION) ///////////////////////

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

static EF_LogLevel minLevel   = EF_LL_INFO;
static const char* logTimeFmt = "%F %T";

static void printValidLogLevels(void);

#ifndef NDEBUG
static bool inDebugger = false;
#endif

static void __attribute__((constructor)) init(void) {
	_Static_assert(sizeof(f32) == 4, "we expect f32 to be 4 bytes");
	_Static_assert(sizeof(f64) == 8, "we expect f64 to be 8 bytes");

	////////////////////////////////////////

	const char* logLevel = getenv("EF_LOG");
	if(logLevel != NULL && strlen(logLevel) > 0) {
#define X(x, _)                              \
	else if(strcasecmp(logLevel, #x) == 0) { \
		minLevel = EF_LL_##x;                \
	}
		if(strcmp(logLevel, "-1") == 0 ||
		   strcmp(logLevel, "false") == 0 ||
		   strcmp(logLevel, "no") == 0) {
			minLevel = EF_LL_Count;
		}
		EF_LOGLEVELS
		else {
			char*       end   = NULL;
			EF_LogLevel level = strtol(logLevel, &end, 10);
			if(*end == 0 && level < EF_LL_Count) {
				minLevel = level;
			} else {
				ef_fail("invalid log level '%s'", logLevel);
				printValidLogLevels();
				exit(1);
			}
		}
#undef X
	}

	const char* timeFmt = getenv("EF_LOG_TS");
	if(timeFmt != NULL && strlen(timeFmt) > 0) {
		logTimeFmt = timeFmt;
	}

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
			inDebugger = true;
			break;
		}
	}

	free(line);
	fclose(statusFile);
#endif
}

////////////////////////////////////////////////////////////////////////////////

#include <stdarg.h>
#include <time.h>

void _ef_log(EF_LogLevel level, const char* fmt, ...) {
	if(level < minLevel) return;

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

#endif
