## Makefile.am

if HAVE_MOTIF
MOTIF_DIR = Motif
else
MOTIF_DIR =
endif

if WITH_GTK
GTK_DIR = Gtk+
else
GTK_DIR =
endif

MAINTAINERCLEANFILES = Makefile.in
SUBDIRS              = common $(MOTIF_DIR) $(GTK_DIR)
