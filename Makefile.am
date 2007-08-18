SUBDIRS = src

dist_noinst_DATA = bios_dev_name.rules.in bios_dev_name.spec

bios_dev_name.rules: bios_dev_name.rules.in
	sed -e 's#/usr/sbin/#$(sbindir)/#' bios_dev_name.rules.in > bios_dev_name.rules

install-data-local: bios_dev_name.rules
	mkdir -p $(DESTDIR)@sysconfdir@/udev/rules.d
	$(INSTALL_DATA) $(srcdir)/bios_dev_name.rules $(DESTDIR)@sysconfdir@/udev/rules.d/@RULEDEST@
