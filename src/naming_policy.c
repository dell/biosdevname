/*
 *  Copyright (c) 2006, 2007 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "bios_device.h"
#include "naming_policy.h"
#include "libbiosdevname.h"
#include "state.h"
#include "dmidecode/dmidecode.h"

static void use_all_ethN(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;
	unsigned int i=0;
	char buffer[IFNAMSIZ];

	memset(buffer, 0, sizeof(buffer));
	list_for_each_entry(dev, &state->bios_devices, node) {
		if (dev->netdev) {
			snprintf(buffer, sizeof(buffer), "eth%u", i++);
			dev->bios_name = strdup(buffer);
		}
	}
}

static void use_physical(const struct libbiosdevname_state *state, const char *prefix)
{
	struct bios_device *dev;
	char buffer[IFNAMSIZ];
	char location[IFNAMSIZ];
	char port[IFNAMSIZ];
	char interface[IFNAMSIZ];
	unsigned int portnum=0;
	int known=0;
	struct pci_device *vf;

	list_for_each_entry(dev, &state->bios_devices, node) {
		known = 0;
		memset(buffer, 0, sizeof(buffer));
		memset(location, 0, sizeof(location));
		memset(port, 0, sizeof(port));
		memset(interface, 0, sizeof(interface));

		if (is_pci(dev)) {
			vf = dev->pcidev;
			if (dev->pcidev->physical_slot == 0) { /* embedded devices only */
				portnum = INT_MAX;
				/* Use master VPD device if available */
				if (vf->vpd_pf)
					vf = vf->vpd_pf;
				if (vf->pf)
				  	vf = vf->pf;
				if (dev->port)
					portnum = dev->port->port;
				else if (vf->uses_sysfs & HAS_SYSFS_INDEX)
					portnum = vf->sysfs_index;
				else if (vf->uses_smbios & HAS_SMBIOS_INSTANCE && is_pci_smbios_type_ethernet(vf))
					portnum = vf->smbios_instance;
				else if (vf->embedded_index_valid)
					portnum = vf->embedded_index;
				if (portnum != INT_MAX) {	
					snprintf(location, sizeof(location), "%s%u", prefix, portnum);
					known=1;
				}
			}
			else if (dev->pcidev->physical_slot < PHYSICAL_SLOT_UNKNOWN) {
				snprintf(location, sizeof(location), "p%u", dev->pcidev->physical_slot);
				if (dev->port)
					portnum = dev->port->port;
				else if (dev->pcidev->vpd_port < INT_MAX)
					portnum = dev->pcidev->vpd_port;
				else if (!dev->pcidev->is_sriov_virtual_function)
				  	portnum = dev->pcidev->index_in_slot;
				else
					portnum = dev->pcidev->pf->index_in_slot;
				snprintf(port, sizeof(port), "p%u", portnum);
				known=1;
			}

			if (dev->port && dev->port->pfi != -1)
				snprintf(interface, sizeof(interface), "_%u", dev->port->pfi);
			else if (dev->pcidev->is_sriov_virtual_function)
				snprintf(interface, sizeof(interface), "_%u", dev->pcidev->vf_index);
			else if (dev->pcidev->vpd_pfi < INT_MAX)
				snprintf(interface, sizeof(interface), "_%u", dev->pcidev->vpd_pfi);

			if (known) {
				snprintf(buffer, sizeof(buffer), "%s%s%s", location, port, interface);
				dev->bios_name = strdup(buffer);
			}
		}
	}
}


int assign_bios_network_names(const struct libbiosdevname_state *state, int policy, const char *prefix)
{
	int rc = 0;
	switch (policy) {
	case all_ethN:
		use_all_ethN(state);
		break;
	case physical:
	default:
		use_physical(state, prefix);
		break;
	}

	return rc;
}

