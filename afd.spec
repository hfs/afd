Summary: A file distribution system
Name: afd
Version: 1.2.12
Release: 2
License: GPL
Group: Applications/Communications
BuildRoot: %{_builddir}/%{name}-root
Prefix: /opt
Source0: src-%{version}.tar.bz2
URL: http://www.dwd.de/general/GBTI/afd/

%description
The Automatic File Distributor provides a framework for very flexible,
non-stop, log and debug-able delivery of an arbitrary amount of files to
multiple recipients as expressed in URLs (currently mailing and ftp
supported with the mailto://user@domain and ftp://user:password@host
URL conventions).

%prep
%setup -n src-%{version}

%build
rm -f Include.mk
ln -s Include.mk.$RPM_OS Include.mk
make

%install
if [ ! "$RPM_BUILD_ROOT" = "/" ]
then
   rm -rf $RPM_BUILD_ROOT
   mkdir $RPM_BUILD_ROOT
fi
mkdir $RPM_BUILD_ROOT/%{prefix} $RPM_BUILD_ROOT/%{prefix}/%{name} $RPM_BUILD_ROOT/%{prefix}/%{name}/bin $RPM_BUILD_ROOT/%{prefix}/%{name}/sbin
make INSTDIR=$RPM_BUILD_ROOT/%{prefix}/%{name} sinstall

%clean
if [ ! "$RPM_BUILD_ROOT" = "/" ]
then
   rm -rf $RPM_BUILD_ROOT
fi

%files
%dir /opt/%{name}/bin
/opt/%{name}/bin/*
%dir /opt/%{name}/sbin
/opt/%{name}/sbin/*

%changelog
*Sat Mar 2 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
- Use build root so the build version is not installed on the build system.

*Sun Feb 10 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
- Default install in /opt

*Fri Feb 1 2002 Doug Henry <doug_henry@xontech.com>
- Initial release
