#
# Makefile to build show_queue for the AFD
#

include ../../../Include.mk

INSTDIR  = ../../../..
COMDIR   = ../../../common
XCOMDIR  = ../common
LIB      = $(XCOMDIR)/libxafd.a $(COMDIR)/libafd.a
CFLAGS   = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I$(XCOMDIR) -I../../.. -I../../../init_afd -I../../../fd -I../afd_ctrl $(XINCLUDES)

SQUEPROG = show_queue
SQUEOBJS = callbacks.o\
           delete_files.o\
           display_data.o\
           format_info.o\
           get_data.o\
           print_data.o\
           show_queue.o

PROGS    = $(SQUEPROG)$(EXE_EXT)
OBJS     = $(SQUEOBJS)
HEADERS  = ../../../init_afd/afddefs.h ../../../config.h $(XCOMDIR)/x_common_defs.h show_queue.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(SQUEPROG)$(EXE_EXT) : $(SQUEOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SQUEOBJS) $(LIB) $(XLIBPATH) $(XMLIBS) $(XLIBS) $(SPECIAL_LIBS) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(SQUEPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
show_queue.o : ../../../version.h ../../../init_afd/permission.h
delete_files.o get_data.o : ../../../fd/fddefs.h
