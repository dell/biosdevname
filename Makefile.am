AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = 

dist_noinst_DATA = biosdevname.rules.in biosdevname.spec.fedora biosdevname.spec.suse

CLEANFILES = biosdevname.rules
biosdevname.rules: biosdevname.rules.in
	sed -e 's#/usr/sbin/#$(sbindir)/#' $(top_srcdir)/biosdevname.rules.in > $(top_builddir)/biosdevname.rules

install-data-local: biosdevname.rules
	mkdir -p $(DESTDIR)@sysconfdir@/udev/rules.d
	$(INSTALL_DATA) $(top_builddir)/biosdevname.rules $(DESTDIR)@sysconfdir@/udev/rules.d/@RULEDEST@

include src/Makefile.am
