#
# Makefile to build the AFD (Automatic File Distributor)
#

include Include.mk

INSTALLDIR = ../bin
COMMONDIR  = common

AFDLIB     = libafd.a
PROTOLIB   = protocols
MISCDIR    = misc
IAFDPROG   = init_afd
AMGPROG    = amg
FDPROG     = fd
LOGPROGS   = log
MAINTPROGS = tools
AWPROG     = archive_watch
AFDDPROG   = afdd
AFDMONPROG = afd_mon
STAPROG    = statistics
XPROG      = X
WIDGET_SET = Motif
SCRIPTS    = scripts

PROGS      = $(AFDLIB) $(PROTOLIB) $(MISCDIR) $(IAFDPROG) $(AMGPROG) $(FDPROG) $(LOGPROGS) $(MAINTPROGS) $(AWPROG) $(AFDDPROG) $(AFDMONPROG) $(STAPROG) $(XPROG)

all : $(PROGS)

everything : $(PROGS) $(AFDMONPROG)

$(AFDLIB) : dummy
	cd $(COMMONDIR) && $(MAKE) $(FORKMAKEFLAG) all

$(PROTOLIB) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(MISCDIR) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(IAFDPROG) : dummy
	cd $(@) ; $(MAKE) $(FORKMAKEFLAG) all

$(AMGPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(FDPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(LOGPROGS) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(MAINTPROGS) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(AWPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(AFDDPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(AFDMONPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(STAPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(XPROG) : dummy
	cd $(@)/$(WIDGET_SET) && $(MAKE) all

dummy :

clean :
	cd $(COMMONDIR) && $(MAKE) clean
	cd $(PROTOLIB) && $(MAKE) clean
	cd $(IAFDPROG) && $(MAKE) clean
	cd $(AMGPROG) && $(MAKE) clean
	cd $(FDPROG) && $(MAKE) clean
	cd $(LOGPROGS) && $(MAKE) clean
	cd $(MISCDIR) && $(MAKE) clean
	cd $(MAINTPROGS) ; $(MAKE) clean
	cd $(AWPROG) && $(MAKE) clean
	cd $(AFDDPROG) && $(MAKE) clean
	cd $(AFDMONPROG) && $(MAKE) clean
	cd $(STAPROG) && $(MAKE) clean
	cd $(XPROG)/$(WIDGET_SET) && $(MAKE) clean

clobber :
	cd $(COMMONDIR) && $(MAKE) clobber
	cd $(PROTOLIB) && $(MAKE) clobber
	cd $(IAFDPROG) && $(MAKE) clobber
	cd $(AMGPROG) && $(MAKE) clobber
	cd $(FDPROG) && $(MAKE) clobber
	cd $(LOGPROGS) && $(MAKE) clobber
	cd $(MISCDIR) && $(MAKE) clobber
	cd $(MAINTPROGS) && $(MAKE) clobber
	cd $(AWPROG) && $(MAKE) clobber
	cd $(AFDDPROG) && $(MAKE) clobber
	cd $(AFDMONPROG) && $(MAKE) clobber
	cd $(STAPROG) && $(MAKE) clobber
	cd $(XPROG)/$(WIDGET_SET) && $(MAKE) clobber

real_clobber :
	$(MAKE) clobber
	cd $(MAINTPROGS) && $(MAKE) real_clobber
	rm -f $(INSTALLDIR)/*
	rm -f ../fifodir/*.fifo* ../fifodir/amg_counter

install :
	cd $(IAFDPROG) && $(MAKE) install
	cd $(AMGPROG) && $(MAKE) install
	cd $(FDPROG) && $(MAKE) install
	cd $(LOGPROGS) && $(MAKE) install
	cd $(MISCDIR) && $(MAKE) install
	cd $(MAINTPROGS) && $(MAKE) install
	cd $(AWPROG) && $(MAKE) install
	cd $(AFDDPROG) && $(MAKE) install
	cd $(AFDMONPROG) && $(MAKE) install
	cd $(STAPROG) && $(MAKE) install
	cd $(XPROG)/$(WIDGET_SET) && $(MAKE) install
	cd $(SCRIPTS) && $(MAKE) install
