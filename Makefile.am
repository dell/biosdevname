SUBDIRS = src

dist_noinst_DATA = 60-bios_dev_name.rules.in bios_dev_name.spec

60-bios_dev_name.rules: 60-bios_dev_name.rules.in
	sed -e 's#/usr/sbin/#$(sbindir)/#' 60-bios_dev_name.rules.in > 60-bios_dev_name.rules

install-data-local: 60-bios_dev_name.rules
	mkdir -p $(DESTDIR)@sysconfdir@/udev/rules.d
	$(INSTALL_DATA) $(srcdir)/60-bios_dev_name.rules $(DESTDIR)@sysconfdir@/udev/rules.d/60-bios_dev_name.rules
