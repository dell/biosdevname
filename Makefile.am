SUBDIRS = src

dist_noinst_DATA = biosdevname.rules.in biosdevname.spec

biosdevname.rules: biosdevname.rules.in
	sed -e 's#/usr/sbin/#$(sbindir)/#' biosdevname.rules.in > biosdevname.rules

install-data-local: biosdevname.rules
	mkdir -p $(DESTDIR)@sysconfdir@/udev/rules.d
	$(INSTALL_DATA) $(srcdir)/biosdevname.rules $(DESTDIR)@sysconfdir@/udev/rules.d/@RULEDEST@
