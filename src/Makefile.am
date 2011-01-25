INCLUDES = -I..
AM_CFLAGS = -Wall -fno-strict-aliasing

man_MANS = biosdevname.1

sbin_PROGRAMS = src/biosdevname src/dump_pirq
src_biosdevname_SOURCES =  \
	src/bios_dev_name.c \
	src/bios_device.c \
	src/pirq.c \
	src/pci.c \
	src/eths.c \
	src/read_proc.c \
	src/naming_policy.c \
	src/sysfs.c \
	src/dmidecode/dmidecode.c \
	src/dmidecode/dmioem.c \
	src/dmidecode/util.c
src_biosdevname_LDADD = -lz -lpci

src_dump_pirq_SOURCES = src/pirq.c
src_dump_pirq_CFLAGS = -DUNIT_TEST_PIRQ

EXTRA_DIST += \
	src/bios_dev_name.h \
	src/bios_device.h \
	src/pirq.h \
	src/pci.h \
	src/eths.h \
	src/ethtool-util.h \
	src/ethtool-copy.h \
	src/list.h \
	src/naming_policy.h \
	src/state.h \
	src/sysfs.h \
	src/libbiosdevname.h \
	src/dmidecode/config.h \
	src/dmidecode/dmidecode.h \
	src/dmidecode/dmioem.h \
	src/dmidecode/types.h \
	src/dmidecode/util.h
