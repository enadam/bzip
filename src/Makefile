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

# Program information
prog_name := bzip

target_fname := p
ifeq ($(HAVE_CONFIGURED), yes)
 ifneq ($(DEBUG), yes)
  target_fname := $(shell echo $(prog_name) \
	| sed -e '$(INSTALL_TRANSFORM_NAME)')
 endif
endif

# Program argumnets
CPPFLAGS += -DHAVE_CONFIG_H -I$(TOPDIR) @bzip_cppflags@
CFLAGS += @bzip_cflags@
LDFLAGS += @bzip_ldflags@
LIBS += @bzip_libs@
OBJS += @bzip_objs@

# Components
SUBDIRS :=

sources := main.c version.c crc.c models.c compress.c decompress.c
headers := $(TOPDIR)/config.h $(TOPDIR)/confdeps.h \
	main.h cmdline.h version.h lc_common.h \
	bzip.h bitstream.h crc.h models.h \
	compress.h
objs := main.o version.o crc.o models.o $(OBJS)

# Rules
# General rules
include $(ROOTDIR)/Makefile.rules

# Implicit rules
%.s: $(NEEDS_CONFIGURED) %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -S $<;

%.o: $(NEEDS_CONFIGURED) %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<;

# Explicit rules
include deps

$(target_fname): $(NEEDS_CONFIGURED) version $(objs)
	$(CC) $(LDFLAGS) $(objs) $(LIBS) -o $@;

version: $(sources)
	$(CHVERS) version BUILD -i;

version.h: version.h.in version
	$(SHVERS) version < version.h.in > version.h;

# Standard commands
curdir_depend: $(NEEDS_CONFIGURED)
	$(C_MAKEDEP) $(CPPFLAGS) $(sources) > deps;

curdir_all: $(target_fname)

curdir_install: $(NEEDS_CONFIGURED) $(target_fname)
	$(INSTALL) -d $(INSTALL_BIN_DIR);
	$(INSTALL_PROG) $(target_fname) $(INSTALL_BIN_DIR);

curdir_uninstall:
	rm -f $(INSTALL_BIN_DIR)/$(target_fname);

curdir_clean:
	rm -f core $(target_fname) $(objs);

curdir_distclean:
	cp -f Makefile.in Makefile;

curdir_mostlyclean:
	rm -f version.h;

curdir_maintainerclean:
	rm -f tags;

# Extra commands
.PHONY: test ctags setver

test: $(target_fname)
	./$(target_fname) -1 -fc test/test1.dat | cmp test/test1.dat.bz;
	./$(target_fname) -2 -fc test/test2.dat | cmp test/test2.dat.bz;
	./$(target_fname) -dc test/test1.dat.bz | cmp test/test1.dat;
	./$(target_fname) -dc test/test2.dat.bz | cmp test/test2.dat;
	@echo "All tests have passed correctly.";

ctags: $(NEEDS_CONFIGURED)
	$(C_TAGS) $(sources) $(headers);

setver: $(NEEDS_CONFIGURED) version
	@ \
	newname="$(ROOTDIR)/../$(prog_name)_`$(SHVERS) version -progver`"; \
	[ -e "$$newname" ] || mv $(ROOTDIR) "$$newname";

# End of Makefile
