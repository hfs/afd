#
# Makefile to build the statistic program for the AFD
#

include ../Include.mk

INSTDIR = ../..
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

PROGS   = $(STAPROG)$(EXE_EXT) $(SHOPROG)$(EXE_EXT) $(STBPROG)$(EXE_EXT)
OBJS    = $(STAOBJS) $(SHOOBJS) $(STBOBJS)
HEADERS = ../init_afd/afddefs.h ../config.h statdefs.h

all : $(PROGS)

$(STAOBJS) : $(HEADERS)

$(STAPROG)$(EXE_EXT) : $(STAOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(STAOBJS) $(LIB) $(EFENCE)

$(SHOPROG)$(EXE_EXT) : $(SHOOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(SHOOBJS) $(LIB) $(EFENCE)

$(STBPROG)$(EXE_EXT) : $(STBOBJS) $(LIB)
	rm -f $@
	$(CC) $(CFLAGS) -o $@ $(STBOBJS) $(LIB) $(EFENCE)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS) show_bench_stat$(EXE_EXT)

install :
	cd $(INSTDIR)/bin && rm -f $(PROGS)
	cp -p $(PROGS) $(INSTDIR)/bin
	chmod $(BINMODE) $(INSTDIR)/bin/$(STAPROG)$(EXE_EXT) $(INSTDIR)/bin/$(SHOPROG)$(EXE_EXT)

sinstall :
	strip $(PROGS)
	$(MAKE) install

# Other dependencies
afd_stat.o show_stat.o : ../version.h
