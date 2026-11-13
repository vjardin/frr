// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Crash reports on Windows.
 *
 * do backtrace(), backtrace_symbols() and backtrace_symbols_fd(), as glibc
 * The frames come from RtlCaptureStackBackTrace(), their names from the
 * debug help library, which reads the program database (.pdb): the build makes
 * one per program, installed next to it. Without it, a frame is a module
 * and an offset in it, which a debugger resolves later with the .pdb of
 * that build.
 *
 * frr_win_crash_dump() writes a minidump, the core file of Windows, for the
 * crash handling of lib/win32/compat.c.
 *
 * The debug help library is single threaded. Its functions run under a
 * lock, which a crashing thread only tries to take: a crash in a thread
 * that holds it gets the plain addresses rather than a deadlock.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

#define VOID void
#define CONST const
#define IN
#define OUT
#define OPTIONAL
#define far
#define near
#include <dbghelp.h>
#undef VOID
#undef CONST
#undef IN
#undef OUT
#undef OPTIONAL
#undef far
#undef near

#include <execinfo.h>

#pragma comment(lib, "dbghelp.lib")

/* the longest line of backtrace_symbols() */
#define BT_LINE_MAX 384

#define BT_FRAMES_MAX 62

static SRWLOCK bt_lock = SRWLOCK_INIT;
static bool bt_sym_tried, bt_sym_ready;

/* the state of a thread at its crash, for backtrace() in its handler */
static _Thread_local const CONTEXT *bt_crash_context;

void frr_win_crash_dump(void *exception_pointers);
void frr_win_crash_context(const void *context);

static bool bt_sym_init(void)
{
	char dir[MAX_PATH], *sep;
	DWORD len;

	if (bt_sym_tried)
		return bt_sym_ready;
	bt_sym_tried = true;

	len = GetModuleFileNameA(NULL, dir, sizeof(dir));
	if (!len || len >= sizeof(dir))
		dir[0] = '\0';
	sep = strrchr(dir, '\\');
	if (sep)
		*sep = '\0';

	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES |
		      SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
	bt_sym_ready = SymInitialize(GetCurrentProcess(), dir[0] ? dir : NULL, TRUE);
	return bt_sym_ready;
}

static void bt_format(void *addr, char *out, size_t outsz, bool symbols)
{
	char sym_buf[sizeof(SYMBOL_INFO) + 256];
	SYMBOL_INFO *sym = (SYMBOL_INFO *)sym_buf;
	IMAGEHLP_LINE64 line = { .SizeOfStruct = sizeof(line) };
	char path[MAX_PATH] = "?";
	const char *module = path, *file, *p;
	DWORD64 a = (DWORD64)(uintptr_t)addr, disp = 0;
	DWORD line_disp = 0;
	HMODULE mod = NULL;
	int n;

	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			       (LPCSTR)addr, &mod) &&
	    GetModuleFileNameA(mod, path, sizeof(path))) {
		for (p = path; *p; p++)
			if (*p == '\\' || *p == '/')
				module = p + 1;
	}

	memset(sym, 0, sizeof(SYMBOL_INFO));
	sym->SizeOfStruct = sizeof(SYMBOL_INFO);
	sym->MaxNameLen = sizeof(sym_buf) - sizeof(SYMBOL_INFO);

	if (symbols && SymFromAddr(GetCurrentProcess(), a, &disp, sym))
		n = snprintf(out, outsz, "%s(%s+0x%llx) [%p]", module, sym->Name,
			     (unsigned long long)disp, addr);
	else
		n = snprintf(out, outsz, "%s+0x%" PRIxPTR " [%p]", module,
			     (uintptr_t)addr - (uintptr_t)mod, addr);

	if (n < 0 || (size_t)n >= outsz || !symbols ||
	    !SymGetLineFromAddr64(GetCurrentProcess(), a, &line_disp, &line) || !line.FileName)
		return;

	for (file = p = line.FileName; *p; p++)
		if (*p == '\\' || *p == '/')
			file = p + 1;
	snprintf(out + n, outsz - n, " %s:%lu", file, line.LineNumber);
}

void frr_win_crash_context(const void *context)
{
	bt_crash_context = context;
}

