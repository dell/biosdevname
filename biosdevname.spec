Name:		biosdevname
Version:	0.2.1
Release:	1%{?dist}
Summary:	udev helper for naming devices per BIOS names

Group:		System Environment/Base
License:	GPLv2
URL:		http://linux.dell.com/files/%{name}
Source0:	http://linux.dell.com/files/%{name}/%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	pciutils-devel, zlib-devel
# FIXME: how does this work for RHEL?
%if 0%{?fedora}
%if 0%{?fedora} < 6
BuildRequires:  sysfsutils-devel
%elseif 0%{?fedora} >= 6
BuildRequires:  libsysfs-devel
%endif
%elseif %{?_vendor} == 'redhat'
# only RHEL5 has this
BuildRequires:  libsysfs-devel
%endif
%if 0%{?suse_version} >= 1000
# SLES 9 doesn't have sysfsutils at all
BuildRequires:  sysfsutils
%endif


%description
biosdevname in its simplest form takes an kernel name name as an
argument, and returns the BIOS-given name it "should" be.  This is necessary
on systems where the BIOS name for a given device (e.g. the label on
the chassis is "Gb1") doesn't map directly and obviously to the kernel
name (e.g. eth0).

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
# only nuke the static lib if the shared lib was built
if [ -e $RPM_BUILD_ROOT%{_libdir}/libbiosdevname.so ]; then
rm $RPM_BUILD_ROOT%{_libdir}/libbiosdevname.a
fi
rm $RPM_BUILD_ROOT%{_libdir}/libbiosdevname.la
rm $RPM_BUILD_ROOT%{_sbindir}/%{name}S


%clean
rm -rf $RPM_BUILD_ROOT


%files

%defattr(-,root,root,-)
%doc COPYING README
%{_sbindir}/%{name}
%{_libdir}/libbiosdevname.*
%{_sysconfdir}/udev/rules.d/*biosdevname.rules

%post -p /sbin/ldconfig


%changelog
* Thu Aug 16 2007 Matt Domsch <Matt_Domsch@dell.com> 0.2-1
- initial release
