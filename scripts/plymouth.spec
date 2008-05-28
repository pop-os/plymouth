Summary: Plymouth Graphical Boot Animation and Logger
Name: plymouth
Version: 0.0.1
Release: 1%{?dist}
License: GPL
Group: System Environment/Base
Source0: %{name}-%{version}.tar.bz2
URL: http://git.freedesktop.org/plymouth
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Conflicts: rhgb
BuildRequires: libpng-devel
Requires: mkinitrd

%description
Plymouth provides an attractive graphical boot animation in
place of the text messages that normally get shown.  Text
messages are instead redirected to a log file for viewing
after boot.

%package plugin-fade-in
Summary: Plymouth "Fade-In" plugin
Group: System Environment/Base
Requires: %name = %{version}-%{release}

%description plugin-fade-in
This package contains the "Fade-In" boot splash plugin for
Plymouth. It features a centered logo that fades in and out
while stars twinkle around the logo during system boot up.

%package plugin-spinfinity
Summary: Plymouth "Spinfinity" plugin
Group: System Environment/Base
Requires: %name = %{version}-%{release}

%description plugin-spinfinity
This package contains the "Spinfinity" boot splash plugin for
Plymouth. It features a centered logo and animated spinner that
spins in the shape of an infinity sign.

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

%files
%defattr(-, root, root)
%doc AUTHORS NEWS README
%dir %{_datadir}/plymouth
%{_libexecdir}/plymouth/plymouthd
%{_libexecdir}/plymouth/plymouth-update-initrd
%{_libdir}/libply.so*
%{_bindir}/plymouth
%{_bindir}/rhgb-client
%{_libdir}/plymouth/details.so
%{_libdir}/plymouth/text.so
%{_localstatedir}/run/plymouth

%files plugin-fade-in
%defattr(-, root, root)
%dir %{_datadir}/plymouth/fedora-fade-in
%{_datadir}/plymouth/fedora-fade-in/bullet.png
%{_datadir}/plymouth/fedora-fade-in/entry.png
%{_datadir}/plymouth/fedora-fade-in/lock.png
%{_datadir}/plymouth/fedora-fade-in/star.png
%{_libdir}/plymouth/fedora-fade-in.so

%files plugin-spinfinity
%defattr(-, root, root)
%dir %{_datadir}/plymouth/spinfinity
%{_datadir}/plymouth/spinfinity/box.png
%{_datadir}/plymouth/spinfinity/bullet.png
%{_datadir}/plymouth/spinfinity/entry.png
%{_datadir}/plymouth/spinfinity/lock.png
%{_datadir}/plymouth/spinfinity/throbber-[0-3][0-9].png
%{_libdir}/plymouth/spinfinity.so

%changelog
* Wed May 28 2008 Ray Strode <rstrode@redhat.com> - 0.0.1-1
- Initial import, version 0.0.1
