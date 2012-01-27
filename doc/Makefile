#
# Makefile
#

# Default target
all: curdir_all

# Variables
# General variables
ROOTDIR := ..
TOPDIR := ..
include $(TOPDIR)/Makefile.vars

# Components
SUBDIRS :=

man1dir := $(INSTALL_MAN_DIR)/man1
man1pages := bzip.1 bunzip.1

mandirs := $(man1dir)
manpages := $(man1pages)

# Rules
# General rules
include $(ROOTDIR)/Makefile.rules

# Standard commands
curdir_install: manpages_install
curdir_uninstall: manpages_uninstall

# Extra commands
.PHONY: manpages_install manpages_uninstall

manpages_install: $(NEEDS_CONFIGURED)
	$(INSTALL) -d $(mandirs);
	$(INSTALL) $(man1pages) $(man1dir);
	cd $(man1dir); $(GZIP) $(man1pages);

manpages_uninstall:
	cd $(man1dir); for manp in $(man1pages); do rm -f "$$manp" "$${manp}.gz"; done

# End of Makefile
