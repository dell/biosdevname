/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#ifndef LIBBIOSDEVNAME_H_INCLUDED
#define LIBBIOSDEVNAME_H_INCLUDED

enum namingpolicy {
	physical,
	all_ethN,
};

extern void * setup_bios_devices(int namingpolicy, const char *prefix);
extern void cleanup_bios_devices(void *cookie);
extern char * kern_to_bios(void *cookie, const char *devname);
extern void unparse_bios_devices(void *cookie);
extern void unparse_bios_device_by_name(void *cookie, const char *name);



#endif /* LIBBIOSDEVNAME_H_INCLUDED */
