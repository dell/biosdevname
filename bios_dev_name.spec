Name:		bios_dev_name
Version:	0.2.1
Release:	1%{?dist}
Summary:	udev helper for naming devices per BIOS names

Group:		System Environment/Base
License:	GPLv2
URL:		http://linux.dell.com/files/bios_dev_name
Source0:	http://linux.dell.com/files/bios_dev_name/%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	pciutils-devel, libsysfs-devel

%description
bios_dev_name in its simplest form takes an kernel name name as an
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
rm $RPM_BUILD_ROOT%{_libdir}/libbiosdevname.la $RPM_BUILD_ROOT%{_libdir}/libbiosdevname.a
rm $RPM_BUILD_ROOT%{_sbindir}/%{name}S


%clean
rm -rf $RPM_BUILD_ROOT


%files

%defattr(-,root,root,-)
%doc COPYING README
%{_sbindir}/%{name}
%{_libdir}/libbiosdevname.*
%{_sysconfdir}/udev/rules.d/60-bios_dev_name.rules

%post -p /sbin/ldconfig


%changelog
* Thu Aug 16 2007 Matt Domsch <Matt_Domsch@dell.com> 0.2-1
- initial release
