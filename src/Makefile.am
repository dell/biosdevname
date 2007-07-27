INCLUDES = -I..
AM_CFLAGS = -Wall -fno-strict-aliasing

lib_LTLIBRARIES = libbiosdevname.la
libbiosdevname_la_SOURCES = bios_device.c bios_device.h pirq.c pirq.h pci.c pci.h eths.c eths.h \
	ethtool-util.h ethtool-copy.h list.h \
        read_proc.c naming_policy.c naming_policy.h \
	pcmcia.h pcmcia.c cistpl.h read-cis.c parse_cis.c state.h libbiosdevname.h \
	dmidecode/config.h \
	dmidecode/dmidecode.c \
	dmidecode/dmidecode.h \
	dmidecode/dmioem.c \
	dmidecode/dmioem.h \
	dmidecode/types.h \
	dmidecode/util.c \
	dmidecode/util.h


sbin_PROGRAMS = bios_dev_name bios_dev_nameS
bios_dev_name_SOURCES =  bios_dev_name.c bios_dev_name.h
bios_dev_name_LDADD = libbiosdevname.la

bios_dev_nameS_SOURCES =  bios_dev_name.c bios_dev_name.h
bios_dev_nameS_LDADD = libbiosdevname.la
bios_dev_nameS_LDFLAGS = -all-static
