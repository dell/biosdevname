AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = 

dist_noinst_DATA = biosdevname.rules.in biosdevname.spec.fedora biosdevname.spec.suse

CLEANFILES = biosdevname.rules version
biosdevname.rules: biosdevname.rules.in
	sed -e 's#/usr/sbin#$(sbindir)#' $(top_srcdir)/biosdevname.rules.in > $(top_builddir)/biosdevname.rules

install-data-local: biosdevname.rules
	mkdir -p $(DESTDIR)@sysconfdir@/udev/rules.d
	$(INSTALL_DATA) $(top_builddir)/biosdevname.rules $(DESTDIR)@sysconfdir@/udev/rules.d/@RULEDEST@

include src/Makefile.am

# extra space at beginning to work around automake... :(
 ifndef SPEC_FILE
 SPEC_FILE=biosdevname.spec.fedora
 endif
rpm: dist
	mkdir -p dist/{SOURCES,BUILD,RPMS,SRPMS,SPECS}
	cp $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz dist/SOURCES
	cp $(SPEC_FILE) dist/SPECS/@PACKAGE_NAME@.spec
	rpmbuild --define "_topdir $$PWD/dist" -ba --nodeps dist/SPECS/@PACKAGE_NAME@.spec ;
	rm -rf dist/{SOURCES,BUILD,SPECS}

srpm: dist
	mkdir -p dist/{SOURCES,BUILD,RPMS,SRPMS,SPECS}
	cp $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz dist/SOURCES
	cp $(SPEC_FILE) dist/SPECS/@PACKAGE_NAME@.spec
	rpmbuild --define "_topdir $$PWD/dist" -bs --nodeps dist/SPECS/@PACKAGE_NAME@.spec ;
	rm -rf dist/{SOURCES,BUILD,SPECS}
