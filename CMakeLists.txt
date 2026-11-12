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

set(FRR_NPCAP_SDK "" CACHE PATH
	"Npcap SDK (Include/, Lib/); isisd and fabricd are built when it is set")

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

set(FRR_HOST_CLIPPY "" CACHE FILEPATH
	"clippy.exe of a native build, for a build of another architecture")
if(NOT FRR_HOST_CLIPPY)
	find_library(FRR_PYTHON_LIBRARY NAMES ${_frr_pylib}
		HINTS "${FRR_PYTHONHOME}/libs" NO_DEFAULT_PATH REQUIRED)
	add_library(frr_python INTERFACE)
	target_include_directories(frr_python INTERFACE "${FRR_PYTHON_INCLUDE}")
	target_link_libraries(frr_python INTERFACE "${FRR_PYTHON_LIBRARY}")
endif()
find_package(Perl REQUIRED)
find_program(FRR_BISON NAMES win_bison bison REQUIRED)
find_program(FRR_FLEX NAMES win_flex flex REQUIRED)

find_package(json-c CONFIG REQUIRED)
find_package(pcre2 CONFIG REQUIRED COMPONENTS POSIX)
find_package(unofficial-readline-win32 CONFIG REQUIRED)

find_path(LIBYANG_INCLUDE_DIR libyang/libyang.h
	HINTS "${FRR_DEPS_PREFIX}/include" REQUIRED)
find_library(LIBYANG_LIBRARY yang HINTS "${FRR_DEPS_PREFIX}/lib" REQUIRED)
find_file(LIBYANG_DLL yang.dll HINTS "${FRR_DEPS_PREFIX}/lib" "${FRR_DEPS_PREFIX}/bin")

set(FRR_VTYSH_DAEMON_NAMES zebra mgmtd bgpd ripd ripngd ospfd ospf6d
	staticd eigrpd babeld bfdd sharpd)
if(FRR_NPCAP_SDK)
	if(CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "ARM64")
		set(_frr_npcap_lib "${FRR_NPCAP_SDK}/Lib/ARM64")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_frr_npcap_lib "${FRR_NPCAP_SDK}/Lib/x64")
	else()
		set(_frr_npcap_lib "${FRR_NPCAP_SDK}/Lib")
	endif()
	find_path(NPCAP_INCLUDE_DIR pcap.h
		HINTS "${FRR_NPCAP_SDK}/Include" NO_DEFAULT_PATH REQUIRED)
	find_library(NPCAP_LIBRARY wpcap
		HINTS "${_frr_npcap_lib}" NO_DEFAULT_PATH REQUIRED)
	set(HAVE_ISISD 1)
	set(HAVE_FABRICD 1)
	list(APPEND FRR_VTYSH_DAEMON_NAMES isisd fabricd)
else()
	message(STATUS "FRR_NPCAP_SDK is not set: isisd and fabricd are not built")
endif()

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
set(e_watchfrr_sh "\\\"${CMAKE_INSTALL_PREFIX}/sbin/watchfrr.cmd\\\"")
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
	COMMAND "${PERL_EXECUTABLE}" "${FRR_SRC}/vtysh/daemons.pl" ${FRR_VTYSH_DAEMON_NAMES}
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
add_library(frr_win32 OBJECT lib/win32/compat.c lib/win32/pthread.c lib/win32/backtrace.c)
frr_target_setup(frr_win32)
set_source_files_properties(lib/win32/backtrace.c PROPERTIES
	COMPILE_DEFINITIONS "FRR_WIN_CRASHDIR=\"${FRR_STATEDIR}/crash\"")
set_source_files_properties(lib/win32/compat.c PROPERTIES
	COMPILE_DEFINITIONS FRR_WIN32_COMPAT_IMPL)

# clippy (CLI definition preprocessor, embeds Python)
if(FRR_HOST_CLIPPY)
	add_executable(clippy IMPORTED)
	set_target_properties(clippy PROPERTIES IMPORTED_LOCATION "${FRR_HOST_CLIPPY}")
