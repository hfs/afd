CFLAGS  = -O2

TF1PROG = tfilter1
TF1OBJS = tfilter.o\
          filter1.o

TF2PROG = tfilter2
TF2OBJS = tfilter.o\
          filter2.o

TF3PROG = tfilter3
TF3OBJS = tfilter.o\
          filter3.o

PROGS   = $(TF1PROG)$(EXE_EXT) $(TF2PROG)$(EXE_EXT) $(TF3PROG)$(EXE_EXT)
OBJS    = $(TF1OBJS) $(TF2OBJS) $(TF3OBJS)

all : $(PROGS)

$(TF1PROG)$(EXE_EXT) : $(TF1OBJS)
	$(CC) $(CFLAGS) -o $@ $(TF1OBJS)

$(TF2PROG)$(EXE_EXT) : $(TF2OBJS)
	$(CC) $(CFLAGS) -o $@ $(TF2OBJS)

$(TF3PROG)$(EXE_EXT) : $(TF3OBJS)
	$(CC) $(CFLAGS) -o $@ $(TF3OBJS)

clean :
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	rm -f $(PROGS)
