Windows (MSVC)
==============

FRR can be built natively for Windows with the Microsoft C compiler (MSVC,
``cl.exe``).
FRR can run as a Windows service, logs to the Windows event log.

The autotools build is not used on Windows.

A ``CMakeLists.txt`` at the top of the tree shall be used.

Prerequisites
-------------

- Visual Studio Build Tools 2022 or later with the C++ build tools workload
  (MSVC v14.4x or later, Windows 11 SDK, CMake and Ninja)
- Git for Windows (provides ``bash``, ``perl`` and the usual Unix tools).
- Python 3.8 or later for the target architecture, with its ``libs/``
  (the default python.org installer) - ``clippy`` embeds it.
- ``win_flex`` / ``win_bison`` from `winflexbison
  <https://github.com/lexxmark/winflexbison>`_ in ``PATH``.
- `vcpkg <https://github.com/microsoft/vcpkg>`_.
- Optionally Lua 5.3, for scripting (see below).
- Optionally the Npcap SDK, unpacked anywhere, to build ``isisd`` and
  ``fabricd``.

Dependencies
------------

Install the libraries with vcpkg (use ``x64-windows`` on x64)::

   vcpkg install --triplet=arm64-windows json-c pcre2 pthreads dirent dlfcn-win32 getopt readline-win32

and ``c-ares`` for the BMP module of ``bgpd``, which is left out without
it.

The SNMP modules need net-snmp, which vcpkg does not have: build it from
its sources, in its ``win32`` directory, as static libraries with their
headers::

   git clone --branch v5.9.4 https://github.com/net-snmp/net-snmp.git
   cd net-snmp\win32
   perl Configure --config=release --linktype=static --with-sdk --with-ipv6 --prefix=C:/src/deps/netsnmp
   nmake
   nmake install install_devel

``libyang`` (version 3 is recommended) is not packaged by vcpkg; build it
from source with the same toolchain::

   git clone --branch v3.13.6 https://github.com/CESNET/libyang.git
   cd libyang && mkdir build && cd build
   cmake -G Ninja .. -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
      -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake ^
      -DVCPKG_TARGET_TRIPLET=arm64-windows ^
      -DCMAKE_INSTALL_PREFIX=C:/src/deps -DENABLE_TESTS=OFF
   ninja install

For x64, the same with ``x64-windows`` and the x64 compiler (the
``Hostarm64/x64`` tools on an ARM64 machine), into another prefix.

Lua 5.3 is the only version FRR supports, and the ``lua`` port of vcpkg is
at a later version. Its version database still has 5.3.5: a manifest with
an override installs it, in a directory of its own (``builtin-baseline`` is
the commit of the vcpkg clone, ``git rev-parse HEAD``)::

   {
     "name": "frr-lua53",
     "version": "1.0",
     "dependencies": [ "lua" ],
     "overrides": [ { "name": "lua", "version-string": "5.3.5", "port-version": 6 } ],
     "builtin-baseline": "<vcpkg commit>"
   }

   vcpkg install --triplet=arm64-windows --x-install-root=C:/src/lua53/arm64

A manifest installation keeps one triplet per installation root: give each
architecture its own.

Building
--------

From a "Developer Command Prompt" (or any shell with the MSVC environment)::

   cd frr
   mkdir build && cd build
   cmake -G Ninja .. -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
      -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake ^
      -DVCPKG_TARGET_TRIPLET=arm64-windows ^
      -DFRR_DEPS_PREFIX=C:/src/deps
   ninja

or only some of the targets, ``ninja zebra mgmtd bgpd vtysh`` for instance.

``isisd`` and ``fabricd`` are added when ``cmake`` is told where the Npcap
SDK is, with ``-DFRR_NPCAP_SDK=C:/src/npcap-sdk``.

Scripting (``HAVE_SCRIPTING``: the ``match script`` of ``bgpd`` route-maps,
``zebra on-rib-process script``) comes with
``-DFRR_LUA=C:/src/lua53/arm64/arm64-windows``, the prefix of a Lua 5.3
installation. The scripts are in ``C:/ProgramData/frr/etc/scripts``.

The SNMP modules are built with ``-DFRR_NETSNMP=C:/src/deps/netsnmp``, the
prefix of the net-snmp installation.

