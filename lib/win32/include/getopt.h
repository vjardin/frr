// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <getopt.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_GETOPT_H
#define _FRR_WIN_GETOPT_H
extern char *optarg;
extern int optind, opterr, optopt;
struct option { const char *name; int has_arg; int *flag; int val; };
#define no_argument 0
#define required_argument 1
#define optional_argument 2
extern int getopt(int argc, char *const argv[], const char *optstring);
extern int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex);
extern int getopt_long_only(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex);
#endif
