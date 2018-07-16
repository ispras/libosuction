Name:           mkpriv
Version:        1
Release:        1
Summary:        Collection of wrappers for dependency analysis

License:        GPL-2.0
Url:            http://ispras.ru
Group:          Development/Building
Source0:        %{name}.tar.gz
BuildRequires:  glibc-static
BuildRequires:  glibc-devel
BuildRequires:  gcc
BuildRequires:  gcc-armv7l
BuildRequires:  binutils-armv7l
BuildRequires:  gmp-devel
BuildRequires:  binutils-devel
BuildRequires:  patchelf
ExclusiveArch:  x86_64 %{ix86}

# target
%define cross armv7l
%define armv7l 1

# arm
%define cross_cpu %{cross}
%define cross_abi %{?armv7l:eabi}
%define cross_arch %{cross_cpu}-tizen-linux-gnu%{?cross_abi}
%define cross_gcc %{cross_arch}-gcc
%define cross_libdir %{_prefix}/lib%{?aarch64:64}

# qemu
%define our_path /emul

# paths
%define plugdir /usr/lib/mkpriv
%define auxdir	/var/lib/mkpriv
%define wrapdir	/usr/bin

%define daemon_host 172.16.52.7:33351
%define daemon_hostdir /etc
%define daemon_hostfile %{daemon_hostdir}/mkprivd

%description
Provide a collection of wrappers for dependency analysis used in binary optimization project.

%package run0
Summary: A wrapper to collect jump-functions

%description run0
A wrapper to collect jump-functions to detect all dlsym-like calls
and include them into global call graph.

%package run1
Summary: A set of wrappers to collect global call graph

%description run1
A set of wrappers to collect global call graph including dlsym symbols
that can be determined statically.

%package run2
Summary: A set of wrapper to eliminate unreferenced symbols

%description run2
A set of wrapper to eliminate unreferenced symbols according to
global call graph analysis.

%prep
%setup -q

%build
cross_gcc_version=`%{cross_gcc} --version | sed -ne '1s/%{cross_gcc}[^0-9]*\(\([0-9]\.\?\)*\).*/\1/p'`

./configure --release --elfclass=32 CC=gcc CXX=g++ --plugdir=%{plugdir} --auxdir=%{auxdir} \
	    CPPFLAGS="-I%{cross_libdir}/gcc/%{cross_arch}/${cross_gcc_version}/plugin/include"
make -B
# Cross-compile binaries for target
make -B util/srcid.o CC=%{cross_gcc} CFLAGS="--param=ssp-buffer-size=4 -march=armv7-a -mtune=cortex-a8 -mlittle-endian -mfpu=neon -mfloat-abi=softfp -mthumb -Wp,-D__SOFTFP__ -Wl,-O1 -Wl,--hash-style=gnu -Wa,-mimplicit-it=thumb"
make -B util/dummy.o CC=%{cross_gcc} CFLAGS="--param=ssp-buffer-size=4 -march=armv7-a -mtune=cortex-a8 -mlittle-endian -mfpu=neon -mfloat-abi=softfp -mthumb -Wp,-D__SOFTFP__ -Wl,-O1 -Wl,--hash-style=gnu -Wa,-mimplicit-it=thumb"


%install
cross_gcc_version=`%{cross_gcc} --version | sed -ne '1s/%{cross_gcc}[^0-9]*\(\([0-9]\.\?\)*\).*/\1/p'`
mkdir -p %{buildroot}%{plugdir}/ld
mkdir -p %{buildroot}%{auxdir}
install -m 755 %{_builddir}/%{name}-%{version}/gcc-plug/dlsym/*.so %{buildroot}%{plugdir}
install -m 755 %{_builddir}/%{name}-%{version}/gcc-plug/hide/*.so %{buildroot}%{plugdir}
install -m 755 %{_builddir}/%{name}-%{version}/ld-plug/*.so %{buildroot}%{plugdir}/ld
install -m 755 %{_builddir}/%{name}-%{version}/util/srcid.o %{buildroot}%{plugdir}/ld
install -m 755 %{_builddir}/%{name}-%{version}/util/dummy.o %{buildroot}%{plugdir}/ld
install -m 644 %{_sourcedir}/ignored-for-plugopt.txt %{buildroot}%{auxdir}

mkdir -p %{buildroot}%{wrapdir}
for tool in gcc-wrapper-{0,1,2} wrapper-{1,2}; do
  install -m 755 %{_builddir}/%{name}-%{version}/util/$tool %{buildroot}%{wrapdir}
done

%ifarch %ix86
  LD="/%{_lib}/ld-linux.so.2"
%endif
%ifarch x86_64
  LD="/%{_lib}/ld-linux-x86-64.so.2"
%endif

for binary in \
  %{buildroot}%{plugdir}/*.so \
  %{buildroot}%{plugdir}/ld/*.so \
  %{buildroot}%{wrapdir}/gcc-wrapper-{0,1,2} \
  %{buildroot}%{wrapdir}/wrapper-{1,2}
do
  patchelf --set-rpath "%{our_path}/%{_libdir}" $binary
# not all binaries have an .interp section
  if patchelf --print-interpreter $binary 1>/dev/null 2>/dev/null; then
    patchelf --set-interpreter "%{our_path}$LD" $binary
  fi
done

# add etc files for wrappers to connect to daemon (required for run0 and run1)
mkdir %{buildroot}%{daemon_hostdir}
echo %{daemon_host} > %{buildroot}%{daemon_hostfile}
# update baselibs.conf
sed -i -e "s,#TARGET_ARCH#,%{cross_arch}," %{_sourcedir}/baselibs.conf
sed -i -e "s,#GCC_VERSION#,${cross_gcc_version}," %{_sourcedir}/baselibs.conf
%ifarch %ix86
  sed -i -e "s,#QA_VERSION#,armv7l," %{_sourcedir}/baselibs.conf
%endif
%ifarch x86_64
  sed -i -e "s,#QA_VERSION#,x86_64-armv7l," %{_sourcedir}/baselibs.conf
%endif
sed -i -e "s,#PLUGDIR#,%{plugdir}," %{_sourcedir}/baselibs.conf
sed -i -e "s,#WRAPDIR#,%{wrapdir}," %{_sourcedir}/baselibs.conf
sed -i -e "s,#AUXDIR#,%{auxdir}," %{_sourcedir}/baselibs.conf

## Set gold linker
%if "%_repository" == "tizen-iot-4.0-gold"
sed -i -e "s,#USE_GOLD#,true," %{_sourcedir}/baselibs.conf
%endif
%if "%_repository" != "tizen-iot-4.0-gold"
sed -i -e "s,#USE_GOLD#,false," %{_sourcedir}/baselibs.conf
%endif

# At this moment there is no difference in the packages due to comfortable
# developing process. We do not delete unused files from packages.

%files run0
%{plugdir}/*
%{wrapdir}/*
%{daemon_hostfile}

%files run1
%{plugdir}/*
%{wrapdir}/*
%{daemon_hostfile}

%files run2
%{plugdir}/*
%{wrapdir}/*
%{auxdir}/ignored-for-plugopt.txt
%{daemon_hostfile}

%changelog
