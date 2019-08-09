Name          : zabbix-module-systemd
Vendor        : cavaliercoder
Version       : 1.2.0
Release       : 2
Summary       : systemd monitoring module for Zabbix

Group         : Applications/Internet
License       : GNU GPLv2
URL           : https://github.com/cavaliercoder/libzbxsystemd

Source0       : %{name}-%{version}.tar.gz
Source1       : http://sourceforge.net/projects/zabbix/files/ZABBIX%20Latest%20Stable/4.2.5/zabbix-4.2.5.tar.gz
Buildroot     : %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires      : zabbix-agent >= 4.2.0
BuildRequires : selinux-policy-devel autoconf automake libtool dbus-devel

%define module libzbxsystemd

%description
zabbix-module-systemd is a loadable Zabbix module that enables Zabbix to query
the systemd D-Bus API for native and granular service monitoring.

%prep
%setup0 -q -n %{name}-%{version}
%setup -q -b 1
# fix up some lib64 issues
autoreconf -ifv || exit 1
sed -i.orig -e 's|_LIBDIR=/usr/lib|_LIBDIR=%{_libdir}|g' configure

%build
%configure --enable-dependency-tracking --with-zabbix=%{_builddir}/zabbix-4.2.5
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_libdir}/zabbix/modules/%{module}.so
%{_sysconfdir}/zabbix/zabbix_agentd.d/%{module}.conf
# uncomment if use selinux
#%{_datarootdir}/selinux/packages/%{name}/%{module}.pp
%{_docdir}/%{name}-%{version}/README.md
%{_docdir}/%{name}-%{version}/COPYING

%changelog
* Fri Aug 9 2019 Igor Sidorenko <neither89@gmail.com> 1.2.0-2
- Added support Zabbix 4.2.5

* Thu Apr 27 2017 Ryan Armstrong <ryan@cavaliercoder.com> 1.2.0-1
- Added type parameter to systemd.unit.discovery

* Tue Apr 25 2017 Ryan Armstrong <ryan@cavaliercoder.com> 1.1.0-1
- Added systemd.service.discovery key
- Fixed cross-thread connection handling
- Fixed SELinux permissions for init_t:service status

* Wed Apr 19 2017 Ryan Armstrong <ryan@cavaliercoder.com> 1.0.0-1
- Initial release