/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#ifndef BIOS_DEVICE_H_INCLUDED
#define BIOS_DEVICE_H_INCLUDED

#include <pci/pci.h>
#include "list.h"
#include "eths.h"
#include "pci.h"
#include "naming_policy.h"

struct pci_port;

struct bios_device {
	struct list_head node;
	struct network_device *netdev;
	struct pci_device *pcidev;
	char *bios_name;
	struct pci_port *port;
	int duplicate;
};

static inline int is_pci(const struct bios_device *dev)
{
	return dev->pcidev != NULL;
}

#endif /* BIOS_DEVICE_H_INCLUDED */