else()
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
endif()

# DEFPY: foo.c includes foo_clippy.c
function(frr_clippy outvar)
	set(_out)
	foreach(_src ${ARGN})
		string(REGEX REPLACE "\\.c$" "_clippy.c" _gen "${_src}")
		get_property(_done GLOBAL PROPERTY FRR_GENERATED)
		if(NOT "${FRR_BLD}/${_gen}" IN_LIST _done)
			add_custom_command(
				OUTPUT "${FRR_BLD}/${_gen}"
				COMMAND "${CMAKE_COMMAND}" -E env "PYTHONHOME=${FRR_PYTHONHOME}"
					$<TARGET_FILE:clippy> "${FRR_SRC}/python/clidef.py"
					-o "${FRR_BLD}/${_gen}" "${FRR_SRC}/${_src}"
				DEPENDS clippy "${FRR_SRC}/${_src}" "${FRR_SRC}/python/clidef.py"
				COMMENT "CLIPPY ${_src}")
			set_property(GLOBAL APPEND PROPERTY FRR_GENERATED "${FRR_BLD}/${_gen}")
		endif()
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
	lib/zlog_live.c
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

enable_language(RC)
get_filename_component(_frr_rc_dir "${CMAKE_RC_COMPILER}" DIRECTORY)
find_program(FRR_MC_COMPILER mc HINTS "${_frr_rc_dir}" REQUIRED)
add_custom_command(
	OUTPUT "${FRR_BLD}/eventlog.rc" "${FRR_BLD}/eventlog.h" "${FRR_BLD}/MSG00409.bin"
	COMMAND "${FRR_MC_COMPILER}" -U -h "${FRR_BLD}" -r "${FRR_BLD}"
		"${FRR_SRC}/lib/win32/eventlog.mc"
	DEPENDS "${FRR_SRC}/lib/win32/eventlog.mc"
	WORKING_DIRECTORY "${FRR_BLD}"
	COMMENT "Generating the Event Log message table"
	VERBATIM)

