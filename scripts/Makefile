#
# Makefile to install some scripts for the AFD
#

include ../Include.mk

INSTDIR   = ../../sbin

SCRIPTS = rafdd_cmd

install :
	cp -p $(SCRIPTS) $(INSTDIR)
	cd $(INSTDIR) && chmod 755 $(SCRIPTS)

# Other Dependencies
