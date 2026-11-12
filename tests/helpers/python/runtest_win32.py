# SPDX-License-Identifier: GPL-2.0-or-later
#
# Runs one wrapper of the unit tests of libfrr (tests/lib/test_*.py) the way
# pytest would, for ctest on Windows, where the build has no pytest
#
#     runtest_win32.py <wrapper.py>
#
# The wrappers run their program as lib/<name>, relative to the current
# directory: ctest runs this from the tests directory of the build.
#
# Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>

import importlib.util
import inspect
import os
import sys
import traceback


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: %s <test wrapper .py>" % sys.argv[0])

    path = os.path.abspath(sys.argv[1])
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    sys.path.insert(0, os.path.dirname(path))

    import frrtest

    name = os.path.splitext(os.path.basename(path))[0]
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    # where inspect finds the file of its classes, as frrtest needs
    sys.modules[name] = module
    spec.loader.exec_module(module)

    tests = []
    for fname, func in inspect.getmembers(module, inspect.isfunction):
        if fname.startswith("test_") and func.__module__ == name:
            tests.append((fname, func))
    for cname, cls in inspect.getmembers(module, inspect.isclass):
        if cls.__module__ != name:
            continue
        if not issubclass(cls, (frrtest._TestMultiOut, frrtest.TestRefOut)):
            continue
        obj = cls()
        for mname in sorted(dir(cls)):
            if mname.startswith("test_"):
                tests.append(("%s.%s" % (cname, mname), getattr(obj, mname)))

    failed = 0
    for tname, test in tests:
        try:
            test()
        except Exception:
            failed += 1
            print("FAIL %s" % tname)
            traceback.print_exc(file=sys.stdout)
        else:
            print("ok   %s" % tname)

    print("%s: %d passed, %d failed" % (name, len(tests) - failed, failed))
    return 1 if failed or not tests else 0


if __name__ == "__main__":
    sys.exit(main())
