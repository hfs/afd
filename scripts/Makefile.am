## Makefile.am

if WITH_AFD_MON
   AFD_MON_FILES = rmon_ctrl rmon_ctrl_ssh
   EXTRA_DIST    = rafdd_cmd.in rafdd_cmd_ssh.in rmon_ctrl.in rmon_ctrl_ssh.in\
                   afdalarm.in afd.in afd.sysconfig afd_auto_config.in
else
   AFD_MON_FILES =
   EXTRA_SCRIPTS = rmon_ctrl rmon_ctrl_ssh
   EXTRA_DIST    = rafdd_cmd.in rafdd_cmd_ssh.in afdalarm.in afd.in\
                   afd.sysconfig afd_auto_config.in
endif

MAINTAINERCLEANFILES = Makefile.in
bin_SCRIPTS          = afdalarm afd_auto_config rafdd_cmd rafdd_cmd_ssh\
                       $(AFD_MON_FILES)
