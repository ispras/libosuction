Binary Size Optimization
========================
Tool Guide
''''''''''

Introduction
------------

This document is a guide to the Binary Size Optimization Tool for developers. The
document covers descriptions of installation process on the Open Build Service
(OBS) platform and the tool usage to obtain system image optimized by binary
size.

Installation process consists of two major parts: organizing the structure of
projects and repositories in OBS and launching the supportive system on the
machines on which OBS runs.

The usage section describes the steps needed to obtain an optimized image. Each
step is accompanied with comments describing what is happening at the particular
step.

Glossary
--------
Package
   The term corresponds to the similar term in OBS. A set of resources needed to
   build an RPM.

Project
   The term corresponds to the similar term in OBS. A set of packages.

Repository
   The term corresponds to the similar term in OBS. The output of a project –
   a bunch of RPMs organized for easy use and dependency resolving.

Host Architecture
   The architecture corresponding to the environment *in* which the packages are
   built (usually ``i586`` or ``x86_64``).

Target Architecture
   The architecture corresponding to the environment *for* which the packages are
   built (can be any, ``i586``, ``x86_64``, ``armv7l``, ``aarch64``).

Base Repository
   A repository that fulfills dependencies for a particular repository
   so that the latter can be completely built.

Jump-function
   A function that passes its argument to another function without changes.

Dlsym-like function
   A function that behaves as a ``dlsym`` function: it returns the address of
   a symbol loaded into memory at run-time by its name.

Plugin
   Shared library that extends the functionality of a compiler or linker.

Wrapper
   A program that is used instead of compiler or linker that adds additional
   arguments to the call (e.g. plugins, arguments, paths) and calls the original
   compiler or linker.

Daemon
   TCP server that collects information obtained from wrappers, compiler, and
   linker during compiling and linking packages. The server also processes the
   information between runs and returns preprocessed data on demand. Daemon
   writes all the collected and processed data (its serialized state) for
   debugging purposes e.g. starting from an intermediate state.

Auxiliary Information
   The preprocessed data collected by daemon.

Run-0
   The system image build when the jump-functions are collected to produce a
   list of all dlsym-like functions.

Run-1
   The system image build that follows Run-0 build. At this stage the global
   call graph is collected (including edges to symbols loaded through
   dlsym-like functions) and the decision of eliminating of unreferenced code
   and data is made.

Run-2
   The system image build that follows Run-1 build. The stage when the
   elimination of unreferenced code and data happens.

Installation Guide
------------------
The general idea is following:

1. Create a project for the optimization tool and upload the package with
   source code there and configure them i.e. set up repository and
   architectures.

2. Select the project and repository of packages that should be optimized. For
   selected repository create three auxiliary repositories with suffixes run0,
   run1, run2 and configure them.

3. Compile the optimization tool locally.

4. Place the daemon binary on a server available from all OBS instances that
   take part in the building process.

The first two points are about configuring OBS, others – to organize
information exchange between runs. Further in the section each step will be
carefully described.

Step 1. Creating a project
~~~~~~~~~~~~~~~~~~~~~~~~~~
Let's start with creating a project in OBS and calling it ``Tizen:StripTools``.
Next, create a package ``mkpriv`` in the project. It is the main package that
installs plugins and wraps compiler and linker to collect information and
perform optimization. All the necessary files can be obtained from directory
``spec`` (``mkpriv.spec``, ``ignored-for-plugopt.txt``, ``baselibs.conf``) and
``mkpriv.tar.gz`` that contains a folder ``mkpriv-1`` with all source files.
The project should have its own repository, separate from the one being optimized. This
repository should have in additional paths the ``Base`` packages of the optimized
repository to avoid binary incompatibilities that might occur if different versions
of gcc and/or binutils are used. The package ``mkpriv`` should be built for the
host architecture of the repository being optimized. The target architecture should
also be included. That allows OBS to auto import target packages built by host
architecture. The project configuration file of the created project should be
empty.

    **Example**: We created project ``Tizen:StripTools`` and a package
    ``mkpriv`` inside it. Then created repository ``tizen-4.0`` to optimize one
    of Tizen 4 project. After that we added additional path ``Tizen:4.0:Base``
    to repository ``tizen-4.0`` and finished with adding ``x86_64`` and
    ``armv7l`` architecture because OBS environment is configured for
    ``x86_64`` and our target is ``armv7l``.

When the package ``mkpriv`` is successfully built, the step is over.

Step 2. Prepare optimizable project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Move on to the project that should be optimized. We selected
``Tizen:4.0:Headless`` and repository ``standard`` for ``armv7l``. The next
step is to create three copies of that repository. In this way, we created
repositories ``standard-run0``, ``standard-run1``, and ``standard-run2``; copied
meta configuration (paths to repositories with dependencies); also the
conditions in project configuration file should be extended for new
repositories (e.g. in proj.conf there is a line ``%if "%_repository" ==
"standard"`` it should be changed to ``%if "%_repository" == "standard" ||
"%_repository" == "standard-run0" || ...`` for all auxiliary repositories). It
allows to have three independent repositories identical to the original one.
*At this point, it would be useful to disable them to not trigger the building
process.* The next step is to add a dependency of ``Tizen:StripTools`` project
``tizen-4.0`` repository. It can be done through the *repositories* tab or meta
configuration. Finally, we have to make the repositories use our tools.
To do that, add the following to project configuration file according to
repositories' names and target architecture.

.. code-block:: none

    %if "%_repository" == "standard-run0"
    Preinstall: mkpriv-run0-armv7l
    %endif

    %if "%_repository" == "standard-run1"
    Preinstall: mkpriv-run1-armv7l
    %endif

    %if "%_repository" == "standard-run2"
    Preinstall: mkpriv-run2-armv7l
    %endif

Step 3. Build the Daemon
~~~~~~~~~~~~~~~~~~~~~~~~
The daemon is built as a part of ``mkpriv`` project for the host architecture.
This can be done with performing the commands:

.. code-block:: none

   ./configure --release
   make

There is a list of files that are needed after compilation:

**utils/daemon**
   On the Run-0 and Run-1 compiler and linker send to daemon information
   collected at particular run that will be used further. Daemon collects it in the
   directory from which it is running.

**utils/dlsym-signs-base.txt**
   The file with base dlsym-like functions.

**force/***
   Files needed to force symbols to be uneliminable during optimization.

Step 4. Setup the Daemon
~~~~~~~~~~~~~~~~~~~~~~~~
The final step to finish the installation is to organize workspace for daemon.
We recommend to keep the following structure of a directory for each daemon
instance.

.. code-block:: none

   /path/to/workspace
   |-- force
   |   `-- deps-*
   |-- utils
   |   |-- daemon
   |   `-- dlsym-signs-base.txt
   `-- aux

Copy the all contents from ``force`` directory in source files to a ``force``
directory in a newly created structure.  Also copy ``daemon`` and
``dlsym-signs-base.txt`` from ``utils`` directory in source files to ``utils``
in the structure. Daemon will use  ``aux`` directory to save auxiliary files.

Usage Guide
-----------
The natural flow of building is a consequent build of runs starting with the
zeroth and ending with the second producing information for a next run by
consuming information from a previous run. At the end, the optimized packages
will be located in the repository with suffix ``run2``.

To describe the process of building and be consistent, we will refer to the
structure described in `Installation Guide`_, however you may have you own
structure in OBS.

Run 0 – Jump Functions
~~~~~~~~~~~~~~~~~~~~~~~
We assume that `Installation Guide`_ was completely performed and the base set
of files were installed.

The first step is Run-0. Before the building process of a repository with
suffix ``run0`` is triggered to build, it is necessary to launch a daemon
instance on a server available from all OBS instances that take part in the
building process.

.. code-block:: bash

   cd /path/to/daemon/aux
   ../utils/daemon \
       [--port N] \
       --dlsym-base ../utils/dlsym-signs-base.txt \
       --force-files ../force/* > log

The command above starts the daemon which is implemented as a TCP server. It
takes an optional parameter ``--port`` that should correspond to the port value
in ``Tizen:StripTools/mkpriv/mkpriv.spec``; ``--force-files`` provides to a
daemon a set of symbols that should not be eliminated; ``--dlsym-base``
parameter points to base dlsym signatures. All commands received by daemon are
logged into file ``log``.

The build of repository with suffix ``run0`` can now be triggered. When
the Run-0 build is finished, the directory ``aux`` will have files with pattern
name ``jfunc-*``. These files contain information about jump functions
collected through the building. These files could be used for analysis as well
as to start the daemon at this stage ignoring the zeroth run.

At this moment the Run-0 is finished.

Run 1 – Dynamic Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The prerequisite for this run is to have the launched daemon from the Run-0
that collected jump functions.  This state of daemon could be reached in two
ways. The first is that you have already built Run-0 and the daemon is still
working.  The second is to start daemon and pass ``jfunc-*`` files as
arguments. *Note that this action requires some time to read all files from
disk into memory*. Log output indicates when reading and processing is
finished.

.. code-block:: bash

   cd /path/to/daemon/aux
   ../utils/daemon \
       [--port N] \
       --jfunc-files jfunc-* \
       --force-files ../force/* > log

The ``--jfunc-files`` option specifies files where jump functions could be found.

The build of repository with suffix ``run1`` can now be triggered. When
the build is finished, the files with the pattern names ``deps-*`` and
``dlsym-*`` appear in directory ``aux`` as well as a file named
``signatures.txt`` that contains information about dlsym-like functions.

``deps-*`` and ``dlsym-*`` files could be used for analysis as well as to start
the daemon at this stage ignoring the first run.

At this moment the Run-1 is finished.

Run 2 – Symbol Elimination
~~~~~~~~~~~~~~~~~~~~~~~~~~~
The prerequisite for this run is to have the launched daemon from the Run-1
that collected dlsym symbols and symbol dependencies.  This state of daemon
could be reached in two ways. The first is that you have already built Run-1
and the daemon is still working.  The second is to start daemon and pass
``deps-*`` and ``dlsym-*`` files as arguments. *Note that this action requires
some time to read all files from disk into memory*. Log output indicates when
reading and processing is finished.

.. code-block:: bash

   cd /path/to/daemon/aux
   ../utils/daemon \
       [--port N] \
       --deps-files deps-* \
       --dlsym-files dlsym-* \
       --force-files ../force/* > log

The build of repository with suffix ``run2`` can now be triggered. When
the build is finished, the repository with suffix ``run2`` will contain
optimized rpm packages.

Additional Information
----------------------
The section contains a kind of details that should be considered if changes in
processes described in `Installation Guide`_ and `Usage Guide`_ sections happen.

Run Rebuild
~~~~~~~~~~~
To rebuild a particular run, all RPM packages should be deleted from repository
for this run. That can be done with command using `OSC`__ tool:

.. __: https://en.opensuse.org/openSUSE:OSC

.. code-block:: bash

   osc wipebinaries PROJECT -r REPOSITORY -a ARCH

mkpriv Package Behavior
~~~~~~~~~~~~~~~~~~~~~~~
The builds for ARM architectures in OBS are done with using ``qemu-accel``
package.  It wraps all calls to the executable during building and redirects
them to native executable for a host architecture if it is possible. In case of
a compiler, the redirection leads to using a cross compiler.

The optimization tool uses plugins for compiler i.e. they should be compiled
for the host architecture. For this reason, the package ``mkpriv`` is built for
host architecture.

As far as OBS allows to use only target architecture packages to build packages
for the target, it is not possible to use ``mkpriv`` compiled for the host. The
`baselibs concept of OBS`__ helps. It was designed for using 32bit libraries
to build 64bit packages. The configuration file of baselibs is located at
``spec/baselibs.conf``. It allows to suppress OBS requirements checks and
transfer raw executable of one architecture to another. ``qemu-accel`` uses the
same approach. The last moment is to patch run-time search path and interpreter
of executables. We need to change them to the appropriate for the host,
fortunately they are already provided by ``qemu-accel`` for its own needs.

.. __: https://en.opensuse.org/openSUSE:Build_Service_baselibs.conf

Final view is the following: each OBS call to a compiler or a linker is
redirected to ``qemu-accel`` wrapper that tries to find the native executable;
instead of calling native executable ``qemu-accel`` wrapper calls  ``mkpriv``
wrapper; ``mkpriv`` wrapper appends necessary parameters and finally calls the
compiler or linker. Wrappers and plugins in their turn communicate with daemon
to exchange information.

Supplemental Tools Description
------------------------------
Supplemental tools could be found in directory ``util`` after building the sources.

add_deps
   A bash script that allows to easily append a new symbol to deps-files
   located in ``force`` folder. It appends the new symbol with boilerplate data and
   increments the total count of symbols in a file. The example of usage is
   shown below. *Note that it is applicable only to files located in force
   folder*.

   .. code-block:: bash

      ./add_deps /path/to/deps-smth symbol1 symbol2 symbol3

amend-merge-output.sh
   A bash script that consumes a result provided by merge utility and adjust it
   for GCC plugin that hides symbols from global scope on the Run-2.

   .. code-block:: bash

      ./amend-merge-output.sh merge.vis

daemon
   TCP server that collects information obtained from wrappers, compiler,
   linker during compiling and linking packages. The server also processes the
   information between runs and returns preprocessed data on demand. Daemon
   writes all collected and processed data into separate files for intermediate
   start and development reasons.

   Daemon integrates the functionaly of most of supplemental tools such as
   ``jf2sign``, ``merge`` and ``amend-merge-output.sh``.

   Daemon accepts the following arguments:

   --port N                 optional, the port daemon starts on and listens
   --dlsym-base file        required for Run-0, a file that contains
                            information about base dlsym signatures
   --jfunc-files files      required for Run-1, a sequence of files with jump
                            functions that were dumped at Run-0 by daemon 
   --deps-files files       required for Run-2, a sequence of files with
                            symbol dependencies that were dumped at Run-1 by 
                            daemon
   --dlsym-files files      required for Run-2, a sequence of files with
                            symbol linked through dlsym-like functions that
                            were dumped at Run-1 by daemon
   --force-files files      optional, a set of symbols that should not be
                            eliminated

gcc-wrapper-0
   The wrapper for GCC used at the Run-0. It appends arguments for real GCC to
   enable a plugin that collects and sends jump functions to the daemon.

gcc-wrapper-1
   The wrapper for GCC used at the Run-1. It appends arguments for real GCC to
   enable plugins that collect symbols used in dlsym-like calls and add a section
   with ``srcid`` for a linker.

   The plugin for collecting symbols from dlsym-calls receives information
   about signatures from the daemon and sends back the revealed symbols.

gcc-wrapper-2
   The wrapper for GCC used at the Run-2. It appends arguments for real GCC to
   enable a plugin that changes symbols' visibility.

   The plugin receives information about symbols which need to have changed
   visibility from the daemon.

jf2sign
   A utility that consumes jfunc files obtained from daemon to produce a set of
   dlsym wrappers needed by a plugin working with dlsym-like functions.

merge
   A utility that consumes all dependencies and produces the final list of
   symbols that should be eliminated, hided or localized. This list is used in
   linker plugin as well as in script ``amend-merge-output.sh``.

   The first argument of the utility should be a file that consists of
   concatenated dlsym-files obtained from daemon of the Run-1. Others are
   deps-files.

   .. code-block:: bash

      ./merge dlsym-all deps-* > merged.vis

wrapper-1
   The wrapper for a linker used at Run-1. It appends arguments for the real
   linker to enable a plugin that collects information about dependencies
   during linking. Finally, the plugin sends revealed dependencies to the
   daemon.

wrapper-2
   The wrapper for a linker used at Run-2. It generates and compiles assembly
   file that sets visibility of eliminable symbols to hidden. This file is
   added to linking command so that gc-section optimization is able to delete
   them.

   The assembly file generation is based on information provided by daemon for
   a particular ``linkid``.

RPM Package
-----------
As was mentioned in `Installation Guide`_, we should build RPM package that
will be installed in virtual environment for each package in repository.  It
consists of four files: ``mkpriv.tar.gz``, ``mkpriv.spec``, ``baselibs.conf``
and ``ignored-for-plugopt.txt``.

Sources
~~~~~~~
The sources are provided in an archive named ``mkpriv.tar.gz``. The sources
should be in a nested directory named ``mkpriv-1``.

Spec File
~~~~~~~~~
Spec file describes the actions that should be taken to compile sources for target
architecture. The settings for target architectures come from QEMU package due
to the fact that the binary optimization tool is based on intervening into the chain
of wrappers which is used for cross-compilation.

The following options are related to the tool.

plugdir
   The directory where compiler and linker plugins are installed.

auxdir
   The directory where auxiliary information is located.

wrapdir
   The directory where compiled wrappers are placed.

daemon_host
   IP address and a port where the daemon is supposed to be launched.

daemon_hostdir
   The directory where settings for connecting to daemon are stored.

daemon_hostfile
   The file with daemon connection settings themselves.

The main idea of a package is to build an individual RPM for each run. It could be
reached through baselibs mechanism that allows to provide packages built on one
architecture to another. Also, the dynamic loader of executables should be
patched because OBS cross-build system is based on QEMU.

Baselibs
~~~~~~~~
Baselibs mechanism allows to put files in RPMs for one target, nevertheless
they may be compiled for another one. In our case, we initiate ``x86_64`` build
with a cross compiler to build ``arm`` binaries. Baselibs mechanism allows us
to put ``arm`` binaries into RPMs that will be available at arm repositories in OBS.

``baselibs.conf`` contains scripts that pack three RPMs: ``mkpriv-run0``,
``mkpriv-run1``, and ``mkpriv-run2``. The scripts carefully put right wrappers
into right directories so that they are able to wrap real compiler and linker
correctly.

GCC Plugin Optimization
~~~~~~~~~~~~~~~~~~~~~~~
The process of function localization at GCC level could change the inlining
strategies and, as a result, the package will be build not as was expected. To
avoid that, the file ``ignored-for-plugopt.txt`` was introduced. This file
provides OBS specific mechanism of ignoring packages for this optimization. If a
package name is listed in this file, we locate a file ``ignored-for-plugopt``
in a directory defined by variable ``auxdir`` . GCC wrappers check the existence
of this file and if it is present, the optimization would not be applied.

To ignore the optimization for a particular package, just write a package name
in the end of file ``ignored-for-plugopt.txt`` (each package name on separate
line). It already contains four package names, where the optimization
changes behavior: ``boost``, ``iotivity``, ``nss``, ``glibc``.

Intermediate Files Formats
--------------------------
All communication between wrappers and daemon happens using text file formats.
This section is aimed to describe them.

Dlsym signatures
~~~~~~~~~~~~~~~~
This format is used to describe the signatures of a dlsym-like functions. It is
used for Run-0 as a ``--dlsym-base`` argument and, also, the processed jump
functions extend the dlsym-base file by adding wrappers in the same manner. The
signature consists of a function name and an argument position where is a
symbol name expected. In the beginning it has a number of signatures described
in a file.

.. code-block:: none

   <number>
   <fun name> <arg pos>

Example, for the code shown below the following signatures are formed.

.. code-block:: c

   void *dlsym(void *handle, const char *symbol);
   void *dlvsym(void *handle, char *symbol, char *version);
   static lua_CFunction ll_sym (lua_State *L, void *lib, const char *sym);

..
   **


.. code-block:: none

   3
   dlsym 1
   dlvsym 1
   ll_sym 2

Jump Functions
~~~~~~~~~~~~~~
This format is used in ``jfunc-*`` files generated by daemon. Jump function is
an abstraction that represents a function that passes its own argument without
changes to another function. The format is quite self-explanatory, it consists
of a function name and a position of an argument that is passed to another
function as an argument. In the beginning it has a number of jump functions
described in a file.

.. code-block:: none

   <number>
   <from fun name> <from arg pos> <to fun name> <to arg pos>

Example, for the code shown below the following signatures are formed.

.. code-block:: c

   void foo(int a, int b) {
      bar (4, a);
      baz (b);
   }

.. code-block:: none

   2
   foo 0 bar 1
   foo 1 baz 0


Symbol Dependencies
~~~~~~~~~~~~~~~~~~~
This format is used in ``deps-*`` files generated by daemon. This format
describes dependencies between sections and symbols within one unit of linkage.
It consists of two parts. The first part recursively describes output file
starting from output file type and name, continuing with description of each
object file and finishing with sections within each object file and their
dependencies. The second part describes the state of symbols appeared in this
linkage and in which sections they are used.

.. code-block:: none

   <dso type> <nobj> <dso name> <entrypoint> <linkid>
   <nsec>    <offset>        <obj name>      <srcid>
             <used>          <size>     <sec name>  <sec uid>
                             <ndeps>    <sec uid> ...
             ...
   ...
   <nsym>
   <weak><vis><tls> <sec uid>   <sym>
                                <ndeps> <sec uid> ...
   ...

dso type
   The type of output file. Could be '``R``' – object file, '``D``' – shared
   object file, '``E``' – executable and '``P``' – PIE.
nobj
   Amount of object files that participate in linkage.
dso name
   Name of output file.
entrypoint
   A function where control is transferred from the operating system.
linkid
   Unique ID of linked object file based on srcids of input files.
nsec
   Amount of sections in an object file.
offset
   Offset of an object if it is in a static library.
obj name
   Object name. In case of static library it is a library name.
srcid
   Unique ID of source file based on md5 hash of translation unit's intermediate representation.
used
   Indicates that the section is used.
size
   Size of the section.
sec name
   Section name.
sec uid
   Unique ID of a section within file.
ndeps
   Number of dependencies.
nsym
   Number of symbols in a linkage.
weak
   Indicates the symbol weakness. It could be '``D``' – strong, '``C``' – common, '``W``' – weak and '``U``' – undefined.
visibility.
   Indicates the symbol visibility. It could be '``d``' – default, '``p``' – protected and '``h``' – hidden.
tls
   Indicates the TLS symbols with value '``T``' and '``_``' otherwise.
sym
   A symbol name.

The sample is shown below.

.. code-block:: none

   E 1 3 conftest _start 6bc56b8dff655fbf45091d3876f668af
   3       0       /tmp/ccfzRuin.o 6bc56b8dff655fbf45091d3876f668ae
           0       4       .text   0
                   1 0
           0       4       .data   1
                   1 0
           0       0       .bss    2
                   0
   2
   Dd_ 0   _start
           0
   Dd_ 0   __start
           0

Dlsym Functions
~~~~~~~~~~~~~~~
This format is used in ``dlsym-*`` files generated by daemon. This format
describes the place where the symbol could be linked at run-time by using
dlsym-like functions. In the beginning it has a number of calls described in
a file

.. code-block:: none

   <number>
   <filename>:<row>:<srcid>:<sec name>:<fun name>:<type>:<symbols>

number
   Amount of calls i.e. amount of following lines.
filename
   A name of source file where a call happens.
row
   A row in source file where a call happens.
srcid
   Unique ID of source file based on md5 hash of translation unit's intermediate representation.
sec name
   A section name in which a function will be after compilation.
fun name
   A function name where a call happens.
type
   A type of call. There are four different types: ``UNDEFINED``, ``CONSTANT``,
   ``DYNAMIC`` and ``PARTIALLY_CONSTANT``. Undefined type describes the call
   when symbol is undefined. The constant type describes a situation when a
   dlsym call is always invoked with statically determined literal strings in
   all possible paths leading to the call. In contrast to the constant state, the
   dynamic state shows that no paths to the dlsym-function call can be
   determined statically. The last one, the partially constant state is a mix
   state of the constant and dynamic states.
symbols
   Comma separated symbols that could be an argument and, as a result, could be
   linked at runtime.

The sample is shown below.

.. code-block:: none

   main.c:15:deb29623a63342776613ea55ac13a0ff:.text:main:CONSTANT:foo,bar
   main.c:35:deb29623a63342776613ea55ac13a0ff:.text:main:DYNAMIC:

Merged File
~~~~~~~~~~~
This format is used to pass information about symbols to a linker plugin at
Run-2 stage.

The format consists of a header and four sections. The header describes the
size of sections and ``linkid`` to identify to which link process it belongs.
The following four sections delimited by empty lines. The first section
describes symbols that should be discarded during linking. The second section
consists of symbols that should be localized. The third section is about
symbols that should have their visibility changed to ``hidden``. Finally, last section
consists of undefined symbols for this linking process for correctness checking.

.. code-block:: none

   <nelim> <nloc> <nhid> <linkid>
   <object name>:<srcid>:<srcidcnt>:<tls>:<symbol>
   <object name>:<srcid>:<srcidcnt>:<tls>:<symbol>

   <object name>:<srcid>:<srcidcnt>:<tls>:<symbol>
   <object name>:<srcid>:<srcidcnt>:<tls>:<symbol>

   <object name>:<srcid>:<srcidcnt>:<tls>:<symbol>
   <object name>:<srcid>:<srcidcnt>:<tls>:<symbol>

   <object name>:<srcid>:<srcidcnt>:<tls>:<symbol>
   <object name>:<srcid>:<srcidcnt>:<tls>:<symbol>

nelim
   Amount of symbols that should be eliminated.
nloc
   Amount of symbols that should be localized.
nhid
   Amount of symbols that should be hidden.
linkid
   Unique ID of linked object file based on srcids of entered source files.
object name
   The name of resulting object file. The field has a value '``-``' for
   undefined symbols.
srcid
   Unique ID of source file based on md5 hash of translation unit's
   intermediate representation.  The field has a value '``f``' for undefined
   symbols.
srcidcnt
   Amount of symbol appearance with this srcid. This counter is needed to
   determine whether a particular symbol is always in the same class or stronger
   (hidden, localized, eliminated). The field has a value '``-1`` for undefined
   symbols.
tls
   Boolean field that identifies TLS symbol with value '``T``' and '``_``'
   otherwise.
symbol
   Affected symbol.

The sample is shown below.

.. code-block:: none

   4 4 3 1 4b47ced117051b969a6dbe7fef8c3e16
   libmisc.a:3d776c925f25ff6d99be5475bec4594d:6:_:Fts_children
   rpmlua.o:ce131d7e1672ec6ea906838eefd195b2:2:_:rpmluavValueIsNum
   rpmsw.o:a0cbf71beb29922d42153ac7b3f730c5:2:T:rpmswSub
   rpmpgp.o:631e5090514be12cae5230aafe020df7:2:_:pgpVerifySig

   libgcc.a:c8300753bd6ebebf61da489ac885bd6b:35:_:__floatdidf
   rpmkeyring.o:bfd87e236c2a582fd0c72801129dc6f4:2:T:rpmPubkeyLink
   crti.o:971060e4b77392ab26d56881e6745185:1164:_:_fini
   crti.o:971060e4b77392ab26d56881e6745185:1164:_:_init

   rpmlua.o:ce131d7e1672ec6ea906838eefd195b2:2:_:rpmluaPushPrintBuffer
   digest.o:a153b50dc98e4c0a57e55bbd10c25ba5:2:_:rpmDigestBundleFinal
   base64.o:5a47b166b8439dc92397aca69ab2e0f1:2:_:rpmBase64CRC

   -:f:-1:_:lua_next

GCC Merged File
~~~~~~~~~~~~~~~
This format is used to pass information about symbols to GCC plugin to
localize or eliminate them.

The format consists of a header and three sections. The header describes the
sizes of sections. Three sections are delimited by empty line. The first
section contains symbols that should be eliminated during compilation. The
second section consists of symbols that should be localized i.e. the compiler
should make them static. The last section changes the visibility attribute of
the symbol to ``hidden`` (symbols with such visibility do not participate in dynamic
linking).

.. XXX: semantic change ^

.. code-block:: none

   <nelim> <nloc> <nhid>
   <object name>:<srcid>:<symbol>
   ...

   <object name>:<srcid>:<symbol>
   ...

   <object name>:<srcid>:<symbol>
   ...

nelim
   Amount of symbols that should be eliminated.
nloc
   Amount of symbols that should be localized.
nhid
   Amount of symbols that should be hidden.
object name
   The name of resulting object file.
srcid
   Unique ID of source file based on md5 hash of translation unit's intermediate representation.
symbol
   Affected symbol.

The sample is shown below.

.. code-block:: none

   2 1 1
   main.o:deb29623a63342776613ea55ac13a0ff:foo
   main.o:deb29623a63342776613ea55ac13a0ff:bar

   main.o:deb29623a63342776613ea55ac13a0ff:baz

   main.o:deb29623a63342776613ea55ac13a0ff:foobar

.. section-numbering::
