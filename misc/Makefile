#
# Makefile to build the misc modules of the AFD
#
############################################################################
# Select able precompiler options:
# ===============================
#
#   _EOL_TIME_TEST             : This is only good for the gts2tiff()
#                                function. It takes the time how long it
#                                takes to find all EOL in a given T4
#                                data block. It also tests how long it
#                                takes to duplicate all lines.
#
#   _TIFF_CONVERSION_TIME_TEST : This is only good for the gts2tiff()
#                                function. It takes the time how long it
#                                takes to convert the given file from T4
#                                to TIFF.
#
#   __WITH_FILE_INFO           : Again this is only good for the function
#                                gts2tiff(). It write the following
#                                information to the system log:
#                                    - file name
#                                    - bulletin header
#                                    - horizontal and vertical resolution
#                                    - number of EOL's
#
############################################################################

include ../Include.mk

INSTDIR = ../..
AFDLIB  = ../common/libafd.a
CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) -I. -I.. -I../init_afd -I../amg

MISCLIB = libmisc.a
MISCOBJS = afw2wmo.o\
           assemble.o\
           bin_file_chopper.o\
           create_eumetsat_header.o\
           extract.o\
           gts2tiff.o\
           tiff2gts.o

CENPROG  = create_eumetsat_name
CENOBJS  = create_eumetsat_name.o

OBJS     = $(MISCOBJS) $(CENOBJS)

HEADERS = ../init_afd/afddefs.h ../config.h

all : $(MMAP_FLAG) $(MISCLIB)

mapper.o $(MISCOBJS) : $(HEADERS)

-D_NO_MMAP : mapper.o $(AFDLIB)
	$(CC) $(CFLAGS) -o mapper mapper.o $(AFDLIB)

$(MISCLIB) : $(MISCOBJS)
	$(AR) $(ARFLAGS) $@ $?

$(CENPROG) : $(CENOBJS) $(AFDLIB)
	$(CC) $(CFLAGS) -o $@ $(CENOBJS) $(AFDLIB) $(EFENCE) $(CCMALLOC)

clean :
	@if [ "$(MMAP_FLAG)" = "-D_NO_MMAP" ] ; \
	then \
	   rm -f mapper.o ; \
	fi
	rm -f $(OBJS) core

clobber :
	$(MAKE) clean
	@if [ "$(MMAP_FLAG)" = "-D_NO_MMAP" ] ; \
	then \
	   rm -f mapper ; \
	fi
	rm -f $(MISCLIB) $(CENPROG)

install :
	@if [ "$(MMAP_FLAG)" = "-D_NO_MMAP" ] ; \
	then \
	   cp -p mapper $(INSTDIR)/bin ; \
	   chmod $(BINMODE) $(INSTDIR)/bin/mapper ; \
	fi

sinstall :
	@if [ "$(MMAP_FLAG)" = "-D_NO_MMAP" ] ; \
	then \
	   strip mapper ; \
	   cp -p mapper $(INSTDIR)/bin ; \
	   chmod $(BINMODE) $(INSTDIR)/bin/mapper ; \
	fi

# Other depnedencies
create_eumetsat_header.o : eumetsat_header_defs.h
extract.o assemble.o : ../amg/amgdefs.h
