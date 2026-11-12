// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * clippy without ELF support (Windows)
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include "config.h"
#include <Python.h>
#include <stdbool.h>

#include "clippy.h"

bool elf_py_init(PyObject *pymod)
{
	PyObject *err;

	Py_INCREF(Py_False);
	if (PyModule_AddObject(pymod, "elf_notes", Py_False))
		Py_DECREF(Py_False);

	/* keep the names around so "from _clippy import ..." still works */
	err = PyErr_NewException("_clippy.ELFFormatError", PyExc_ValueError,
				 NULL);
	PyModule_AddObject(pymod, "ELFFormatError", err);
	err = PyErr_NewException("_clippy.ELFAccessError", PyExc_IndexError,
				 NULL);
	PyModule_AddObject(pymod, "ELFAccessError", err);
	return true;
}