set(FRR_DAEMONS)
function(frr_daemon name)
	cmake_parse_arguments(_d "" "" "SOURCES;CLIPPY;YANG;LIBS" ${ARGN})
	frr_clippy(_clippy ${_d_CLIPPY})
	frr_yang_embed(_yang ${_d_YANG})
	add_executable(${name}
		${_d_SOURCES}
		${_yang}
		"${FRR_BLD}/eventlog.rc"
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

set(ZEBRA_SOURCES
	zebra/if_win32.c
	zebra/ipforward_win32.c
	zebra/kernel_win32.c
	zebra/rt_win32.c
	zebra/connected.c
	zebra/debug.c
	zebra/if_socket.c
	zebra/interface.c
	zebra/label_manager.c
	zebra/main.c
	zebra/redistribute.c
	zebra/router-id.c
	zebra/rtadv.c
	zebra/rule_socket.c
	zebra/table_manager.c
	zebra/tc_socket.c
	zebra/zapi_msg.c
	zebra/zebra_affinitymap.c
	zebra/zebra_dplane.c
	zebra/zebra_errors.c
	zebra/zebra_gr.c
	zebra/zebra_l2.c
	zebra/zebra_l2_bridge_if.c
	zebra/zebra_evpn.c
	zebra/zebra_evpn_mac.c
	zebra/zebra_evpn_neigh.c
	zebra/zebra_mlag.c
	zebra/zebra_mlag_vty.c
	zebra/zebra_mpls.c
	zebra/zebra_mpls_null.c
	zebra/zebra_mpls_vty.c
	zebra/zebra_srv6.c
	zebra/zebra_srv6_vty.c
	zebra/zebra_mroute.c
	zebra/zebra_nb.c
	zebra/zebra_nb_config.c
	zebra/zebra_nb_rpcs.c
	zebra/zebra_nb_state.c
	zebra/zebra_netns_id.c
	zebra/zebra_netns_notify.c
	zebra/zebra_nhg.c
	zebra/zebra_ns.c
	zebra/zebra_opaque.c
	zebra/zebra_pbr.c
	zebra/zebra_ptm.c
	zebra/zebra_ptm_redistribute.c
	zebra/zebra_pw.c
	zebra/zebra_rib.c
	zebra/zebra_router.c
	zebra/zebra_rnh.c
	zebra/zebra_routemap.c
	zebra/zebra_routemap_nb.c
	zebra/zebra_routemap_nb_config.c
	zebra/zebra_script.c
	zebra/zebra_srte.c
	zebra/zebra_tc.c
	zebra/zebra_trace.c
	zebra/zebra_vrf.c
	zebra/zebra_vty.c
	zebra/zebra_vxlan.c
	zebra/zebra_vxlan_if.c
	zebra/zebra_evpn_mh.c
	zebra/zebra_neigh.c
	zebra/zserv.c
)
set(ZEBRA_CLIPPY
	zebra/debug.c
	zebra/interface.c
	zebra/rtadv.c
	zebra/zebra_mlag_vty.c
	zebra/zebra_routemap.c
	zebra/zebra_vty.c
	zebra/zebra_srv6_vty.c
	zebra/zebra_vrf.c
	zebra/zserv.c
	zebra/label_manager.c
)
set(ZEBRA_YANG
	yang/frr-zebra.yang
	yang/frr-zebra-route-map.yang
)
frr_daemon(zebra SOURCES ${ZEBRA_SOURCES} CLIPPY ${ZEBRA_CLIPPY} YANG ${ZEBRA_YANG}
	LIBS iphlpapi cfgmgr32)

set(MGMTD_SOURCES
	mgmtd/mgmt.c
	mgmtd/mgmt_ds.c
	mgmtd/mgmt_be_adapter.c
	mgmtd/mgmt_fe_adapter.c
	mgmtd/mgmt_history.c
	mgmtd/mgmt_memory.c
	mgmtd/mgmt_txn.c
	mgmtd/mgmt_txn_cfg.c
	mgmtd/mgmt_vty.c
	mgmtd/mgmt_vty_frontend.c
	mgmtd/mgmt_main.c
)
add_library(mgmt_be_nb OBJECT
	mgmtd/mgmt_be_nb.c
	zebra/zebra_cli.c
	ripd/rip_cli.c
	ripngd/ripng_cli.c
	staticd/static_vty.c
)
frr_target_setup(mgmt_be_nb "${FRR_SRC}/lib/win32/assert")
target_compile_definitions(mgmt_be_nb PRIVATE INCLUDE_MGMTD_CMDDEFS_ONLY)
target_link_libraries(mgmt_be_nb PRIVATE json-c::json-c PCRE2::POSIX)
add_dependencies(mgmt_be_nb frr_genheaders frr_lib_clippy mgmtd_clippy)
frr_daemon(mgmtd
	SOURCES ${MGMTD_SOURCES} $<TARGET_OBJECTS:mgmt_be_nb>
	CLIPPY
		mgmtd/mgmt_vty.c
		zebra/zebra_cli.c
		ripd/rip_cli.c
		ripngd/ripng_cli.c
		staticd/static_vty.c
	YANG
		yang/frr-zebra.yang
		yang/frr-zebra-route-map.yang
		yang/ietf/ietf-netconf.yang
		yang/ietf/ietf-netconf-with-defaults.yang
		yang/frr-bfdd.yang
		yang/frr-ripd.yang
		yang/frr-ripngd.yang
		yang/frr-staticd.yang
)

# ripd, ripngd

frr_daemon(ripd
	SOURCES
		ripd/rip_bfd.c
		ripd/rip_debug.c
		ripd/rip_errors.c
		ripd/rip_interface.c
		ripd/rip_offset.c
		ripd/rip_main.c
		ripd/rip_nb.c
		ripd/rip_nb_config.c
		ripd/rip_nb_rpcs.c
		ripd/rip_nb_notifications.c
		ripd/rip_nb_state.c
		ripd/rip_peer.c
		ripd/rip_routemap.c
		ripd/rip_zebra.c
		ripd/ripd.c
	CLIPPY
		ripd/rip_bfd.c
	YANG
		yang/frr-ripd.yang
		yang/frr-bfdd.yang
)

frr_daemon(ripngd
	SOURCES
		ripngd/ripng_debug.c
		ripngd/ripng_interface.c
		ripngd/ripng_nexthop.c
		ripngd/ripng_offset.c
		ripngd/ripng_main.c
		ripngd/ripng_nb.c
		ripngd/ripng_nb_config.c
		ripngd/ripng_nb_rpcs.c
		ripngd/ripng_nb_state.c
		ripngd/ripng_peer.c
		ripngd/ripng_route.c
		ripngd/ripng_routemap.c
		ripngd/ripng_zebra.c
		ripngd/ripngd.c
	YANG
		yang/frr-ripngd.yang
)

# ospfd, ospf6d

frr_daemon(ospfd
	SOURCES
		ospfd/ospf_abr.c
		ospfd/ospf_api.c
		ospfd/ospf_apiserver.c
		ospfd/ospf_asbr.c
		ospfd/ospf_ase.c
		ospfd/ospf_bfd.c
		ospfd/ospf_dump.c
		ospfd/ospf_dump_api.c
		ospfd/ospf_errors.c
		ospfd/ospf_ext.c
		ospfd/ospf_flood.c
		ospfd/ospf_gr.c
		ospfd/ospf_ia.c
		ospfd/ospf_interface.c
		ospfd/ospf_ism.c
		ospfd/ospf_ldp_sync.c
		ospfd/ospf_lsa.c
		ospfd/ospf_lsdb.c
		ospfd/ospf_memory.c
		ospfd/ospf_neighbor.c
		ospfd/ospf_network.c
		ospfd/ospf_nsm.c
		ospfd/ospf_opaque.c
		ospfd/ospf_packet.c
		ospfd/ospf_quicknbr.c
		ospfd/ospf_ri.c
		ospfd/ospf_route.c
		ospfd/ospf_routemap.c
		ospfd/ospf_routemap_nb.c
		ospfd/ospf_routemap_nb_config.c
		ospfd/ospf_spf.c
		ospfd/ospf_ti_lfa.c
		ospfd/ospf_sr.c
		ospfd/ospf_te.c
		ospfd/ospf_vty.c
		ospfd/ospf_zebra.c
		ospfd/ospfd.c
		ospfd/ospf_gr_helper.c
		ospfd/ospf_auth.c
		ospfd/ospf_main.c
	CLIPPY
		ospfd/ospf_vty.c
		ospfd/ospf_ldp_sync.c
		ospfd/ospf_dump.c
		ospfd/ospf_gr.c
	YANG
		yang/frr-ospf-route-map.yang
)

frr_daemon(ospf6d
	SOURCES
		ospf6d/ospf6_nssa.c
		ospf6d/ospf6_abr.c
		ospf6d/ospf6_area.c
		ospf6d/ospf6_asbr.c
		ospf6d/ospf6_routemap_nb.c
		ospf6d/ospf6_routemap_nb_config.c
		ospf6d/ospf6_bfd.c
		ospf6d/ospf6_flood.c
		ospf6d/ospf6_gr.c
		ospf6d/ospf6_gr_helper.c
		ospf6d/ospf6_interface.c
		ospf6d/ospf6_intra.c
		ospf6d/ospf6_lsa.c
		ospf6d/ospf6_lsdb.c
		ospf6d/ospf6_message.c
		ospf6d/ospf6_neighbor.c
		ospf6d/ospf6_network.c
		ospf6d/ospf6_proto.c
		ospf6d/ospf6_route.c
		ospf6d/ospf6_spf.c
		ospf6d/ospf6_top.c
		ospf6d/ospf6_zebra.c
		ospf6d/ospf6d.c
		ospf6d/ospf6_auth_trailer.c
		ospf6d/ospf6_main.c
	CLIPPY
		ospf6d/ospf6d.c
		ospf6d/ospf6_top.c
		ospf6d/ospf6_area.c
		ospf6d/ospf6_asbr.c
		ospf6d/ospf6_interface.c
		ospf6d/ospf6_lsa.c
		ospf6d/ospf6_gr_helper.c
		ospf6d/ospf6_gr.c
		ospf6d/ospf6_nssa.c
		ospf6d/ospf6_route.c
		ospf6d/ospf6_neighbor.c
	YANG
		yang/frr-ospf-route-map.yang
		yang/frr-ospf6-route-map.yang
)

if(FRR_NPCAP_SDK)
set(ISISD_COMMON_SOURCES
	isisd/isis_affinitymap.c
	isisd/isis_adjacency.c
	isisd/isis_bfd.c
	isisd/isis_circuit.c
	isisd/isis_csm.c
	isisd/isis_dr.c
	isisd/isis_dynhn.c
	isisd/isis_errors.c
	isisd/isis_events.c
	isisd/isis_flags.c
	isisd/isis_ldp_sync.c
	isisd/isis_lfa.c
	isisd/isis_lsp.c
	isisd/isis_misc.c
	isisd/isis_mt.c
	isisd/isis_pdu.c
	isisd/isis_pdu_counter.c
	isisd/isis_redist.c
	isisd/isis_route.c
	isisd/isis_routemap.c
	isisd/isis_spf.c
	isisd/isis_sr.c
	isisd/isis_flex_algo.c
	isisd/isis_srv6.c
	isisd/isis_te.c
	isisd/isis_tlvs.c
	isisd/isis_tx_queue.c
	isisd/isis_zebra.c
	isisd/isisd.c
	isisd/iso_checksum.c
	isisd/fabricd.c
	isisd/isis_main.c
	isisd/isis_win32.c
)

frr_daemon(isisd
	SOURCES
		${ISISD_COMMON_SOURCES}
		isisd/isis_nb.c
		isisd/isis_nb_config.c
		isisd/isis_nb_notifications.c
		isisd/isis_nb_state.c
		isisd/isis_cli.c
	CLIPPY
		isisd/isis_cli.c
	YANG
		yang/frr-isisd.yang
	LIBS "${NPCAP_LIBRARY}" delayimp iphlpapi)

frr_daemon(fabricd
	SOURCES
		${ISISD_COMMON_SOURCES}
		isisd/isis_vty_fabricd.c
	LIBS "${NPCAP_LIBRARY}" delayimp iphlpapi)
target_compile_definitions(fabricd PRIVATE FABRICD=1)

foreach(_daemon isisd fabricd)
	target_include_directories(${_daemon} PRIVATE "${NPCAP_INCLUDE_DIR}")
	target_link_options(${_daemon} PRIVATE /DELAYLOAD:wpcap.dll)
endforeach()
endif()

frr_daemon(staticd
	SOURCES
		staticd/static_bfd.c
		staticd/static_trace.c
		staticd/static_debug.c
		staticd/static_nht.c
		staticd/static_routes.c
		staticd/static_zebra.c
		staticd/static_vrf.c
		staticd/static_vty.c
		staticd/static_nb.c
		staticd/static_nb_config.c
		staticd/static_srv6.c
		staticd/static_main.c
	CLIPPY
		staticd/static_vty.c
	YANG
		yang/frr-bfdd.yang
		yang/frr-staticd.yang
)

frr_daemon(eigrpd
	SOURCES
		eigrpd/eigrp_cli.c
		eigrpd/eigrp_dump.c
		eigrpd/eigrp_errors.c
		eigrpd/eigrp_filter.c
		eigrpd/eigrp_fsm.c
		eigrpd/eigrp_hello.c
		eigrpd/eigrp_interface.c
		eigrpd/eigrp_main.c
		eigrpd/eigrp_metric.c
		eigrpd/eigrp_neighbor.c
		eigrpd/eigrp_network.c
		eigrpd/eigrp_northbound.c
		eigrpd/eigrp_packet.c
		eigrpd/eigrp_query.c
		eigrpd/eigrp_reply.c
		eigrpd/eigrp_siaquery.c
		eigrpd/eigrp_siareply.c
		eigrpd/eigrp_snmp.c
		eigrpd/eigrp_topology.c
		eigrpd/eigrp_update.c
		eigrpd/eigrp_vrf.c
		eigrpd/eigrp_vty.c
		eigrpd/eigrp_zebra.c
		eigrpd/eigrpd.c
	CLIPPY
		eigrpd/eigrp_cli.c
		eigrpd/eigrp_vty.c
	YANG
		yang/frr-eigrpd.yang
)

frr_daemon(babeld
	SOURCES
		babeld/babel_errors.c
		babeld/babel_filter.c
		babeld/babel_interface.c
		babeld/babel_main.c
		babeld/babel_zebra.c
		babeld/babeld.c
		babeld/kernel.c
		babeld/message.c
		babeld/neighbour.c
		babeld/net.c
		babeld/resend.c
		babeld/route.c
		babeld/source.c
		babeld/util.c
		babeld/xroute.c
	CLIPPY
		babeld/babel_interface.c
		babeld/babeld.c
)

frr_daemon(bfdd
	SOURCES
		bfdd/bfd.c
		bfdd/bfdd_nb.c
		bfdd/bfdd_nb_config.c
		bfdd/bfdd_nb_state.c
		bfdd/bfdd_vty.c
		bfdd/bfdd_cli.c
		bfdd/bfd_packet.c
		bfdd/dplane.c
		bfdd/event.c
		bfdd/ptm_adapter.c
		bfdd/bfd_trace.c
		bfdd/bfdd.c
	CLIPPY
		bfdd/bfdd_cli.c
		bfdd/bfdd_vty.c
	YANG
		yang/frr-bfdd.yang
)

frr_daemon(sharpd
	SOURCES
		sharpd/sharp_nht.c
		sharpd/sharp_zebra.c
		sharpd/sharp_vty.c
		sharpd/sharp_logpump.c
		sharpd/sharp_main.c
	CLIPPY
		sharpd/sharp_vty.c
)

frr_daemon(watchfrr
	SOURCES
		watchfrr/watchfrr.c
		watchfrr/watchfrr_errors.c
		watchfrr/watchfrr_vty.c
	CLIPPY
		watchfrr/watchfrr_vty.c
)

set(FRR_VTYSH_DAEMONS ${FRR_DAEMONS})
set(FRR_XREFS)
foreach(_daemon ${FRR_VTYSH_DAEMONS})
	set(_xref "${FRR_BLD}/${_daemon}.xref")
	add_custom_command(
		OUTPUT "${_xref}"
		COMMAND "${CMAKE_COMMAND}" -E env "FRR_XREF_DUMP=${_xref}"
			$<TARGET_FILE:${_daemon}>
		DEPENDS ${_daemon}
		COMMENT "XREF ${_daemon}")
	list(APPEND FRR_XREFS "${_xref}")
endforeach()
add_custom_command(
	OUTPUT "${FRR_BLD}/vtysh/vtysh_cmd.c"
	COMMAND "${CMAKE_COMMAND}" -E env "PYTHONHOME=${FRR_PYTHONHOME}"
		$<TARGET_FILE:clippy> "${FRR_SRC}/python/xref2vtysh.py"
		-o "${FRR_BLD}/vtysh/vtysh_cmd.c" ${FRR_XREFS}
	DEPENDS clippy ${FRR_XREFS} "${FRR_SRC}/python/xref2vtysh.py"
	COMMENT "VTYSH commands")

# ("\e" in the colors of the live log, which Windows does not have)
set_source_files_properties(vtysh/vtysh.c PROPERTIES COMPILE_OPTIONS /wd4129)
frr_daemon(vtysh
	SOURCES
		vtysh/vtysh_main.c
		vtysh/vtysh.c
		vtysh/vtysh_user.c
		vtysh/vtysh_config.c
		vtysh/vtysh_win32.c
		"${FRR_BLD}/vtysh/vtysh_cmd.c"
	CLIPPY
		vtysh/vtysh.c
	LIBS
		unofficial::readline-win32::readline
)

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

# tests/lib: the unit tests of libfrr that build with MSVC, run by ctest.
# As with "make check", a test passes when it exits with 0 and, if it has a
# .refout file, prints exactly that.

option(FRR_TESTS "Build the unit tests of libfrr" ON)
if(FRR_TESTS)
	enable_testing()
	set(FRR_TEST_DIR "${FRR_BLD}/tests/lib")

	function(frr_test name)
		cmake_parse_arguments(_t "NOLIB" "" "SOURCES;CLIPPY" ${ARGN})
		frr_clippy(_clippy ${_t_CLIPPY})
		if(_t_NOLIB)
			set(_lib)
		else()
			set(_lib $<TARGET_OBJECTS:frr>)
		endif()
		add_executable(${name} ${_t_SOURCES} "${FRR_BLD}/eventlog.rc"
			${_lib} $<TARGET_OBJECTS:frr_win32>)
		add_custom_target(${name}_clippy DEPENDS ${_clippy})
		add_dependencies(${name} frr_genheaders ${name}_clippy frr_lib_clippy)
		if(_t_NOLIB)
			set(_assert)
		else()
			set(_assert "${FRR_SRC}/lib/win32/assert")
		endif()
		frr_target_setup(${name} ${_assert} "${FRR_SRC}/tests/lib"
			"${FRR_SRC}/tests/helpers/c" "${FRR_SRC}/tests")
		target_link_libraries(${name} PRIVATE
			json-c::json-c PCRE2::POSIX "${LIBYANG_LIBRARY}")
		set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FRR_TEST_DIR}")
		if(FRR_RUNTIME_DLLS)
			add_custom_command(TARGET ${name} POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${FRR_RUNTIME_DLLS}
					"${FRR_TEST_DIR}")
		endif()
	endfunction()

	set(_prng tests/helpers/c/prng.c)
	frr_test(test_assert SOURCES tests/lib/test_assert.c)
	frr_test(test_atomlist SOURCES tests/lib/test_atomlist.c)
	frr_test(test_bfd_timer SOURCES tests/lib/test_bfd_timer.c)
	frr_test(test_darr SOURCES tests/lib/test_darr.c)
	frr_test(test_graph SOURCES tests/lib/test_graph.c)
	frr_test(test_idalloc SOURCES tests/lib/test_idalloc.c)
	frr_test(test_link_state SOURCES tests/lib/test_link_state.c)
	frr_test(test_mgmt_msg SOURCES tests/lib/test_mgmt_msg.c)
	frr_test(test_network_address SOURCES tests/lib/test_network_address.c)
	frr_test(test_nexthop SOURCES tests/lib/test_nexthop.c)
	frr_test(test_nexthop_iter SOURCES tests/lib/test_nexthop_iter.c ${_prng})
	frr_test(test_ntop SOURCES tests/lib/test_ntop.c ${_prng} NOLIB)
	frr_test(test_prefix2str SOURCES tests/lib/test_prefix2str.c ${_prng})
	frr_test(test_printfrr SOURCES tests/lib/test_printfrr.c)
	frr_test(test_ptm_lib SOURCES tests/lib/test_ptm_lib.c)
	frr_test(test_rcu SOURCES tests/lib/test_rcu.c)
	frr_test(test_ringbuf SOURCES tests/lib/test_ringbuf.c)
	frr_test(test_srcdest_table SOURCES tests/lib/test_srcdest_table.c ${_prng})
	frr_test(test_srv6_sid_notify SOURCES tests/lib/test_srv6_sid_notify.c)
	frr_test(test_stream SOURCES tests/lib/test_stream.c)
	frr_test(test_stream_expand SOURCES tests/lib/test_stream_expand.c)
	frr_test(test_table SOURCES tests/lib/test_table.c)
	frr_test(test_timer_correctness SOURCES tests/lib/test_timer_correctness.c ${_prng})
	frr_test(test_ttable SOURCES tests/lib/test_ttable.c)
	frr_test(test_versioncmp SOURCES tests/lib/test_versioncmp.c)
	frr_test(test_xref SOURCES tests/lib/test_xref.c)
	foreach(_t assert atomlist bfd_timer darr graph idalloc link_state mgmt_msg
		network_address nexthop nexthop_iter ntop prefix2str printfrr ptm_lib
		rcu ringbuf srcdest_table srv6_sid_notify stream stream_expand table
		timer_correctness ttable versioncmp xref)
		add_test(NAME test_${_t}
			COMMAND "${Python3_EXECUTABLE}"
				"${FRR_SRC}/tests/helpers/python/runtest_win32.py"
				"${FRR_SRC}/tests/lib/test_${_t}.py"
			WORKING_DIRECTORY "${FRR_BLD}/tests")
	endforeach()

	frr_test(test_segv SOURCES tests/lib/test_segv.c)
	add_test(NAME test_segv COMMAND test_segv WORKING_DIRECTORY "${FRR_TEST_DIR}")
	set_tests_properties(test_segv PROPERTIES PASS_REGULAR_EXPRESSION
		"Received signal 11 .*Backtrace for [0-9]+ stack frames.*func1.*in thread threadfunc")
