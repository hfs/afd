#
# Makefile to build all X-Window programs for the AFD
#

include ../../Include.mk

XAFDLIB   = common
AFDCPROG  = afd_ctrl
DIRCPROG  = dir_ctrl
AFDIPROG  = afd_info
DIRIPROG  = dir_info
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

PROGS = $(XAFDLIB) $(AFDCPROG) $(DIRCPROG) $(AFDIPROG) $(DIRIPROG) $(AFDLPROG) $(EHCPROG) $(SCMDPROG) $(SLOGPROG) $(SOLOGPROG) $(SILOGPROG) $(SDLOGPROG) $(MONCPROG) $(MONIPROG) $(VDCPROG)

all : $(PROGS)

$(XAFDLIB) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(AFDCPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(DIRCPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(AFDIPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(DIRIPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(AFDLPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(EHCPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(SCMDPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(SLOGPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(SOLOGPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(SILOGPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(SDLOGPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(MONCPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(MONIPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

$(VDCPROG) : dummy
	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

#$(STATPROG) : dummy
#	cd $(@) && $(MAKE) $(FORKMAKEFLAG) all

dummy :

clean :
	cd $(XAFDLIB) && $(MAKE) clean
	cd $(AFDCPROG) && $(MAKE) clean
	cd $(DIRCPROG) && $(MAKE) clean
	cd $(AFDIPROG) && $(MAKE) clean
	cd $(DIRIPROG) && $(MAKE) clean
	cd $(AFDLPROG) && $(MAKE) clean
	cd $(EHCPROG) && $(MAKE) clean
	cd $(SCMDPROG) && $(MAKE) clean
	cd $(SLOGPROG) && $(MAKE) clean
	cd $(SOLOGPROG) && $(MAKE) clean
	cd $(SILOGPROG) && $(MAKE) clean
	cd $(SDLOGPROG) && $(MAKE) clean
	cd $(MONCPROG) && $(MAKE) clean
	cd $(MONIPROG) && $(MAKE) clean
	cd $(VDCPROG) && $(MAKE) clean
#	cd $(STATPROG) && $(MAKE) clean

clobber :
	cd $(XAFDLIB) && $(MAKE) clobber
	cd $(DIRCPROG) && $(MAKE) clobber
	cd $(AFDCPROG) && $(MAKE) clobber
	cd $(AFDIPROG) && $(MAKE) clobber
	cd $(DIRIPROG) && $(MAKE) clobber
	cd $(AFDLPROG) && $(MAKE) clobber
	cd $(EHCPROG) && $(MAKE) clobber
	cd $(SCMDPROG) && $(MAKE) clobber
	cd $(SLOGPROG) && $(MAKE) clobber
	cd $(SOLOGPROG) && $(MAKE) clobber
	cd $(SILOGPROG) && $(MAKE) clobber
	cd $(SDLOGPROG) && $(MAKE) clobber
	cd $(MONCPROG) && $(MAKE) clobber
	cd $(MONIPROG) && $(MAKE) clobber
	cd $(VDCPROG) && $(MAKE) clobber
#	cd $(STATPROG) && $(MAKE) clobber

install :
	cd $(AFDCPROG) && $(MAKE) install
	cd $(DIRCPROG) && $(MAKE) install
	cd $(AFDIPROG) && $(MAKE) install
	cd $(DIRIPROG) && $(MAKE) install
	cd $(AFDLPROG) && $(MAKE) install
	cd $(EHCPROG) && $(MAKE) install
	cd $(SCMDPROG) && $(MAKE) install
	cd $(SLOGPROG) && $(MAKE) install
	cd $(SOLOGPROG) && $(MAKE) install
	cd $(SILOGPROG) && $(MAKE) install
	cd $(SDLOGPROG) && $(MAKE) install
	cd $(MONCPROG) && $(MAKE) install
	cd $(MONIPROG) && $(MAKE) install
	cd $(VDCPROG) && $(MAKE) install
#	cd $(STATPROG) && $(MAKE) install
