dnl @synopsis AC_PROG_SSH
dnl
dnl Check for the program 'ssh', let script continue if exists, pops up error message if not.
dnl
dnl Besides checking existence, this macro also set these environment variables upon completion:
dnl
dnl     SSH = which ssh
dnl
dnl @version $Id: ac_prog_ssh.m4,v 1.1 2002/04/11 14:20:17 simons Exp $
dnl @author Gleen Salmon <gleensalmon@yahoo.com>
dnl
AC_DEFUN([AC_PROG_SSH],[
AC_REQUIRE([AC_EXEEXT])dnl
AC_PATH_PROG(SSH, ssh$EXEEXT, nocommand)
if test "$SSH" = nocommand; then
        AC_MSG_ERROR([ssh not found in $PATH])
fi;dnl
])
