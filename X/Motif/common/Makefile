#
# Makefile to build the XAFD library
#

include ../../../Include.mk

CFLAGS  = $(DEBUG_FLAGS) $(COMPILER_FLAGS) $(XSTATIC_FLAG) -I. -I../../.. -I../../../init_afd -I../afd_ctrl -I../show_olog
LIB     = libxafd.a
HEADERS = ../afd_ctrl/afd_ctrl.h ../../../config.h
OBJS    = check_info_file.o\
          check_nummeric.o\
          get_current_jid_list.o\
          get_ip_no.o\
          get_printer_cmd.o\
          handle_setup_file.o\
          init_color.o\
          locate_xy.o\
          make_xprocess.o\
          sfilter.o\
          show_info.o\
          show_message.o\
          store_host_names.o\
          update_time.o\
          xrec.o

all : $(LIB)

$(OBJS) : $(HEADERS)

$(LIB) : $(OBJS)
	$(AR) $(ARFLAGS) $@ $?

clean :
	rm -f $(OBJS) core

clobber :
	make clean
	rm -f $(LIB)

# Other Dependencies
