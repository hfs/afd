#
# Makefile to build the AFD (Automatic File Distributor)
#

include Include.mk

INSTALLDIR = ../bin
COMMONDIR  = common

AFDLIB     = libafd.a
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

PROGS      = $(AFDLIB) $(MISCDIR) $(IAFDPROG) $(AMGPROG) $(FDPROG) $(LOGPROGS) $(MAINTPROGS) $(AWPROG) $(AFDDPROG) $(AFDMONPROG) $(STAPROG) $(XPROG)

all : $(PROGS)

everything : $(PROGS) $(AFDMONPROG)

$(AFDLIB) : dummy
	cd $(COMMONDIR) ; $(MAKE) all

$(MISCDIR) : dummy
	cd $(@) ; $(MAKE) all

$(IAFDPROG) : dummy
	cd $(@) ; $(MAKE) all

$(AMGPROG) : dummy
	cd $(@) ; $(MAKE) all

$(FDPROG) : dummy
	cd $(@) ; $(MAKE) all

$(LOGPROGS) : dummy
	cd $(@) ; $(MAKE) all

$(MAINTPROGS) : dummy
	cd $(@) ; $(MAKE) all

$(AWPROG) : dummy
	cd $(@) ; $(MAKE) all

$(AFDDPROG) : dummy
	cd $(@) ; $(MAKE) all

$(AFDMONPROG) : dummy
	cd $(@) ; $(MAKE) all

$(STAPROG) : dummy
	cd $(@) ; $(MAKE) all

$(XPROG) : dummy
	cd $(@)/$(WIDGET_SET) ; make all

dummy :

clean :
	cd $(COMMONDIR) ; make clean
	cd $(IAFDPROG) ; make clean
	cd $(AMGPROG) ; make clean
	cd $(FDPROG) ; make clean
	cd $(LOGPROGS) ; make clean
	cd $(MISCDIR) ; make clean
	cd $(MAINTPROGS) ; make clean
	cd $(AWPROG) ; make clean
	cd $(AFDDPROG) ; make clean
	cd $(AFDMONPROG) ; make clean
	cd $(STAPROG) ; make clean
	cd $(XPROG)/$(WIDGET_SET) ; make clean

clobber :
	cd $(COMMONDIR) ; make clobber
	cd $(IAFDPROG) ; make clobber
	cd $(AMGPROG) ; make clobber
	cd $(FDPROG) ; make clobber
	cd $(LOGPROGS) ; make clobber
	cd $(MISCDIR) ; make clobber
	cd $(MAINTPROGS) ; make clobber
	cd $(AWPROG) ; make clobber
	cd $(AFDDPROG) ; make clobber
	cd $(AFDMONPROG) ; make clobber
	cd $(STAPROG) ; make clobber
	cd $(XPROG)/$(WIDGET_SET) ; make clobber

real_clobber :
	make clobber
	cd $(MAINTPROGS) ; make real_clobber
	rm -f $(INSTALLDIR)/*
	rm -f ../fifodir/*.fifo* ../fifodir/amg_counter

install :
	cd $(IAFDPROG) ; make install
	cd $(AMGPROG) ; make install
	cd $(FDPROG) ; make install
	cd $(LOGPROGS) ; make install
	cd $(MISCDIR) ; make install
	cd $(MAINTPROGS) ; make install
	cd $(AWPROG) ; make install
	cd $(AFDDPROG) ; make install
	cd $(AFDMONPROG) ; make install
	cd $(STAPROG) ; make install
	cd $(XPROG)/$(WIDGET_SET) ; make install
