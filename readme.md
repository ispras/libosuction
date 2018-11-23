Libosuction is a tool for minification of shared libraries and executables in
the ELF format in _closed-world_ setups.  It works in phases (full-system
rebuilds), augmenting the build process with necessary analyses and/or
transformations.

Libosuction does not depend on any particular build system.  However, the source
tree of the project does include the tooling necessary for using libosuction on
packages cross-compiled in OBS environment.

[Design documentation](txt/t1.rst) and the [user guide](txt/guide.rst) can be
found in the [txt/](txt) subdirectory.

Also, some academic papers covering the design of libosuction have been
published:
 - [System-Wide Elimination of Unreferenced Code and Data in Dynamically Linked
   Programs](https://ieeexplore.ieee.org/document/8273289) (2017)
 - Pruning ELF: Size Optimization of Dynamic Shared Objects at Post-link
   Time (2018, pending publication)
