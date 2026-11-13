# SPDX-License-Identifier: GPL-2.0-or-later
#
# PoC of signature when -DFRR_SIGN_ARGS is set
#
# Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>

if(DEFINED CPACK_PACKAGE_FILES)
	set(_files ${CPACK_PACKAGE_FILES})
else()
	file(GLOB_RECURSE _files
		"${CPACK_TEMPORARY_DIRECTORY}/*.exe" "${CPACK_TEMPORARY_DIRECTORY}/*.dll")
	list(FILTER _files EXCLUDE REGEX "/vcruntime[0-9_]*\\.dll$")
endif()
list(LENGTH _files _count)
if(_count EQUAL 0)
	message(FATAL_ERROR "nothing to sign in ${CPACK_TEMPORARY_DIRECTORY}")
endif()
message(STATUS "Signing ${_count} files")
execute_process(COMMAND "${CPACK_FRR_SIGNTOOL}" sign ${CPACK_FRR_SIGN_ARGS} ${_files}
	RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
	message(FATAL_ERROR "signtool sign failed (${_rc})")
endif()
