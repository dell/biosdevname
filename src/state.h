/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#ifndef LIBBIOSDEVICE_STATE_H_INCLUDED
#define LIBBIOSDEVICE_STATE_H_INCLUDED

#include <pci/pci.h>
#include "list.h"
#include "pirq.h"

struct libbiosdevname_state {
	struct list_head bios_devices;
	struct list_head pci_devices;
	struct list_head network_devices;
	struct list_head slots;
	struct pci_access *pacc;
	struct routing_table *pirq_table;
};

#endif /* LIBBIOSDEVICESTATE_H_INCLUDED */
