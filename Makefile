#
# Makefile
#

# Default target
all: curdir_all

# Variables
# General variables
ROOTDIR := .
TOPDIR := .
include $(TOPDIR)/Makefile.vars

# Package information
# Components
SUBDIRS := src doc

# Rules
# General rules
include $(ROOTDIR)/Makefile.rules
include $(ROOTDIR)/Makefile.autoconf

# Standard commands
curdir_clean:
	rm -f config.h;

curdir_distclean: autoconf_distclean
	cp -f Makefile.vars.in Makefile.vars;

curdir_mostlyclean: autoconf_mostlyclean

# Extra commands
.PHONY: install_bin uninstall_bin \
	install_doc uninstall_doc \
	install_data uninstall_data

install_bin uninstall_bin:
	$(MAKE) -C src install;

install_doc uninstall_doc:
	$(MAKE) -C doc install;

install_data uninstall_data:

# End of Makefile
