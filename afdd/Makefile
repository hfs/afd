#
# Makefile to build the AFDD (AFD daemon)
#

include ../Include.mk

INSTDIR = ../../bin
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
	$(CC) $(CFLAGS) -o $@ $(AFDDOBJS) $(AFDLIB) $(SPECIAL_LIBS)

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(PROGS)

install :
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(AFDDPROG)

# Other depnedencies
afdd.o handle_request.o : ../version.h
handle_request.o : ../log/logdefs.h
