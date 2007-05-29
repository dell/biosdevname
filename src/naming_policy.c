/*
 *  Copyright (c) 2006 Dell, Inc.
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


static void use_kernel_names(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;

	list_for_each_entry(dev, &state->bios_devices, node) {
		if (dev->netdev)
			strncpy(dev->bios_name, dev->netdev->kernel_name, sizeof(dev->bios_name));
	}
}

static void use_all_ethN(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;
	unsigned int i=0;

	list_for_each_entry(dev, &state->bios_devices, node) {
		snprintf(dev->bios_name, sizeof(dev->bios_name), "eth%u", i++);
	}
}

static void pcmcia_names(struct bios_device *dev)
{
	snprintf(dev->bios_name, sizeof(dev->bios_name), "eth_pccard_%u.%u",
		 dev->pcmciadev->socket, dev->pcmciadev->function);
}

static void use_embedded_ethN_slots_names(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;
	unsigned int i=0;

	list_for_each_entry(dev, &state->bios_devices, node) {
		if (is_pci(dev)) {
			if (dev->pcidev->physical_slot == 0)
				snprintf(dev->bios_name, sizeof(dev->bios_name), "eth%u", i++);
			else if (dev->pcidev->physical_slot < INT_MAX)
				snprintf(dev->bios_name, sizeof(dev->bios_name), "eth_s%d_%u",
					 dev->pcidev->physical_slot,
					 dev->pcidev->index_in_slot);
			else if (dev->pcidev->physical_slot == INT_MAX)
				snprintf(dev->bios_name, sizeof(dev->bios_name), "eth_unknown_%u", i++);
		}
		else if (is_pcmcia(dev))
			pcmcia_names(dev);
	}
}

static void use_all_names(const struct libbiosdevname_state *state)
{
	struct bios_device *dev;
	unsigned int i=0;

	list_for_each_entry(dev, &state->bios_devices, node) {
		if (is_pci(dev)) {
			if (dev->pcidev->physical_slot < INT_MAX)
				snprintf(dev->bios_name, sizeof(dev->bios_name), "eth_s%d_%u",
					 dev->pcidev->physical_slot,
					 dev->pcidev->index_in_slot);
			else
				snprintf(dev->bios_name, sizeof(dev->bios_name), "eth_unknown_%u", i++);
		}
		else if (is_pcmcia(dev))
			pcmcia_names(dev);
	}
}

int assign_bios_network_names(const struct libbiosdevname_state *state, int sort, int policy)
{
	if (sort != nosort) {
		switch (policy) {
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
		default:
			use_kernel_names(state);
			break;
		}
	}
	else
		use_kernel_names(state);

	return 0;
}

