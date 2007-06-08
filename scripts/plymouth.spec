Summary: Plymouth Graphical Boot Animation
Name: plymouth
Version: 0.0.1
Release: 1%{?dist} 
License: GPL
Group: System Environment/Base
Source0: %{name}-%{version}.tar.bz2
URL: http://people.freedesktop.org/~halfline/plymouth
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n) 

Conflicts: rhgb
BuildRequires: libpng-devel
Requires: mkinitrd

%description
Plymouth provides an attractive graphical boot animation in place 
of the text messages that normally get shown.

%prep
%setup -q 

%build
%configure 
make

%install
rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT

find $RPM_BUILD_ROOT -name '*.a' -exec rm -f {} \;
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} \;

%clean
rm -rf $RPM_BUILD_ROOT

%post
/usr/libexec/plymouth/plymouth-update-initrd > /dev/null
%preun

%files
%defattr(-, root, root)
%doc AUTHORS ChangeLog NEWS README
%{_datadir}/plymouth/star.png
%{_libexecdir}/plymouth/plymouth
%{_libexecdir}/plymouth/plymouth-update-initrd
%{_libdir}/plymouth/fedora-fade-in.so
%{_libdir}/libply.so*
%{_bindir}/rhgb-client

%changelog
* Fri Jun  8 2007 Ray Strode <rstrode@redhat.com> - 0.0.1-1
- Initial import, version 0.0.1
