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

int system_uses_smbios_names;

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

static void use_kernel_names(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;
	char buffer[IFNAMSIZ];

	memset(buffer, 0, sizeof(buffer));
	list_for_each_entry(dev, &state->bios_devices, node) {
		if (dev->netdev)
			dev->bios_name = dev->netdev->kernel_name;
	}
}


static void pcmcia_names(struct bios_device *dev)
{
	snprintf(dev->bios_name, sizeof(dev->bios_name), "eth_pccard_%u.%u",
		 dev->pcmciadev->socket, dev->pcmciadev->function);
}


static int use_smbios_names(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;
	if (!system_uses_smbios_names) {
		fprintf(stderr, "Error: BIOS does not provide Ethernet device names in SMBIOS.\n");
		fprintf(stderr, "       Use a different naming policy.\n");
		return 1;
	}
	list_for_each_entry(dev, &state->bios_devices, node) {
		if (is_pci(dev) && dev->pcidev->uses_smbios && dev->pcidev->smbios_label) {
			dev->bios_name = dev->pcidev->smbios_label;
		}
		else if (is_pcmcia(dev))
			pcmcia_names(dev);
	}
	return 0;
}


static void use_embedded_ethN_slots_names(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;
	unsigned int i=0;
	char buffer[IFNAMSIZ];

	memset(buffer, 0, sizeof(buffer));
	list_for_each_entry(dev, &state->bios_devices, node) {
		if (is_pci(dev)) {
			if (dev->pcidev->physical_slot == 0)
				snprintf(buffer, sizeof(buffer), "eth%u", i++);
			else if (dev->pcidev->physical_slot < INT_MAX)
				snprintf(buffer, sizeof(buffer), "eth_s%d_%u",
					 dev->pcidev->physical_slot,
					 dev->pcidev->index_in_slot);
			else if (dev->pcidev->physical_slot == INT_MAX)
				snprintf(buffer, sizeof(buffer), "eth_unknown_%u", i++);
		}
		else if (is_pcmcia(dev))
			pcmcia_names(dev);
		dev->bios_name = strdup(buffer);
	}
}

static void use_all_names(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;
	unsigned int i=0;
	char buffer[IFNAMSIZ];

	memset(buffer, 0, sizeof(buffer));
	list_for_each_entry(dev, &state->bios_devices, node) {
		if (is_pci(dev)) {
			if (dev->pcidev->physical_slot < INT_MAX)
				snprintf(buffer, sizeof(buffer), "eth_s%d_%u",
					 dev->pcidev->physical_slot,
					 dev->pcidev->index_in_slot);
			else
				snprintf(buffer, sizeof(buffer), "eth_unknown_%u", i++);
		}
		else if (is_pcmcia(dev))
			pcmcia_names(dev);
		dev->bios_name = strdup(buffer);
	}
}

static void use_embedded(const struct libbiosdevname_state *state, const char *prefix)
{
	struct bios_device *dev;
	char buffer[IFNAMSIZ];

	memset(buffer, 0, sizeof(buffer));
	list_for_each_entry(dev, &state->bios_devices, node) {
		if (is_pci(dev)) {
			if (dev->pcidev->physical_slot == 0) { /* embedded devices only */
				if (dev->pcidev->uses_sysfs & HAS_SYSFS_INDEX) {
					snprintf(buffer, sizeof(buffer), "%s%u", prefix, dev->pcidev->sysfs_index);
					dev->bios_name = strdup(buffer);
				}
				else if (dev->pcidev->uses_smbios) {
					snprintf(buffer, sizeof(buffer), "%s%u", prefix, dev->pcidev->smbios_instance);
					dev->bios_name = strdup(buffer);
				}
			}
		}
	}
}

static void use_pony(const struct libbiosdevname_state *state, const char *prefix)
{
	struct bios_device *dev;
	char buffer[IFNAMSIZ];
	char location[IFNAMSIZ];
	char port[IFNAMSIZ];
	char interface[IFNAMSIZ];
	unsigned int portnum=0;
	int known=0;

	memset(buffer, 0, sizeof(buffer));
	memset(location, 0, sizeof(location));
	memset(port, 0, sizeof(port));
	memset(interface, 0, sizeof(interface));

	list_for_each_entry(dev, &state->bios_devices, node) {
		if (is_pci(dev)) {
			if (dev->pcidev->physical_slot == 0) { /* embedded devices only */
				if (dev->pcidev->uses_sysfs & HAS_SYSFS_INDEX)
					portnum = dev->pcidev->sysfs_index;
				else if (dev->pcidev->uses_smbios)
					portnum = dev->pcidev->smbios_instance;
				snprintf(location, sizeof(location), "%s%u", prefix, portnum);
				known=1;
			}
			else if (dev->pcidev->physical_slot < PHYSICAL_SLOT_UNKNOWN) {
				snprintf(location, sizeof(location), "pci%u", dev->pcidev->physical_slot);
				if (!dev->pcidev->is_virtual_function)
					portnum = dev->pcidev->index_in_slot;
				else
					portnum = dev->pcidev->pf->index_in_slot;
				snprintf(port, sizeof(port), "#%u", portnum);
				known=1;
			}

			if (dev->pcidev->is_virtual_function)
				snprintf(interface, sizeof(interface), "_%u", dev->pcidev->vf_index);

			if (known) {
				snprintf(buffer, sizeof(buffer), "%s%s%s", location, port, interface);
				dev->bios_name = strdup(buffer);
			}
		}
	}
}


int assign_bios_network_names(const struct libbiosdevname_state *state, int sort, int policy, const char *prefix)
{
	int rc = 0;
	if (sort != nosort) {
		switch (policy) {
		case smbios_names:
			rc = use_smbios_names(state);
			break;
		case all_ethN:
			use_all_ethN(state);
			break;
		case embedded_ethN_slots_names:
			use_embedded_ethN_slots_names(state);
			break;
		case all_names:
			use_all_names(state);
			break;
		case kernelnames:
			use_kernel_names(state);
			break;
		case embedded:
			use_embedded(state, prefix);
			break;
		case pony:
		default:
			use_pony(state, prefix);
			break;
		}
	}
	else
		use_kernel_names(state);

	return rc;
}

