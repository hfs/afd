#
# Makefile to build the statistic program for the AFD
#

include ../Include.mk

INSTDIR = ../../bin
COMDIR  = ../common
LIB     = $(COMDIR)/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd

STAPROG = afd_stat
STAOBJS = afd_stat.o\
          eval_input_as.o\
          locate_host.o\
          read_afd_stat_db.o\
          save_old_year.o

SHOPROG = show_stat
SHOOBJS = show_stat.o\
          eval_input_ss.o\
          locate_host.o

STBPROG = show_bench_stat
STBOBJS = show_bench_stat.o

PROGS   = $(STAPROG) $(SHOPROG) $(STBPROG)
OBJS    = $(STAOBJS) $(SHOOBJS) $(STBOBJS)
HEADERS = ../init_afd/afddefs.h ../config.h statdefs.h

all : $(PROGS)

$(STAOBJS) : $(HEADERS)

$(STAPROG) : $(STAOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(STAOBJS) $(LIB) $(EFENCE)

$(SHOPROG) : $(SHOOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SHOOBJS) $(LIB) $(EFENCE)

$(STBPROG) : $(STBOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(STBOBJS) $(LIB) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS) show_bench_stat

install :
	cd $(INSTDIR) && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)
	chmod $(BINMODE) $(INSTDIR)/$(STAPROG) $(INSTDIR)/$(SHOPROG)

# Other dependencies
afd_stat.o show_stat.o : ../version.h
