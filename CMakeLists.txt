# SPDX-License-Identifier: GPL-2.0-or-later
#
# CMake build for FRRouting on Windows with the Microsoft C compiler (MSVC).
#
# The autotools build (configure/make) remains the build system for every
# other platform, this file only covers the Windows port. See
# doc/developer/building-frr-for-windows.rst.
#
# Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>

cmake_minimum_required(VERSION 3.21)
project(frr C)

if(NOT MSVC OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	message(FATAL_ERROR "This CMake build targets Windows with MSVC (cl.exe) only; use ./configure && make on other platforms.")
endif()

# version / options
file(READ "${CMAKE_SOURCE_DIR}/configure.ac" _frr_ac)
string(REGEX MATCH "AC_INIT\\(\\[frr\\], \\[([^]]+)\\]" _frr_m "${_frr_ac}")
set(FRR_VERSION "${CMAKE_MATCH_1}")
string(TIMESTAMP FRR_CONFDATE "%Y%m%d")
message(STATUS "FRR version ${FRR_VERSION}")

set(FRR_STATEDIR "C:/ProgramData/frr" CACHE PATH
	"Root for run/state files, config and vty sockets")
set(FRR_DEPS_PREFIX "C:/src/deps" CACHE PATH
	"Prefix where libyang was installed (include/, lib/)")

set(FRR_WIN32_EXTRA_DEFINES "")

# tools and dependencies
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
execute_process(
	COMMAND "${Python3_EXECUTABLE}" -c
		"import sys, sysconfig; print(sysconfig.get_path('include')); print(sys.base_prefix); print('python%d%d' % sys.version_info[:2])"
	OUTPUT_VARIABLE _frr_py OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
string(REPLACE "\n" ";" _frr_py "${_frr_py}")
list(GET _frr_py 0 FRR_PYTHON_INCLUDE)
list(GET _frr_py 1 FRR_PYTHONHOME)
list(GET _frr_py 2 _frr_pylib)
file(TO_CMAKE_PATH "${FRR_PYTHON_INCLUDE}" FRR_PYTHON_INCLUDE)
file(TO_CMAKE_PATH "${FRR_PYTHONHOME}" FRR_PYTHONHOME)
find_library(FRR_PYTHON_LIBRARY NAMES ${_frr_pylib}
	HINTS "${FRR_PYTHONHOME}/libs" NO_DEFAULT_PATH REQUIRED)
add_library(frr_python INTERFACE)
target_include_directories(frr_python INTERFACE "${FRR_PYTHON_INCLUDE}")
target_link_libraries(frr_python INTERFACE "${FRR_PYTHON_LIBRARY}")
find_package(Perl REQUIRED)
find_program(FRR_BISON NAMES win_bison bison REQUIRED)
find_program(FRR_FLEX NAMES win_flex flex REQUIRED)

find_package(json-c CONFIG REQUIRED)
find_package(pcre2 CONFIG REQUIRED COMPONENTS POSIX)

find_path(LIBYANG_INCLUDE_DIR libyang/libyang.h
	HINTS "${FRR_DEPS_PREFIX}/include" REQUIRED)
find_library(LIBYANG_LIBRARY yang HINTS "${FRR_DEPS_PREFIX}/lib" REQUIRED)
find_file(LIBYANG_DLL yang.dll HINTS "${FRR_DEPS_PREFIX}/lib" "${FRR_DEPS_PREFIX}/bin")

# compiler setup
set(FRR_SRC "${CMAKE_SOURCE_DIR}")
set(FRR_BLD "${CMAKE_BINARY_DIR}")

# FRR keeps its own always-on assert(); don't let NDEBUG
foreach(_cfg RELEASE RELWITHDEBINFO MINSIZEREL)
	string(REPLACE "/DNDEBUG" "" CMAKE_C_FLAGS_${_cfg} "${CMAKE_C_FLAGS_${_cfg}}")
endforeach()

set(FRR_C_OPTIONS
	/std:c17 /experimental:c11atomics /Zc:preprocessor /utf-8
	/FI${FRR_SRC}/lib/win32/frr_win32.h
	/W3
	/we4013 # undefined function, assuming extern returning int
	/we4020 # too many actual parameters
	/we4716 # function must return a value
	/wd4244 /wd4267 /wd4996 /wd4200 /wd4214 /wd4146 /wd4018 /wd4334
	/wd4116 /wd4090 /wd4057 /wd4005 /wd4068 /wd5274
)
set(FRR_C_INCLUDES
	"${FRR_SRC}/lib/win32/include"
	"${FRR_BLD}"
	"${FRR_SRC}"
	"${FRR_SRC}/include"
	"${FRR_SRC}/lib"
	"${FRR_BLD}/lib"
	"${LIBYANG_INCLUDE_DIR}"
)

function(frr_target_setup tgt)
	target_compile_options(${tgt} PRIVATE ${FRR_C_OPTIONS})
	target_include_directories(${tgt} BEFORE PRIVATE ${ARGN} ${FRR_C_INCLUDES})
	target_compile_definitions(${tgt} PRIVATE HAVE_CONFIG_H)
endfunction()

# generated headers
configure_file(lib/win32/config.h.in "${FRR_BLD}/config.h" @ONLY)

set(e_frr_runstatedir "${FRR_STATEDIR}/run")
set(e_frr_libstatedir "${FRR_STATEDIR}/lib")
set(e_yangmodelsdir "${CMAKE_INSTALL_PREFIX}/share/yang")
set(e_moduledir "${CMAKE_INSTALL_PREFIX}/lib/frr/modules")
set(e_scriptdir "${FRR_STATEDIR}/etc/scripts")
set(e_frr_sysconfdir "${FRR_STATEDIR}/etc")
set(e_vtysh_bin "${CMAKE_INSTALL_PREFIX}/bin/vtysh.exe")
set(e_watchfrr_sh "${CMAKE_INSTALL_PREFIX}/sbin/watchfrr.sh")
configure_file(lib/config_paths.h.in "${FRR_BLD}/lib/config_paths.h" @ONLY)

set(PACKAGE_NAME "frr")
set(PACKAGE_VERSION "${FRR_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/frrouting/frr/issues")
set(CONFIG_ARGS "cmake (Windows/MSVC)")
configure_file(lib/version.h.in "${FRR_BLD}/lib/version.h" @ONLY)

set(FRR_RT_ENABLED babeld bfdd bgpd eigrpd isisd fabricd ldpd nhrpd ospfd ospf6d
	pbrd pimd ripd ripngd sharpd vrrpd zebra)
list(TRANSFORM FRR_RT_ENABLED PREPEND "--enabled=" OUTPUT_VARIABLE _frr_rt_args)
add_custom_command(
	OUTPUT "${FRR_BLD}/lib/route_types.h"
	COMMAND "${PERL_EXECUTABLE}" "${FRR_SRC}/lib/route_types.pl" ${_frr_rt_args}
		< "${FRR_SRC}/lib/route_types.txt" > "${FRR_BLD}/lib/route_types.h"
	DEPENDS "${FRR_SRC}/lib/route_types.pl" "${FRR_SRC}/lib/route_types.txt"
	COMMENT "Generating lib/route_types.h")

add_custom_command(
	OUTPUT "${FRR_BLD}/lib/vtysh_daemons.h"
	COMMAND "${PERL_EXECUTABLE}" "${FRR_SRC}/vtysh/daemons.pl" zebra mgmtd bgpd ripd ripngd
		> "${FRR_BLD}/lib/vtysh_daemons.h"
	DEPENDS "${FRR_SRC}/vtysh/daemons.pl"
	COMMENT "Generating lib/vtysh_daemons.h")

add_custom_target(frr_genheaders DEPENDS
	"${FRR_BLD}/lib/route_types.h" "${FRR_BLD}/lib/vtysh_daemons.h")

# flex/bison
add_custom_command(
	OUTPUT "${FRR_BLD}/lib/command_parse.c" "${FRR_BLD}/lib/command_parse.h"
	COMMAND "${FRR_BISON}" -d "-Dapi.prefix={cmd_yy}" -Dparse.error=verbose -Wno-yacc
		-o "${FRR_BLD}/lib/command_parse.c" "${FRR_SRC}/lib/command_parse.y"
	DEPENDS "${FRR_SRC}/lib/command_parse.y"
	COMMENT "BISON lib/command_parse.y")
add_custom_command(
	OUTPUT "${FRR_BLD}/lib/command_lex.c" "${FRR_BLD}/lib/command_lex.h"
	COMMAND "${FRR_FLEX}" --nounistd "${FRR_SRC}/lib/command_lex.l"
	WORKING_DIRECTORY "${FRR_BLD}"
	DEPENDS "${FRR_SRC}/lib/command_lex.l" "${FRR_BLD}/lib/command_parse.h"
	COMMENT "FLEX lib/command_lex.l")
add_custom_command(
	OUTPUT "${FRR_BLD}/lib/defun_lex.c"
	COMMAND "${FRR_FLEX}" --nounistd "${FRR_SRC}/lib/defun_lex.l"
	WORKING_DIRECTORY "${FRR_BLD}"
	DEPENDS "${FRR_SRC}/lib/defun_lex.l"
	COMMENT "FLEX lib/defun_lex.l")
set_source_files_properties(
	"${FRR_BLD}/lib/command_lex.c" "${FRR_BLD}/lib/command_parse.c"
	PROPERTIES COMPILE_OPTIONS "/FI${FRR_SRC}/lib/win32/cmd_tokens.h")

function(frr_yang_embed outvar)
	set(_out)
	foreach(_y ${ARGN})
		set(_c "${FRR_BLD}/${_y}.c")
		get_property(_done GLOBAL PROPERTY FRR_GENERATED)
		if(NOT _c IN_LIST _done)
			add_custom_command(
				OUTPUT "${_c}"
				COMMAND "${Python3_EXECUTABLE}" "${FRR_SRC}/yang/embedmodel.py"
					"${FRR_SRC}/${_y}" "${_c}"
				DEPENDS "${FRR_SRC}/${_y}" "${FRR_SRC}/yang/embedmodel.py"
				COMMENT "YANG ${_y}")
			set_property(GLOBAL APPEND PROPERTY FRR_GENERATED "${_c}")
		endif()
		list(APPEND _out "${_c}")
	endforeach()
	set(${outvar} ${_out} PARENT_SCOPE)
endfunction()

# Windows compatibility layer
add_library(frr_win32 OBJECT lib/win32/compat.c lib/win32/pthread.c)
frr_target_setup(frr_win32)
set_source_files_properties(lib/win32/compat.c PROPERTIES
	COMPILE_DEFINITIONS FRR_WIN32_COMPAT_IMPL)

# clippy (CLI definition preprocessor, embeds Python)
add_executable(clippy
	lib/jhash.c
	lib/clippy.c
	lib/command_graph.c
	lib/command_py.c
	lib/graph.c
	lib/libfrr_trace.c
	lib/memory.c
	lib/typesafe.c
	lib/vector.c
	lib/win32/clippy_noelf.c
	"${FRR_BLD}/lib/command_lex.c"
	"${FRR_BLD}/lib/command_parse.c"
	"${FRR_BLD}/lib/defun_lex.c"
	$<TARGET_OBJECTS:frr_win32>
)
add_dependencies(clippy frr_genheaders)
frr_target_setup(clippy)
target_compile_definitions(clippy PRIVATE BUILDING_CLIPPY)
target_link_libraries(clippy PRIVATE frr_python)
foreach(_dll python3.dll "python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
	if(EXISTS "${FRR_PYTHONHOME}/${_dll}")
		add_custom_command(TARGET clippy POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${FRR_PYTHONHOME}/${_dll}" "$<TARGET_FILE_DIR:clippy>")
	endif()
endforeach()

# DEFPY: foo.c includes foo_clippy.c
function(frr_clippy outvar)
	set(_out)
	foreach(_src ${ARGN})
		string(REGEX REPLACE "\\.c$" "_clippy.c" _gen "${_src}")
		add_custom_command(
			OUTPUT "${FRR_BLD}/${_gen}"
			COMMAND "${CMAKE_COMMAND}" -E env "PYTHONHOME=${FRR_PYTHONHOME}"
				$<TARGET_FILE:clippy> "${FRR_SRC}/python/clidef.py"
				-o "${FRR_BLD}/${_gen}" "${FRR_SRC}/${_src}"
			DEPENDS clippy "${FRR_SRC}/${_src}" "${FRR_SRC}/python/clidef.py"
			COMMENT "CLIPPY ${_src}")
		list(APPEND _out "${FRR_BLD}/${_gen}")
	endforeach()
	set(${outvar} ${_out} PARENT_SCOPE)
endfunction()

set(FRR_LIB_SOURCES
	lib/admin_group.c
	lib/affinitymap.c
	lib/affinitymap_cli.c
	lib/affinitymap_northbound.c
	lib/agg_table.c
	lib/atomlist.c
	lib/asn.c
	lib/base64.c
	lib/bfd.c
	lib/buffer.c
	lib/checksum.c
	lib/command.c
	lib/command_graph.c
	lib/command_lex.l
	lib/command_match.c
	lib/command_parse.y
	lib/cspf.c
	lib/csv.c
	lib/darr.c
	lib/debug.c
	lib/defaults.c
	lib/distribute.c
	lib/explicit_bzero.c
	lib/ferr.c
	lib/filter.c
	lib/filter_cli.c
	lib/filter_nb.c
	lib/flex_algo.c
	lib/frrcu.c
	lib/frrlua.c
	lib/frrscript.c
	lib/frr_pthread.c
	lib/frrstr.c
	lib/grammar_sandbox.c
	lib/graph.c
	lib/hash.c
	lib/host_cli.c
	lib/host_nb.c
	lib/hook.c
	lib/id_alloc.c
	lib/if.c
	lib/if_rmap.c
	lib/iso.c
	lib/jhash.c
	lib/json.c
	lib/keychain.c
	lib/keychain_cli.c
	lib/keychain_nb.c
	lib/ldp_sync.c
	lib/lib_errors.c
	lib/lib_vty.c
	lib/libagentx.c
	lib/libfrr.c
	lib/libfrr_trace.c
	lib/linklist.c
	lib/link_state.c
	lib/log.c
	lib/log_cli.c
	lib/log_filter.c
	lib/log_nb.c
	lib/log_vty.c
	lib/md5.c
	lib/memory.c
	lib/mgmt_be_client.c
	lib/mgmt_fe_client.c
	lib/mgmt_msg.c
	lib/mgmt_msg_native.c
	lib/mlag.c
	lib/module.c
	lib/mpls.c
	lib/srv6.c
	lib/netlink_parser.c
	lib/network.c
	lib/nexthop.c
	lib/netns.c
	lib/nexthop_group.c
	lib/northbound.c
	lib/northbound_cli.c
	lib/northbound_db.c
	lib/northbound_notif.c
	lib/northbound_oper.c
	lib/ntop.c
	lib/openbsd-tree.c
	lib/pid_output.c
	lib/plist.c
	lib/prefix.c
	lib/privs.c
	lib/ptm_lib.c
	lib/pullwr.c
	lib/qobj.c
	lib/ringbuf.c
	lib/routemap.c
	lib/routemap_cli.c
	lib/routemap_northbound.c
	lib/sbuf.c
	lib/seqlock.c
	lib/sha256.c
	lib/sigevent.c
	lib/skiplist.c
	lib/sockopt.c
	lib/sockunion.c
	lib/spf_backoff.c
	lib/segment_routing.c
	lib/srcdest_table.c
	lib/stream.c
	lib/strformat.c
	lib/strlcat.c
	lib/strlcpy.c
	lib/systemd.c
	lib/table.c
	lib/termtable.c
	lib/event.c
	lib/typerb.c
	lib/typesafe.c
	lib/vector.c
	lib/vrf.c
	lib/vty.c
	lib/wheel.c
	lib/workqueue.c
	lib/xref.c
	lib/yang.c
	lib/yang_translator.c
	lib/yang_wrappers.c
	lib/zclient.c
	lib/zlog.c
	lib/zlog_recirculate.c
	lib/zlog_targets.c
	lib/printf/printf-pos.c
	lib/printf/vfprintf.c
	lib/printf/glue.c
	lib/routing_nb.c
	lib/routing_nb_config.c
	lib/tc.c
)
set(FRR_LIB_CLIPPY
	lib/libagentx.c
	lib/affinitymap_cli.c
	lib/host_cli.c
	lib/if.c
	lib/filter_cli.c
	lib/if_rmap.c
	lib/keychain_cli.c
	lib/log_cli.c
	lib/log_vty.c
	lib/mgmt_be_client.c
	lib/mgmt_fe_client.c
	lib/nexthop_group.c
	lib/northbound_cli.c
	lib/plist.c
	lib/routemap.c
	lib/routemap_cli.c
	lib/event.c
	lib/vty.c
)
set(FRR_LIB_YANG
	yang/frr-affinity-map.yang
	yang/frr-backend.yang
	yang/frr-filter.yang
	yang/frr-host.yang
	yang/frr-if-rmap.yang
	yang/frr-interface.yang
	yang/frr-logging.yang
	yang/frr-route-map.yang
	yang/frr-route-types.yang
	yang/frr-vrf.yang
	yang/frr-routing.yang
	yang/frr-nexthop.yang
	yang/ietf/frr-deviations-ietf-key-chain.yang
	yang/ietf/ietf-routing-types.yang
	yang/ietf/ietf-syslog-types.yang
	yang/ietf/ietf-netconf-acm.yang
	yang/ietf/ietf-key-chain.yang
	yang/ietf/ietf-interfaces.yang
	yang/ietf/ietf-bgp-types.yang
	yang/frr-module-translator.yang
	yang/ietf/ietf-srv6-types.yang
)
frr_clippy(FRR_LIB_CLIPPY_GEN ${FRR_LIB_CLIPPY})
frr_yang_embed(FRR_LIB_YANG_GEN ${FRR_LIB_YANG})

add_library(frr OBJECT
	${FRR_LIB_SOURCES}
	lib/win32/crypt.c
	lib/win32/missing.c
	lib/win32/msvc.c
	lib/win32/xref_dump.c
	${FRR_LIB_YANG_GEN}
	"${FRR_BLD}/lib/command_lex.c"
	"${FRR_BLD}/lib/command_parse.c"
)
add_custom_target(frr_lib_clippy DEPENDS ${FRR_LIB_CLIPPY_GEN})
add_dependencies(frr frr_genheaders frr_lib_clippy)
frr_target_setup(frr "${FRR_SRC}/lib/win32/assert")
target_link_libraries(frr PUBLIC json-c::json-c PCRE2::POSIX "${LIBYANG_LIBRARY}")

set(FRR_DAEMONS)
function(frr_daemon name)
	cmake_parse_arguments(_d "" "" "SOURCES;CLIPPY;YANG;LIBS" ${ARGN})
	frr_clippy(_clippy ${_d_CLIPPY})
	frr_yang_embed(_yang ${_d_YANG})
	add_executable(${name}
		${_d_SOURCES}
		${_yang}
		$<TARGET_OBJECTS:frr>
		$<TARGET_OBJECTS:frr_win32>
	)
	add_custom_target(${name}_clippy DEPENDS ${_clippy})
	add_dependencies(${name} frr_genheaders ${name}_clippy frr_lib_clippy)
	frr_target_setup(${name} "${FRR_SRC}/lib/win32/assert")
	target_link_libraries(${name} PRIVATE
		json-c::json-c PCRE2::POSIX "${LIBYANG_LIBRARY}" ${_d_LIBS})
	list(APPEND FRR_DAEMONS ${name})
	set(FRR_DAEMONS ${FRR_DAEMONS} PARENT_SCOPE)
endfunction()

# bgpd

set(BGPD_SOURCES
	bgpd/bgp_addpath.c
	bgpd/bgp_advertise.c
	bgpd/bgp_aspath.c
	bgpd/bgp_attr.c
	bgpd/bgp_attr_evpn.c
	bgpd/bgp_bfd.c
	bgpd/bgp_clist.c
	bgpd/bgp_community.c
	bgpd/bgp_community_alias.c
	bgpd/bgp_conditional_adv.c
	bgpd/bgp_damp.c
	bgpd/bgp_debug.c
	bgpd/bgp_dump.c
	bgpd/bgp_ecommunity.c
	bgpd/bgp_encap_tlv.c
	bgpd/bgp_errors.c
	bgpd/bgp_evpn.c
	bgpd/bgp_evpn_mh.c
	bgpd/bgp_evpn_vty.c
	bgpd/bgp_filter.c
	bgpd/bgp_flowspec.c
	bgpd/bgp_flowspec_util.c
	bgpd/bgp_flowspec_vty.c
	bgpd/bgp_fsm.c
	bgpd/bgp_unreach.c
	bgpd/bgp_unreach_vty.c
	bgpd/bgp_io.c
	bgpd/bgp_keepalives.c
	bgpd/bgp_label.c
	bgpd/bgp_labelpool.c
	bgpd/bgp_lcommunity.c
	bgpd/bgp_ls.c
	bgpd/bgp_ls_nlri.c
	bgpd/bgp_ls_ted.c
	bgpd/bgp_mac.c
	bgpd/bgp_memory.c
	bgpd/bgp_mpath.c
	bgpd/bgp_mplsvpn.c
	bgpd/bgp_network.c
	bgpd/bgp_nexthop.c
	bgpd/bgp_nht.c
	bgpd/bgp_open.c
	bgpd/bgp_packet.c
	bgpd/bgp_pbr.c
	bgpd/bgp_rd.c
	bgpd/bgp_regex.c
	bgpd/bgp_route.c
	bgpd/bgp_routemap.c
	bgpd/bgp_routemap_nb.c
	bgpd/bgp_routemap_nb_config.c
	bgpd/bgp_script.c
	bgpd/bgp_table.c
	bgpd/bgp_updgrp.c
	bgpd/bgp_updgrp_adv.c
	bgpd/bgp_updgrp_packet.c
	bgpd/bgp_vpn.c
	bgpd/bgp_vty.c
	bgpd/bgp_zebra.c
	bgpd/bgpd.c
	bgpd/bgp_trace.c
	bgpd/bgp_nhg.c
	bgpd/bgp_nhc.c
	bgpd/bgp_srv6.c
	bgpd/bgp_main.c
)
set(BGPD_CLIPPY
	bgpd/bgp_debug.c
	bgpd/bgp_evpn_vty.c
	bgpd/bgp_labelpool.c
	bgpd/bgp_route.c
	bgpd/bgp_routemap.c
	bgpd/bgp_unreach_vty.c
	bgpd/bgp_vty.c
	bgpd/bgp_nexthop.c
	bgpd/bgp_bfd.c
)
set(BGPD_YANG
	yang/frr-bgp-types.yang
	yang/frr-bgp.yang
	yang/frr-bgp-common-structure.yang
	yang/frr-bgp-common.yang
	yang/frr-bgp-common-multiprotocol.yang
	yang/frr-bgp-neighbor.yang
	yang/frr-bgp-peer-group.yang
	yang/frr-bgp-bmp.yang
	yang/frr-bgp-rpki.yang
	yang/frr-deviations-bgp-datacenter.yang
	yang/frr-bgp-filter.yang
	yang/frr-bgp-route-map.yang
)
frr_daemon(bgpd SOURCES ${BGPD_SOURCES} CLIPPY ${BGPD_CLIPPY} YANG ${BGPD_YANG})

set(FRR_RUNTIME_DLLS)
if(LIBYANG_DLL)
	list(APPEND FRR_RUNTIME_DLLS "${LIBYANG_DLL}")
endif()
foreach(_dll dl.dll pthreadVC3.dll pcre2-8.dll)
	find_file(_frr_dll_${_dll} ${_dll}
		HINTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
		      "${FRR_DEPS_PREFIX}/bin"
		NO_DEFAULT_PATH)
	if(_frr_dll_${_dll})
		list(APPEND FRR_RUNTIME_DLLS "${_frr_dll_${_dll}}")
	endif()
endforeach()
if(FRR_RUNTIME_DLLS)
	foreach(_daemon ${FRR_DAEMONS})
		add_custom_command(TARGET ${_daemon} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${FRR_RUNTIME_DLLS} "$<TARGET_FILE_DIR:${_daemon}>")
	endforeach()
endif()

install(TARGETS ${FRR_DAEMONS} RUNTIME DESTINATION sbin)