The unit tests of ``tests/lib`` are built too (``-DFRR_TESTS=OFF`` leaves
them out) and run with ``ctest``, from the build directory. The Python
wrapper of each test checks its output, as ``make check`` does, through
``tests/helpers/python/runtest_win32.py`` in place of pytest.
``test_typelist`` and ``test_zlog`` are not built: the first one needs
``atomhash``, which is not built on Windows, and the second one forks().

A build for another architecture than the one of the build machine, x64 on
ARM64 for instance, runs the ``clippy`` of a native build: the one it would
build embeds a Python of its own architecture. From an environment for the
x64 compiler::

   cmake -G Ninja .. -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
      -DCMAKE_TOOLCHAIN_FILE=C:/src/vcpkg/scripts/buildsystems/vcpkg.cmake ^
      -DVCPKG_TARGET_TRIPLET=x64-windows -DFRR_DEPS_PREFIX=C:/src/deps-x64 ^
      -DFRR_HOST_CLIPPY=C:/src/frr/build/clippy.exe

The build still runs the daemons it builds, to write their commands for
``vtysh``: ARM64 Windows runs x64 programs.

``ospfclient``, the example program of the client library of the OSPF API
server (``ospfd -a``), is built and installed with ``vtysh``.

The DLLs the daemons need (json-c, pcre2, libyang and its dependencies)
are copied next to them. ``FRR_STATEDIR`` (default ``C:/ProgramData/frr``)
holds the run (``run/``), state (``lib/``) and configuration (``etc/``)
directories. The first two are created on first start.

Running the daemons
-------------------

The daemons are started like on the other platforms, in this order:
``mgmtd`` first, because ``zebra`` sends it its operational state and keeps
logging errors when it is not there, then ``zebra``, then the routing
protocols once ``zebra`` is up. A BGP session that comes up before
``bgpd`` has the interfaces from ``zebra`` does not take IPv6 routes with a
link-local next hop, for instance.

Configuration ``*.conf`` files are loaded from
``C:/ProgramData/frr/etc``
Some daemons read their own configuration file, from the same directory or
from the one ``-f`` gives::

   mgmtd.exe -A 127.0.0.1
   zebra.exe -A 127.0.0.1 -f C:/frr/zebra.conf
   staticd.exe -A 127.0.0.1
   ripd.exe -A 127.0.0.1
   ripngd.exe -A 127.0.0.1
   bgpd.exe -A 127.0.0.1 -f C:/frr/bgpd.conf
   ospfd.exe -A 127.0.0.1 -f C:/frr/ospfd.conf
   ospf6d.exe -A 127.0.0.1 -f C:/frr/ospf6d.conf
   eigrpd.exe -A 127.0.0.1 -f C:/frr/eigrpd.conf
   babeld.exe -A 127.0.0.1 -f C:/frr/babeld.conf
   bfdd.exe -A 127.0.0.1 -f C:/frr/bfdd.conf
   isisd.exe -A 127.0.0.1 -f C:/frr/isisd.conf
   fabricd.exe -A 127.0.0.1 -f C:/frr/fabricd.conf

``ospfd``, ``ospf6d`` and ``eigrpd`` send and receive on raw sockets, which
Windows only gives to an Administrator: started without that right, they
stay up, log ``socket: Permission denied`` and exchange nothing.

``isisd`` and ``fabricd`` need Npcap on the machine. Without it they
start, log ``Npcap is not installed`` and leave their circuits down.

``vtysh`` talks to all of them, from a console or with ``-c``.  It takes
the same ``-N`` as the daemons, and needs an elevated prompt when they run
as Administrator::

   vtysh.exe
   router# show ip rip
   router# configure terminal
   router(config)# router rip
   router(config-router)# network ethernet_32768
   router(config-router)# redistribute connected
   router(config-router)# end
   router# write memory

   vtysh.exe -c "show ip route" -c "show bgp summary"

The vty of each daemon is also reachable over telnet on the ``-A`` address,
on the usual ports, once its configuration has ``line vty`` with
``no login`` or a password.

Warning: Windows Firewall filters what the daemons receive. When a program
first listens on a port, Windows can add rules that block it, BGP needs inbound
TCP port 179 unless this side opens the session, RIP inbound UDP port 520,
RIPng inbound UDP port 521, OSPFv2 and OSPFv3 the inbound IP protocol 89
(IPv4 and IPv6), EIGRP the inbound IP protocol 88, Babel inbound UDP port
6696 and BFD inbound UDP ports 3784 and 4784.