endif()

install(TARGETS ${FRR_VTYSH_DAEMONS} RUNTIME DESTINATION sbin)
# the program databases, for the names in backtraces and for debuggers
foreach(_daemon ${FRR_VTYSH_DAEMONS})
	install(FILES "$<TARGET_PDB_FILE:${_daemon}>" DESTINATION sbin OPTIONAL)
endforeach()

file(TO_NATIVE_PATH "${FRR_STATEDIR}/etc" FRR_SYSCONFDIR_NATIVE)
file(TO_NATIVE_PATH "${FRR_STATEDIR}/run" FRR_RUNSTATEDIR_NATIVE)
file(TO_NATIVE_PATH "${FRR_STATEDIR}/crash" FRR_CRASHDIR_NATIVE)
configure_file(tools/watchfrr.cmd.in "${FRR_BLD}/watchfrr.cmd" @ONLY NEWLINE_STYLE CRLF)
install(PROGRAMS "${FRR_BLD}/watchfrr.cmd" DESTINATION sbin)
configure_file(tools/frrservice.cmd.in "${FRR_BLD}/frrservice.cmd" @ONLY NEWLINE_STYLE CRLF)
install(PROGRAMS "${FRR_BLD}/frrservice.cmd" DESTINATION sbin)
install(TARGETS vtysh RUNTIME DESTINATION bin)
install(FILES "$<TARGET_PDB_FILE:vtysh>" DESTINATION bin OPTIONAL)

install(CODE "
	file(GLOB _frr_dlls \"${FRR_BLD}/*.dll\")
	list(FILTER _frr_dlls EXCLUDE REGEX \"/python[0-9]*\\\\.dll$\")
	file(INSTALL \${_frr_dlls} DESTINATION \"\${CMAKE_INSTALL_PREFIX}/sbin\")
	file(INSTALL \${_frr_dlls} DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin\")
")
