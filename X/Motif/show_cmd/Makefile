#
# Makefile to build show_cmd for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../..
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../log -I../afd_ctrl -I../../../afd_mon $(XINCLUDES)

SCMDPROG = show_cmd
SCMDOBJS = cmd_callbacks.o\
           xexec_cmd.o\
           print_data.o\
           show_cmd.o

PROGS    = $(SCMDPROG)$(EXE_EXT)
OBJS     = $(SCMDOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h ../afd_ctrl/afd_ctrl.h $(XCOMDIR)/x_common_defs.h show_cmd.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SCMDPROG)$(EXE_EXT) : $(SCMDOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SCMDOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(SCMDPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
show_cmd.o : ../../../version.h
