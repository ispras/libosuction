Name:           mkpriv-aux
Version:        1
Release:        1
Summary:        Auxiliary files for mkpriv1
License:        GPL-3.0+
Url:            http://ispras.ru
Group:          Development/Building
Source:         mkpriv-aux-%_repository.tar.gz
AutoReqProv:    off

%define AUXDIR	/var/lib/mkpriv

%description
Provides auxiliary files for different runs of mkpriv

%prep
%setup -q

%build

%install
mkdir -p %{buildroot}%{AUXDIR}

for aux in dlsym-signs.txt merged.vis{,.gcc}
do
    if [ -f %{_builddir}/%{name}-%{version}/$aux ]; then
        install -m 644 %{_builddir}/%{name}-%{version}/$aux %{buildroot}%{AUXDIR}/.
    fi
done

%files
%{AUXDIR}/*

%changelog

