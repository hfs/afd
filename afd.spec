Summary: A file distribution system
Name: afd
Version: 1.3.0
Release: 1
License: GPL
Group: Applications/Communications
BuildRoot: %{_builddir}/%{name}-root
Prefix: /usr
Source0: %{name}-%{version}.tar.bz2
Source1: ftp://ftp.dwd.de/pub/afd/%{name}-%{version}.tar.bz2
Requires: openssl, openmotif
BuildRequires: openssl-devel, openmotif-devel
URL: http://www.dwd.de/AFD/

%description
The Automatic File Distributor provides a framework for very flexible,
non-stop, log and debug-able delivery of an arbitrary amount of files to
multiple recipients as expressed in URLs (currently mailing and ftp
supported with the mailto://user@domain and ftp://user:password@host
URL conventions).

%prep
%setup -n %{name}-%{version}

%build
./configure '--prefix=/usr' '--enable-ssl' '--enable-afd_mon'
make

%install
if [ ! "$RPM_BUILD_ROOT" = "/" ]
then
   rm -rf $RPM_BUILD_ROOT
   mkdir -p $RPM_BUILD_ROOT
   mkdir -p $RPM_BUILD_ROOT/%{prefix} $RPM_BUILD_ROOT/%{prefix}/bin $RPM_BUILD_ROOT/%{prefix}/sbin $RPM_BUILD_ROOT/%{prefix}/etc
   mkdir -p $RPM_BUILD_ROOT//etc/init.d
   mkdir -p $RPM_BUILD_ROOT//etc/sysconfig
else
   if [ ! -d $RPM_BUILD_ROOT/%{prefix} ]
   then
      mkdir -p $RPM_BUILD_ROOT/%{prefix} $RPM_BUILD_ROOT/%{prefix}/bin $RPM_BUILD_ROOT/%{prefix}/sbin $RPM_BUILD_ROOT/%{prefix}/etc
   else
      if [ ! -d $RPM_BUILD_ROOT/%{prefix}/bin ]
      then
         mkdir $RPM_BUILD_ROOT/%{prefix}/bin
      fi
      if [ ! -d $RPM_BUILD_ROOT/%{prefix}/sbin ]
      then
         mkdir $RPM_BUILD_ROOT/%{prefix}/sbin
      fi
      if [ ! -d $RPM_BUILD_ROOT/%{prefix}/etc ]
      then
         mkdir $RPM_BUILD_ROOT/%{prefix}/etc
      fi
   fi
   if [ ! -d $RPM_BUILD_ROOT//etc/init.d ]
   then
      mkdir -p $RPM_BUILD_ROOT//etc/init.d
   fi
   if [ ! -d $RPM_BUILD_ROOT//etc/sysconfig ]
   then
      mkdir -p $RPM_BUILD_ROOT//etc/sysconfig
   fi
fi
make DESTDIR=$RPM_BUILD_ROOT install
install -p -m755 scripts/afd $RPM_BUILD_ROOT//etc/init.d
install -p -m644 scripts/afd.sysconfig $RPM_BUILD_ROOT//etc/sysconfig/afd
install -p -m644 etc/AFD_CONFIG.sample $RPM_BUILD_ROOT/%{prefix}/etc
install -p -m644 etc/DIR_CONFIG.sample $RPM_BUILD_ROOT/%{prefix}/etc
install -p -m644 etc/host.info.sample $RPM_BUILD_ROOT/%{prefix}/etc
install -p -m644 etc/INFO-LOOP.sample $RPM_BUILD_ROOT/%{prefix}/etc
install -p -m644 etc/rename.rule.sample $RPM_BUILD_ROOT/%{prefix}/etc
install -p -m644 etc/afd.name.sample $RPM_BUILD_ROOT/%{prefix}/etc
install -p -m644 etc/afd.users.sample $RPM_BUILD_ROOT/%{prefix}/etc


%clean
if [ ! "$RPM_BUILD_ROOT" = "/" ]
then
   rm -rf $RPM_BUILD_ROOT
fi
rm -rf $RPM_BUILD_ROOT/%{name}-%{version}


%post
if [ -x sbin/chkconfig ]
then
   sbin/chkconfig --add %{name}
fi

%postun
if [ "$1" = "1" ]
then
   /etc/init.d/afd condrestart > /dev/null 2>&1 || :
fi
exit 0

%preun
if [ "$1" = "0" ]
then
   /etc/init.d/afd stop > /dev/null 2>&1 || :
   if [ -x sbin/chkconfig ]
   then
      sbin/chkconfig --del %{name}
   fi
fi
exit 0

%files
%config(noreplace) /etc/sysconfig/afd
%defattr(-,root,root)
%doc COPYING CREDITS Changelog INSTALL KNOWN-BUGS README.configure THANKS TODO changes-1.2.x-1.3.x
%doc doc/*
%defattr(-,root,root)
%dir %{prefix}/bin
%{prefix}/bin/*
%dir %{prefix}/sbin
%{prefix}/sbin/*
%dir %{prefix}/lib
%{prefix}/lib/*
%{prefix}/etc/AFD_CONFIG.sample
%{prefix}/etc/host.info.sample
%{prefix}/etc/INFO-LOOP.sample
%{prefix}/etc/rename.rule.sample
%{prefix}/etc/afd.users.sample
%{prefix}/etc/afd.name.sample
%defattr(600,root,root)
%{prefix}/etc/DIR_CONFIG.sample
%defattr(755,root,root)
/etc/init.d/afd

%changelog
*Thu Jul 21 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
- Do not overwrite /etc/sysconfig/afd.

*Sun Jun 26 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
- Include etc directory
- Setup init/rc script.

*Sat Jun 25 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
- Check for user and group.
- Include doc directory.

*Wed Jun 22 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
- Adapt to build from make.

*Mon May 17 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
- Adapt for version 1.3.x.

*Sat Dec 14 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
- AFD requires openmotif.

*Sat Mar 2 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
- Use build root so the build version is not installed on the build system.

*Sun Feb 10 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
- Default install in /opt

*Fri Feb 1 2002 Doug Henry <doug_henry@xontech.com>
- Initial release