watchfrr
--------

``watchfrr`` watches the daemons named on its command line through their
vtysh sockets, like on the other platforms, and starts, stops and restarts
them with a command that ``cmd.exe`` runs. By default that command is
``watchfrr.cmd``.

   cmake --install .
   "C:\Program Files (x86)\frr\sbin\watchfrr.exe" mgmtd zebra staticd bgpd

``watchfrr.cmd`` reads the file ``daemons`` of the configuration directory
(``C:/ProgramData/frr/etc``), which has the same format than on Linux.
It says which daemons ``all`` stands for, on top of ``mgmtd``, ``zebra``
and ``staticd``, and gives each daemon its options, a daemon without
options gets ``-A 127.0.0.1``::

   bgpd=yes
   ospfd=yes
   zebra_options="-A 127.0.0.1"
   bgpd_options="-A 127.0.0.1 -f C:/frr/bgpd.conf"
   ospfd_options="-A 127.0.0.1 -f C:/frr/ospfd.conf"

Example,

   watchfrr.exe -s "C:\src\frr\build\watchfrr.cmd start %s" ^
      -k "C:\src\frr\build\watchfrr.cmd stop %s" ^
      -r "C:\src\frr\build\watchfrr.cmd restart %s" mgmtd zebra staticd

Running as a Windows service
----------------------------

``frrservice.cmd``, installed next to the daemons, makes FRR a service:
``watchfrr``, run by the service control manager, which starts the daemons
and starts them again when they fail::

   frrservice.cmd install
   frrservice.cmd start
   frrservice.cmd status
   frrservice.cmd stop
   frrservice.cmd uninstall

The service is ``frr`` ("FRRouting"), or ``frr-<pathspace>``.
It starts with the system, as LocalSystem, which the daemons need for their
raw sockets and routes, and the service control manager starts it again 5
seconds, 10 seconds then a minute after a failure.

``services.msc``, ``sc`` and ``net stop frr`` work as well.  The installation
and the service need an Administrator.

Logging and crash reports
-------------------------

Each program carries the message table of these events
(``lib/win32/eventlog.mc``) and registers its source the first time it
runs as an Administrator, ``frrservice.cmd install`` registers them too.

For a service, ``frr_global_options="--log syslog"`` in ``daemons`` sends
the logs of the daemons there::

   PS> Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='FRR zebra'} -MaxEvents 3

A program that crashes, its log file and to its crash log are at:
``C:/ProgramData/frr/tmp/<program>.<pid>/crashlog``
When ``C:/ProgramData/frr/crash`` exists, it also writes a minidump there,
``<program>.<pid>.dmp``, which WinDbg or Visual Studio open
with the ``.pdb`` files of the build; ``frrservice.cmd install`` creates
that directory.

``terminal monitor`` in ``vtysh`` shows the log of the daemons live, as on
the other platforms.

Modules
-------

The modules of a daemon are linked into it on Windows, and ``-M`` loads
them as on the other platforms, by their name: ``bgpd -M bmp``.  ``show
modules`` lists the ones loaded. A module that ``-M`` does not name stays
unused.

BMP (RFC 7854) is built when c-ares is there, it resolves the names of the
collectors with it::

   router bgp 65001
    bmp targets collectors
     bmp connect bmp.example.net port 1790 min-retry 1000 max-retry 2000
     bmp monitor ipv4 unicast pre-policy
     bmp monitor ipv4 unicast post-policy
    exit

``snmp`` is available. Daemons are AgentX subagents, of the ``snmpd``
of net-snmp for instance, which ``agentx`` in their configuration starts,
as on the other platforms.
The default AgentX socket of net-snmp is an AF_UNIX one, which its Windows
build does not have: the master agent and the daemons are given a TCP
socket. In the ``snmpd.conf`` of the master agent::

   master agentx
   agentXSocket tcp:127.0.0.1:705

and in ``frr.conf`` of the configuration directory of net-snmp
(``SNMPCONFPATH``, ``<prefix>/etc/snmp`` by default), which the daemons
read::

   agentXSocket tcp:127.0.0.1:705

zebra
-----

