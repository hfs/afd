#
# Makefile to build show_cmd for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../../bin
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(COMDIR)/libafd.a $(XCOMDIR)/libxafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../log -I../afd_ctrl -I../../../afd_mon

SCMDPROG = show_cmd
SCMDOBJS = cmd_callbacks.o\
           exec_cmd.o\
           show_cmd.o

PROGS    = $(SCMDPROG)
OBJS     = $(SCMDOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h ../afd_ctrl/afd_ctrl.h $(XCOMDIR)/x_common_defs.h show_cmd.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SCMDPROG) : $(SCMDOBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SCMDOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(SCMDPROG)

# Other dependencies
show_cmd.o : ../../../version.h
