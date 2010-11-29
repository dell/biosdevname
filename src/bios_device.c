/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <net/if.h>
#include "list.h"
#include "bios_device.h"
#include "state.h"
#include "libbiosdevname.h"
#include "dmidecode/dmidecode.h"

void free_bios_devices(void *cookie)
{
	struct libbiosdevname_state *state = cookie;
	struct bios_device *dev, *n;
	if (!state)
		return;
	list_for_each_entry_safe(dev, n, &state->bios_devices, node) {
		list_del(&(dev->node));
		free(dev);
	}
}


static void unparse_bios_device(struct bios_device *dev)
{
	char buf[8192];
	memset(buf, 0, sizeof(buf));
	printf("BIOS device: %s\n", dev->bios_name ? dev->bios_name : "");
	if (dev->netdev) {
		unparse_network_device(buf, sizeof(buf), dev->netdev);
		printf("%s", buf);
	}
	else
		printf("  No driver loaded for this device.\n");

	if (is_pci(dev)) {
		unparse_pci_device(buf, sizeof(buf), dev->pcidev);
		printf("%s", buf);
	}
	else if (is_pcmcia(dev)) {
		unparse_pcmcia_device(buf, sizeof(buf), dev->pcmciadev);
		printf("%s", buf);
	}

	printf("\n");
}

void unparse_bios_devices(void *cookie)
{
	struct libbiosdevname_state *state = cookie;
	struct bios_device *dev;
	if (!state)
		return;
	list_for_each_entry(dev, &state->bios_devices, node) {
		unparse_bios_device(dev);
	}
}

void unparse_bios_device_by_name(void *cookie,
				 const char *name)
{

	struct libbiosdevname_state *state = cookie;
	struct bios_device *dev;
	if (!state)
		return;
	list_for_each_entry(dev, &state->bios_devices, node) {
		if (dev->netdev && !strcmp(dev->netdev->kernel_name, name))
			unparse_bios_device(dev);
	}
}

char * kern_to_bios(void *cookie,
		    const char *name)
{
	struct libbiosdevname_state *state = cookie;
	struct bios_device *dev;
	if (!state)
		return NULL;
	list_for_each_entry(dev, &state->bios_devices, node) {
		if (dev->netdev && !strcmp(dev->netdev->kernel_name, name))
			return dev->bios_name;
	}
	return NULL;
}

void unparse_bios_device_list(void *cookie)
{
	struct libbiosdevname_state *state = cookie;
	struct bios_device *dev;
	if (!state)
		return;
	list_for_each_entry(dev, &state->bios_devices, node) {
		unparse_bios_device(dev);
	}
}



/*
 * This sorts all the embedded devices first; by PCI bus/dev/fn, then
 * all the add-in devices by slot number, by pci bus/dev/fn.
 * Unknown location devices show up as physical slot INT_MAX, so they
 * come last.
 */

static int sort_pci(const struct bios_device *bdev_a, const struct bios_device *bdev_b)
{
	const struct pci_device *a = bdev_a->pcidev;
	const struct pci_device *b = bdev_b->pcidev;

	if      (a->physical_slot < b->physical_slot) return -1;
	else if (a->physical_slot > b->physical_slot) return 1;

	if      (a->pci_dev.domain < b->pci_dev.domain) return -1;
	else if (a->pci_dev.domain > b->pci_dev.domain) return  1;

	if      (a->pci_dev.bus < b->pci_dev.bus) return -1;
	else if (a->pci_dev.bus > b->pci_dev.bus) return  1;

	if      (a->pci_dev.dev < b->pci_dev.dev) return -1;
	else if (a->pci_dev.dev > b->pci_dev.dev) return  1;

	if      (a->pci_dev.func < b->pci_dev.func) return -1;
	else if (a->pci_dev.func > b->pci_dev.func) return  1;

	return 0;
}

static int sort_smbios(const struct bios_device *x, const struct bios_device *y)
{
	struct pci_device *a, *b;

	if      (x->pcidev && !y->pcidev) return -1;
	else if (!x->pcidev && y->pcidev) return 1;

	a = x->pcidev;
	b = y->pcidev;

	if      (a->physical_slot == 0 && b->physical_slot == 0) {
		if ( a->smbios_type == b->smbios_type) {
			if ( a->smbios_instance < b->smbios_instance) return -1;
			else if (a->smbios_instance > b->smbios_instance) return 1;
		}
	}
	else {
		if      (a->physical_slot < b->physical_slot) return -1;
		else if (a->physical_slot > b->physical_slot) return 1;
	}

	return sort_pci(x, y);
}


static int sort_pcmcia(const struct bios_device *bdev_a, const struct bios_device *bdev_b)
{
	const struct pcmcia_device *a = bdev_a->pcmciadev;
	const struct pcmcia_device *b = bdev_b->pcmciadev;

	if      (a->socket < b->socket) return -1;
	else if (a->socket > b->socket) return 1;

	if      (a->function < b->function) return -1;
	else if (a->function > b->function) return  1;

	return 0;
}

enum bios_device_types {
	IS_PCI,
	IS_PCMCIA,
	IS_UNKNOWN_TYPE,
};