``zebra`` does not needs any special rights to read the system's state, and
has to run as Administrator (from an elevated prompt, or as a service) to install
routes and to set the interfaces.
Without that right, the routes of the routing protocols stay out of
the Windows routing table, ``zebra`` has them as rejected (``B>r``) and
says so once in its log. Its configuration file only has to open the vty,
``C:/frr/zebra.conf``::

   hostname zebra
   line vty
    no login

The vty of ``zebra`` is on port 2601 of the ``-A`` address::

   zebra> show interface brief
   Interface       Status  VRF             Addresses
   ---------       ------  ---             ---------
   ethernet_32768  up      default         192.0.2.10/24
   loopback_0      up      default

   zebra> show ip route
   K>* 0.0.0.0/0 [0/15] via 192.0.2.1, ethernet_32768, weight 1, 00:00:05
   C>* 192.0.2.0/24 [0/15] is directly connected, ethernet_32768, weight 1, 00:00:05
   L>* 192.0.2.10/32 is directly connected, ethernet_32768, weight 1, 00:00:05

   PS> Get-NetRoute | Where-Object Protocol -in Bgp,Rip

   DestinationPrefix NextHop                  RouteMetric Protocol ifIndex
   ----------------- -------                  ----------- -------- -------
   203.0.113.0/24    192.0.2.1                         20      Bgp       4
   198.51.100.0/24   192.0.2.1                         20      Rip       4
   2001:db8:113::/48 fe80::21c:42ff:fe51:53ea          20      Bgp       4
   2001:db8:100::/64 fe80::21c:42ff:fe51:53ea          20      Rip       4

Running bgpd without zebra
--------------------------

Two BGP speakers on loopback addresses, without zebra, e.g.
``C:/frr/a/bgpd.conf``::

   hostname bgpd-a
   router bgp 65001
    bgp router-id 10.0.0.1
    no bgp ebgp-requires-policy
    no bgp network import-check
    neighbor 127.0.0.2 remote-as 65002
    neighbor 127.0.0.2 update-source 127.0.0.1
    neighbor 127.0.0.2 disable-connected-check
    address-family ipv4 unicast
     network 10.1.0.0/24
     neighbor 127.0.0.2 route-map NH out
    exit-address-family
   !
   route-map NH permit 10
    set ip next-hop 192.0.2.1
   !
   line vty
    no login

and the mirror image for ``bgpd-b`` (AS 65002, ``127.0.0.2``,
``10.2.0.0/24``, next-hop ``192.0.2.2``). The route-map is only needed
because ``127.0.0.0/8`` next-hops are martians and would be rejected.
Start them with::

   bgpd.exe -Z -n -l 127.0.0.1 -A 127.0.0.1 -N a -f C:/frr/a/bgpd.conf
   bgpd.exe -Z -n -l 127.0.0.2 -A 127.0.0.2 -N b -f C:/frr/b/bgpd.conf

``-N`` gives each instance its own run directory (pid file, vtysh socket),
``-Z`` stops the attempts to connect to zebra. The vty is reachable over
telnet on port 2605 of the ``-A`` address. Ctrl+C or Ctrl+Break stops the
daemon cleanly. Binding to loopback addresses avoids a Windows Firewall
prompt, listening on a routable address needs an inbound firewall rule for
TCP port 179.

How the port works
------------------

- ``lib/win32/frr_win32.h`` is force-included (``/FI``) in every source
  file. It includes the Windows SDK and UCRT headers, removes SDK macros
  that collide with FRR identifiers, and declares the POSIX subset FRR uses;
  ``lib/win32/include/`` provides the POSIX headers Windows lacks.

- Sockets are given unique C runtime file descriptors, so the event loop and
  the rest of FRR keep using ``int`` descriptors; ``read()``, ``write()``,
  ``close()``, ``poll()`` (``WSAPoll()``), ``fcntl()`` & co. are redirected
  to wrappers in ``lib/win32/compat.c``.
  ``pipe()`` and ``socketpair()`` are loopback TCP connections, or a pair of
  connected UDP sockets on the loopback for datagrams.

- Threads use ``lib/win32/pthread.c``, a small pthread implementation on top
  of SRW locks, condition variables and fiber local storage.

- Signals are emulated: Ctrl+C / Ctrl+Break / console close deliver
  ``SIGINT`` / ``SIGTERM`` to the handlers FRR registers and interrupt the
  main thread's ``poll()``.  Windows has no signal between processes, and
  a console control event goes to every process of a console: a process
  that handles ``SIGTERM`` has a named event, ``FRR-SIGTERM-<pid>``, that
  stands for it. ``kill()`` sets it, and so does ``watchfrr.cmd``.

