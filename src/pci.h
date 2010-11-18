/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */

#ifndef PCI_H_INCLUDED
#define PCI_H_INCLUDED

#include <limits.h>
#include <pci/pci.h>
#include "list.h"
#include "state.h"
#include "config.h"

struct pci_device {
	struct list_head node;
	struct pci_dev pci_dev;
	int physical_slot;
	unsigned int index_in_slot;
	unsigned short int class;
	unsigned char uses_smbios;
	unsigned char smbios_type;
	unsigned char smbios_instance;
	unsigned char smbios_enabled;
	char *smbios_label;
	unsigned int sysfs_index;
	char * sysfs_label;
	unsigned char uses_sysfs;
	unsigned char is_virtual_function:1;
};

#define HAS_SYSFS_INDEX 1
#define HAS_SYSFS_LABEL 2
#define PHYSICAL_SLOT_UNKNOWN (INT_MAX)

extern int get_pci_devices(struct libbiosdevname_state *state);
extern void free_pci_devices(struct libbiosdevname_state *state);

extern struct pci_device * find_dev_by_pci(const struct libbiosdevname_state *state, const struct pci_dev *p);
extern struct pci_device * find_pci_dev_by_pci_addr(const struct libbiosdevname_state *state, const int domain, const int bus, const int device, const int func);
extern struct pci_device * find_dev_by_pci_name(const struct libbiosdevname_state *state, const char *s);
extern int unparse_pci_device(char *buf, const int size, const struct pci_device *p);
extern int unparse_pci_name(char *buf, int size, const struct pci_dev *pdev);

static inline int is_pci_network(struct pci_device *dev)
{
	return (dev->class & 0xFF00) == 0x0200;
}

#ifdef HAVE_STRUCT_PCI_DEV_DOMAIN
static inline int pci_domain_nr(const struct pci_dev *dev)
{
	return dev->domain;
}
#else
static inline int pci_domain_nr(const struct pci_dev *dev)
{
	return 0;
}
#endif


#endif /* PCI_H_INCLUDED */