#if defined(_M_X64) || defined(_M_ARM64)
static int bt_unwind(const CONTEXT *start, void **buffer, int size)
{
	CONTEXT ctx = *start;
	int n;

	for (n = 0; n < size; n++) {
#ifdef _M_X64
		DWORD64 pc = ctx.Rip;
#else
		DWORD64 pc = ctx.Pc;
#endif
		PRUNTIME_FUNCTION fn;
		DWORD64 image_base, establisher;
		PVOID handler_data;

		if (!pc && n > 0)
			break;
		buffer[n] = (void *)(uintptr_t)pc;

		fn = RtlLookupFunctionEntry(pc, &image_base, NULL);
		if (!fn) {
			/* a leaf function: the return address is at hand */
#ifdef _M_X64
			ctx.Rip = *(DWORD64 *)(uintptr_t)ctx.Rsp;
			ctx.Rsp += 8;
#else
			if (ctx.Pc == ctx.Lr)
				break;
			ctx.Pc = ctx.Lr;
#endif
			continue;
		}
		RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, pc, fn, &ctx, &handler_data,
				 &establisher, NULL);
	}
	return n;
}
#endif

int backtrace(void **buffer, int size)
{
	if (size <= 0)
		return 0;

#if defined(_M_X64) || defined(_M_ARM64)
	if (bt_crash_context)
		return bt_unwind(bt_crash_context, buffer, size);
#endif

	if (size > BT_FRAMES_MAX - 1)
		size = BT_FRAMES_MAX - 1;

	/* not this function */
	return RtlCaptureStackBackTrace(1, (DWORD)size, buffer, NULL);
}

char **backtrace_symbols(void *const *buffer, int size)
{
	bool symbols;
	char **lines;
	char *text;
	int i;

	if (size <= 0)
		return NULL;

	lines = malloc((size_t)size * (sizeof(char *) + BT_LINE_MAX));
	if (!lines)
		return NULL;
	text = (char *)(lines + size);

	symbols = TryAcquireSRWLockExclusive(&bt_lock);
	if (symbols && !bt_sym_init()) {
		ReleaseSRWLockExclusive(&bt_lock);
		symbols = false;
	}

	for (i = 0; i < size; i++) {
		lines[i] = text + (size_t)i * BT_LINE_MAX;
		bt_format(buffer[i], lines[i], BT_LINE_MAX, symbols);
	}

	if (symbols)
		ReleaseSRWLockExclusive(&bt_lock);
	return lines;
}

void backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
	char line[BT_LINE_MAX + 1];
	bool symbols;
	int i;

	symbols = TryAcquireSRWLockExclusive(&bt_lock);
	if (symbols && !bt_sym_init()) {
		ReleaseSRWLockExclusive(&bt_lock);
		symbols = false;
	}

	for (i = 0; i < size; i++) {
		bt_format(buffer[i], line, BT_LINE_MAX, symbols);
		strcat(line, "\n");
		_write(fd, line, (unsigned int)strlen(line));
	}

	if (symbols)
		ReleaseSRWLockExclusive(&bt_lock);
}

/*
 * as a minidump in FRR_WIN_CRASHDIR, "<program>.<pid>.dmp",
 * when that directory exists.
 */
void frr_win_crash_dump(void *exception_pointers)
{
	MINIDUMP_EXCEPTION_INFORMATION mei = {
		.ThreadId = GetCurrentThreadId(),
		.ExceptionPointers = exception_pointers,
		.ClientPointers = FALSE,
	};
	char exe[MAX_PATH], name[MAX_PATH + 64], *prog, *dot;
	DWORD attrs, len;
	HANDLE f;
	BOOL ok;

#ifdef FRR_WIN_CRASHDIR
	attrs = GetFileAttributesA(FRR_WIN_CRASHDIR);
	if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
		return;
#else
	return;
#endif

	len = GetModuleFileNameA(NULL, exe, sizeof(exe));
	if (!len || len >= sizeof(exe))
		return;
	prog = strrchr(exe, '\\');
	prog = prog ? prog + 1 : exe;
	dot = strrchr(prog, '.');
	if (dot)
		*dot = '\0';

#ifdef FRR_WIN_CRASHDIR
	snprintf(name, sizeof(name), "%s/%s.%lu.dmp", FRR_WIN_CRASHDIR, prog,
		 GetCurrentProcessId());
#endif
	f = CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
		return;

	AcquireSRWLockExclusive(&bt_lock);
	ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
			       MiniDumpWithDataSegs | MiniDumpWithThreadInfo |
				       MiniDumpWithIndirectlyReferencedMemory,
			       exception_pointers ? &mei : NULL, NULL, NULL);
	ReleaseSRWLockExclusive(&bt_lock);
	CloseHandle(f);
	if (!ok)
		DeleteFileA(name);
}