- The kernel interface of ``zebra`` is ``zebra/*_win32.c``, on top of the
  IP Helper API

- ``syslog()`` reports events to the event log (``ReportEventW()``); a
  crash, an exception nothing handles or ``abort()``, is the signal POSIX
  systems would raise, delivered to the handler of FRR, with
  ``backtrace()`` and ``backtrace_symbols()`` on top of
  ``RtlCaptureStackBackTrace()`` and the debug help library
  (``lib/win32/backtrace.c``).

- An FRR program runs as a service when the service control manager starts
  it: a thread started with the process connects to it, which fails at once
  otherwise. "Stop" and the shutdown of the system are ``SIGTERM``; the
  ``StopCommand`` of the service runs once the program is done, as the
  ``ExecStop`` of a systemd unit.

- AF_UNIX sockets of Windows have no ancillary data: a socket passed with
  ``SCM_RIGHTS``, the live log of ``terminal monitor``, is duplicated into
  the process at the other end (``WSADuplicateSocketW()``) and its
  description goes ahead of the data, behind a marker that UTF-8 text never
  has.

- Line editing remains GNU readline

- IS-IS runs on Ethernet frames, for which Windows has no socket:
  ``isisd/isis_win32.c`` use them from Npcap, through the pcap API.

- There is no ``fork()``.  ``watchfrr`` is the one program that needs a
  child process: where it forks and runs ``sh -c``, ``lib/win32/compat.c``
  has ``cmd.exe`` run the command, in a job object that stands for the
  process group of the child. ``waitpid()`` reaps these children, their
  end raises ``SIGCHLD``, and ``kill()`` with a negative process ID ends
  what is left in the job.

- ``crypt()`` mimics the hashes of glibc (DES, ``$1$``, ``$5$``, ``$6$``):
  passwords encrypted with ``service password-encryption`` move between
  Windows and the other platforms.

- ``long`` is 32 bits wide on Windows: code must not store pointers or
  ``time_t`` values in a ``long``.

Limitations
-----------

- Blackhole routes and IPv4 routes with IPv6 next hops cannot be
  installed. There is no MPLS, EVPN / VXLAN, policy routing or traffic
  control, and only the default VRF.

- ``zebra`` hears router solicitations through an effect of ``SIO_RCVALL``
  that Microsoft does not document
  Should a later version change it, the periodic router advertisements are
  still sent; the solicitations are no longer answered.

- The other daemons are not ported.
  ``ldpd``, ``pathd`` and ``pbrd`` would have nothing to program: Windows
  has no MPLS forwarding and no policy routing.
  ``pimd`` and ``pim6d`` need a multicast forwarding interface
  ``nhrpd`` multipoint GRE tunnels and ``vrrpd`` macvlan devices.

- The loopback interface of Windows does not send multicast packets:
  ``eigrpd`` logs a send error for each hello on it when a ``network``
  statement covers one of its addresses.

- ``watchfrr`` has no daemon mode: the service runs it.

- ``isisd`` and ``fabricd`` only run on Ethernet interfaces, not on
  point-to-point ones, and the adapter has to accept promiscuous mode,
  which not every Wi-Fi adapter does. Frames with a VLAN tag are not
  looked at.

- No daemon mode (``-d``): FRR runs as a service instead. No privilege
  handling. No ``log extended`` targets. The messages of a crash do not
  go to the event log: they go to the log file, the console and the crash
  log.

- No TCP-MD5 (``neighbor ... password``), no ``IP_MINTTL`` (GTSM), no VRF
  sockets.  ``SO_REUSEADDR`` is ignored on Windows sockets on purpose.
  ``IP_TOS`` / ``IPV6_TCLASS`` are not set: Windows only marks packets
  through a QoS policy.

- The gRPC northbound is not built. It is C++, and the Windows layer of
  FRR (``lib/win32/frr_win32.h``, the MSVC variants of ``lib/win32/msvc/``)
  is C only: the FRR headers do not compile as C++ with MSVC yet. It is
  also a loadable module, while the programs export nothing for modules to
  link with.

- Only the unit tests of ``tests/lib`` run, a topotest port needs a proper
  study.
