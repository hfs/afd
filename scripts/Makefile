#
# Makefile to install some scripts for the AFD
#

include ../Include.mk

INSTDIR   = ../..

SCRIPTS = rafdd_cmd

install :
	cp -p $(SCRIPTS) $(INSTDIR)/sbin
	cd $(INSTDIR)/sbin && chmod 755 $(SCRIPTS)

sinstall :
	$(MAKE) install

# Other Dependencies
