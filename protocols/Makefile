#
# Makefile to build the AFD protocol library
#

include ../Include.mk

CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../fd
LIB     = libprotocols.a
HEADERS = ../init_afd/afddefs.h ../config.h ../fd/fddefs.h
OBJS    = encode_base64.o\
          ftpcmd.o\
          scp1cmd.o\
          smtpcmd.o\
          wmocmd.o

all : $(LIB)

$(OBJS) : $(HEADERS)

$(LIB) : $(OBJS)
	$(AR) $(ARFLAGS) $@ $?

clean :
	rm -f $(OBJS)

clobber :
	$(MAKE) clean
	rm -f $(LIB)

# Other Dependencies
