#
# Makefile to build the AFDD (AFD daemon)
#

include ../Include.mk

INSTDIR = ../..
COMDIR  = ../common
MISCDIR = ../misc
AFDLIB  = $(COMDIR)/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../log -I../init_afd

AFDDPROG = afdd
AFDDOBJS = afdd.o\
           check_changes.o\
           display_file.o\
	   get_display_data.o\
	   handle_request.o\
	   show_host_list.o\
	   show_summary_stat.o

PROGS   = $(AFDDPROG)
OBJS    = $(AFDDOBJS)
HEADERS = ../init_afd/afddefs.h ../config.h afdddefs.h

all : $(PROGS)

$(OBJS) : $(HEADERS)

$(AFDDPROG) : $(AFDDOBJS) $(AFDLIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(AFDDOBJS) $(AFDLIB) $(SPECIAL_LIBS)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(AFDDPROG)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other depnedencies
afdd.o handle_request.o : ../version.h
handle_request.o : ../log/logdefs.h
