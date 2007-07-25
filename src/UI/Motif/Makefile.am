## Makefile.am

if WITH_INPUT_LOG
   AFD_SHOW_ILOG = show_ilog
else
   AFD_SHOW_ILOG = 
endif
if WITH_OUTPUT_LOG
   AFD_SHOW_OLOG = show_olog
else
   AFD_SHOW_OLOG = 
endif
if WITH_DELETE_LOG
   AFD_SHOW_DLOG = show_dlog
else
   AFD_SHOW_DLOG = 
endif

if WITH_AFD_MON
   AFD_MON_DIR = mon_ctrl mon_info
else
   AFD_MON_DIR = 
endif

if WITH_AFD_MON_ONLY
   OTHER_DIRS = common mon_ctrl mon_info show_log
else
   OTHER_DIRS = common afd_ctrl dir_ctrl afd_info dir_info afd_load\
                edit_hc show_cmd show_log $(AFD_SHOW_OLOG) $(AFD_SHOW_ILOG)\
                $(AFD_SHOW_DLOG) show_elog show_queue view_dc xsend_file\
                xshow_stat handle_event $(AFD_MON_DIR)
endif

MAINTAINERCLEANFILES = Makefile.in
SUBDIRS              = $(OTHER_DIRS)
EXTRA_DIST           = AFD
