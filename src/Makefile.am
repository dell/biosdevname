INCLUDES = -I..
AM_CFLAGS = -Wall -fno-strict-aliasing

srcPath=src/

sbin_PROGRAMS = src/biosdevname src/biosdevnameS
src_biosdevname_SOURCES =  \
	$(srcPath)/bios_dev_name.c \
	$(srcPath)/bios_device.c \
	$(srcPath)/pirq.c \
	$(srcPath)/pci.c \
	$(srcPath)/eths.c \
	$(srcPath)/read_proc.c \
	$(srcPath)/naming_policy.c \
	$(srcPath)/pcmcia.c \
	$(srcPath)/read-cis.c \
	$(srcPath)/parse_cis.c \
	$(srcPath)/dmidecode/dmidecode.c \
	$(srcPath)/dmidecode/dmioem.c \
	$(srcPath)/dmidecode/util.c

src_biosdevnameS_SOURCES =  $(src_biosdevname_SOURCES)
src_biosdevnameS_LDFLAGS = -all-static

EXTRA_DIST = \
	$(srcPath)/bios_dev_name.h \
	$(srcPath)/bios_device.h \
	$(srcPath)/pirq.h \
	$(srcPath)/pci.h \
	$(srcPath)/eths.h \
	$(srcPath)/ethtool-util.h \
	$(srcPath)/ethtool-copy.h \
	$(srcPath)/list.h \
	$(srcPath)/naming_policy.h \
	$(srcPath)/pcmcia.h \
	$(srcPath)/cistpl.h \
	$(srcPath)/state.h \
	$(srcPath)/libbiosdevname.h \
	$(srcPath)/dmidecode/config.h \
	$(srcPath)/dmidecode/dmidecode.h \
	$(srcPath)/dmidecode/dmioem.h \
	$(srcPath)/dmidecode/types.h \
	$(srcPath)/dmidecode/util.h
