#
# Makefile to build all X-Window programs for the AFD
#

include ../../Include.mk

XAFDLIB   = common
AFDCPROG  = afd_ctrl
AFDIPROG  = afd_info
AFDLPROG  = afd_load
EHCPROG   = edit_hc
SCMDPROG  = show_cmd
SLOGPROG  = show_log
SOLOGPROG = show_olog
SILOGPROG = show_ilog
SDLOGPROG = show_dlog
MONCPROG  = mon_ctrl
MONIPROG  = mon_info
VDCPROG   = view_dc
#STATPROG  = show_stat

PROGS = $(XAFDLIB) $(AFDCPROG) $(AFDIPROG) $(AFDLPROG) $(EHCPROG) $(SCMDPROG) $(SLOGPROG) $(SOLOGPROG) $(SILOGPROG) $(SDLOGPROG) $(MONCPROG) $(MONIPROG) $(VDCPROG)

all : $(PROGS)

$(XAFDLIB) : dummy
	cd $(@) ; $(MAKE) all

$(AFDCPROG) : dummy
	cd $(@) ; $(MAKE) all

$(AFDIPROG) : dummy
	cd $(@) ; $(MAKE) all

$(AFDLPROG) : dummy
	cd $(@) ; $(MAKE) all

$(EHCPROG) : dummy
	cd $(@) ; $(MAKE) all

$(SCMDPROG) : dummy
	cd $(@) ; $(MAKE) all

$(SLOGPROG) : dummy
	cd $(@) ; $(MAKE) all

$(SOLOGPROG) : dummy
	cd $(@) ; $(MAKE) all

$(SILOGPROG) : dummy
	cd $(@) ; $(MAKE) all

$(SDLOGPROG) : dummy
	cd $(@) ; $(MAKE) all

$(MONCPROG) : dummy
	cd $(@) ; $(MAKE) all

$(MONIPROG) : dummy
	cd $(@) ; $(MAKE) all

$(VDCPROG) : dummy
	cd $(@) ; $(MAKE) all

#$(STATPROG) : dummy
#	cd $(@) ; $(MAKE) all

dummy :

clean :
	cd $(XAFDLIB) ; make clean
	cd $(AFDCPROG) ; make clean
	cd $(AFDIPROG) ; make clean
	cd $(AFDLPROG) ; make clean
	cd $(EHCPROG) ; make clean
	cd $(SCMDPROG) ; make clean
	cd $(SLOGPROG) ; make clean
	cd $(SOLOGPROG) ; make clean
	cd $(SILOGPROG) ; make clean
	cd $(SDLOGPROG) ; make clean
	cd $(MONCPROG) ; make clean
	cd $(MONIPROG) ; make clean
	cd $(VDCPROG) ; make clean
#	cd $(STATPROG) ; make clean

clobber :
	cd $(XAFDLIB) ; make clobber
	cd $(AFDCPROG) ; make clobber
	cd $(AFDIPROG) ; make clobber
	cd $(AFDLPROG) ; make clobber
	cd $(EHCPROG) ; make clobber
	cd $(SCMDPROG) ; make clobber
	cd $(SLOGPROG) ; make clobber
	cd $(SOLOGPROG) ; make clobber
	cd $(SILOGPROG) ; make clobber
	cd $(SDLOGPROG) ; make clobber
	cd $(MONCPROG) ; make clobber
	cd $(MONIPROG) ; make clobber
	cd $(VDCPROG) ; make clobber
#	cd $(STATPROG) ; make clobber

install :
	cd $(AFDCPROG) ; make install
	cd $(AFDIPROG) ; make install
	cd $(AFDLPROG) ; make install
	cd $(EHCPROG) ; make install
	cd $(SCMDPROG) ; make install
	cd $(SLOGPROG) ; make install
	cd $(SOLOGPROG) ; make install
	cd $(SILOGPROG) ; make install
	cd $(SDLOGPROG) ; make install
	cd $(MONCPROG) ; make install
	cd $(MONIPROG) ; make install
	cd $(VDCPROG) ; make install
#	cd $(STATPROG) ; make install