static int bios_device_type_num(const struct bios_device *dev)
{
	if (is_pci(dev))
		return IS_PCI;
	else if (is_pcmcia(dev))
		return IS_PCMCIA;
	return IS_UNKNOWN_TYPE;
}

static int sort_by_type(const struct bios_device *a, const struct bios_device *b)
{
	if (bios_device_type_num(a) < bios_device_type_num(b))
		return -1;
	else if (bios_device_type_num(a) == bios_device_type_num(b)) {
		if (is_pci(a))
			return sort_smbios(a, b);
		else if (is_pcmcia(a))
			return sort_pcmcia(a, b);
		else return 0;
	}
	else if (bios_device_type_num(a) > bios_device_type_num(b))
		return 1;
	return 0;
}


static void insertion_sort_devices(struct bios_device *a, struct list_head *list,
				       int (*cmp)(const struct bios_device *, const struct bios_device *))
{
	struct bios_device *b;
	list_for_each_entry(b, list, node) {
		if (cmp(a, b) <= 0) {
			list_move_tail(&a->node, &b->node);
			return;
		}
	}
	list_move_tail(&a->node, list);
}


static void sort_device_list(struct libbiosdevname_state *state)
{
	LIST_HEAD(sorted_devices);
	struct bios_device *dev, *tmp;
	list_for_each_entry_safe(dev, tmp, &state->bios_devices, node) {
		insertion_sort_devices(dev, &sorted_devices, sort_by_type);
	}
	list_splice(&sorted_devices, &state->bios_devices);
}

static void match_eth_and_pci_devs(struct libbiosdevname_state *state)
{
	struct pci_device *p;
	struct bios_device *b;
	struct network_device *n;
	char pci_name[40];

	list_for_each_entry(p, &state->pci_devices, node) {
		if (!is_pci_network(p))
			continue;

		unparse_pci_name(pci_name, sizeof(pci_name), &p->pci_dev);
		n = find_net_device_by_bus_info(state, pci_name);
		if (!n)
			continue;

		b = malloc(sizeof(*b));
		if (!b)
			continue;
		memset(b, 0, sizeof(*b));
		INIT_LIST_HEAD(&b->node);
		b->pcidev = p;
		b->netdev = n;
		claim_netdev(b->netdev);
		list_add(&b->node, &state->bios_devices);
	}
}

static void match_eth_and_pcmcia(struct libbiosdevname_state *state)
{
	struct pcmcia_device *p;
	struct bios_device *b;
	char pcmcia_name[40];

	list_for_each_entry(p, &state->pcmcia_devices, node) {
		if (!is_pcmcia_network(p))
			continue;

		b = malloc(sizeof(*b));
		if (!b)
			continue;
		memset(b, 0, sizeof(*b));
		INIT_LIST_HEAD(&b->node);
		b->pcmciadev = p;

		unparse_pcmcia_name(pcmcia_name, sizeof(pcmcia_name), p);
		b->netdev = find_net_device_by_bus_info(state, pcmcia_name);

		claim_netdev(b->netdev);
		list_add(&b->node, &state->bios_devices);
	}
}

static void match_unknown_eths(struct libbiosdevname_state *state)
{
	struct bios_device *b;
	struct network_device *n;
	list_for_each_entry(n, &state->network_devices, node)
	{
		if (netdev_is_claimed(n))
			continue;
		if (!drvinfo_valid(n))
			continue;
		if (!is_ethernet(n)) /* for virtual interfaces */
			continue;
		b = malloc(sizeof(*b));
		if (!b)
			continue;
		memset(b, 0, sizeof(*b));
		INIT_LIST_HEAD(&b->node);
		b->netdev = n;
		list_add(&b->node, &state->bios_devices);
	}
}


static void match_all(struct libbiosdevname_state *state)
{
	match_eth_and_pci_devs(state);
	match_eth_and_pcmcia(state);
	match_unknown_eths(state);
}

static struct libbiosdevname_state * alloc_state(void)
{
	struct libbiosdevname_state *state;
	state = malloc(sizeof(*state));
	if (!state)
		return NULL;
	INIT_LIST_HEAD(&state->bios_devices);
	INIT_LIST_HEAD(&state->pci_devices);
	INIT_LIST_HEAD(&state->network_devices);
	INIT_LIST_HEAD(&state->pcmcia_devices);
	return state;
}

void cleanup_bios_devices(void *cookie)
{
	struct libbiosdevname_state *state = cookie;
	if (!state)
		return;
	free_bios_devices(state);
	free_eths(state);
	free_pcmcia_devices(state);
	free_pci_devices(state);
}

void * setup_bios_devices(int namingpolicy, const char *prefix)
{
	int rc=1;
	struct libbiosdevname_state *state = alloc_state();

	if (!state)
		return NULL;

	rc = get_pci_devices(state);
	if (rc)
		goto out;

	rc = get_pcmcia_devices(state);
	if (rc)
		goto out;
	/* this will fail on Xen guests, that's OK */
	dmidecode_main(state);

	get_eths(state);
	match_all(state);
	sort_device_list(state);
	rc = assign_bios_network_names(state, namingpolicy, prefix);
	if (!rc)
		return state;

out:
	cleanup_bios_devices(state);
	free(state);
	return NULL;
}

