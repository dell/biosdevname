/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "pirq.h"
#include <pci/pci.h>
#include "pci.h"

static int
is_parent_bridge(struct pci_dev *p, unsigned int target_bus)
{
 	unsigned int primary, secondary;

	if ( (pci_read_word(p, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_BRIDGE)
		return 0;

	primary=pci_read_byte(p, PCI_PRIMARY_BUS);
	secondary=pci_read_byte(p, PCI_SECONDARY_BUS);

	if (secondary != target_bus)
		return 0;

	return 1;
}

static struct pci_dev *
find_parent(struct pci_access *pacc, unsigned int target_bus)
{
	struct pci_dev *p;

	for (p=pacc->devices; p; p=p->next)
		if (is_parent_bridge(p, target_bus))
			return p;

	return NULL;
}

/*
 * Check our parent in case the device itself isn't listed
 * in the PCI IRQ Routing Table.  This has a problem, as
 * our parent bridge on a card may not be included
 * in the $PIR table.  In that case, it falls back to "unknown".
 */
static int pci_dev_to_slot(struct routing_table *table, struct pci_access *pacc, struct pci_dev *p)
{
	int rc;
	rc = pirq_pci_dev_to_slot(table, p->bus, p->dev);
	if (rc == INT_MAX) {
		p = find_parent(pacc, p->bus);
		if (p)
			rc = pirq_pci_dev_to_slot(table, p->bus, p->dev);
	}
	return rc;
}

static const char *read_pci_sysfs_label(const int domain, const int bus, const int device, const int func)
{
	char path[PATH_MAX];
	int rc;
	char *label = NULL;
	snprintf(path, "/sys/devices/pci%04x\:%02x/%04x\:%02x\:%02x.%x/label", domain, bus, domain, bus, device, func);
	rc = sysfs_read_file(path, &label);
	if (rc == 0)
		return label;
	return NULL;
}

static int read_pci_sysfs_index(unsigned int *index, const int domain, const int bus, const int device, const int func)
{
	char path[PATH_MAX];
	int rc;
	char *indexstr = NULL;
	unsigned int i;
	snprintf(path, "/sys/devices/pci%04x\:%02x/%04x\:%02x\:%02x.%x/index", domain, bus, domain, bus, device, func);
	rc = sysfs_read_file(path, &indexstr);
	if (rc == 0) {
		rc = sscanf(indexstr, "%u", &i);
		if (rc == 1)  {
			*index = i;
			return 0;
		}
	}
	return 1;
}

static void fill_pci_dev_sysfs(struct pci_dev *p)
{
	int rc;
	unsigned int index = 0;
	char *label = NULL;
	rc = read_pci_sysfs_index(&index, pci_domain_nr(&p->pci_dev), p->pci_dev.bus, p->pci_dev.dev, p->pci_dev.func);
	if (!rc) {
		p->sysfs_index = index;
		p->uses_sysfs |= HAS_SYSFS_INDEX;
	}
	label = read_pci_sysfs_label(pci_domain_nr(&p->pci_dev), p->pci_dev.bus, p->pci_dev.dev, p->pci_dev.func);
	if (label) {
		p->sysfs_index = index;
		p->uses_sysfs |= HAS_SYSFS_LABEL;
	}
}

static void add_pci_dev(struct libbiosdevname_state *state,
			struct routing_table *table,
			struct pci_access *pacc, struct pci_dev *p)
{
	struct pci_device *dev;
	dev = malloc(sizeof(*dev));
	if (!dev) {
		fprintf(stderr, "out of memory\n");
		return;
	}
	memset(dev, 0, sizeof(*dev));
	INIT_LIST_HEAD(&dev->node);
	memcpy(&dev->pci_dev, p, sizeof(*p)); /* This doesn't allow us to call PCI functions though */
	dev->physical_slot = pci_dev_to_slot(table, pacc, p);
	dev->class         = pci_read_word(p, PCI_CLASS_DEVICE);
	fill_pci_dev_sysfs(p);
	list_add(&dev->node, &state->pci_devices);
}

void free_pci_devices(struct libbiosdevname_state *state)
{
	struct pci_device *pos, *next;
	list_for_each_entry_safe(pos, next, &state->pci_devices, node) {
		if (pos->smbios_label)
			free(pos->smbios_label);
		if (pos->sysfs_label)
			free(pos->sysfs_label);
		list_del(&pos->node);
		free(pos);
	}
}

int get_pci_devices(struct libbiosdevname_state *state)
{
	struct pci_access *pacc;
	struct pci_dev *p;
	struct pci_device *dev;
	struct routing_table *table;
	int rc=0;

	pacc = pci_alloc();
	if (!pacc)
		return rc;

	pci_init(pacc);
	pci_scan_bus(pacc);

	table = pirq_alloc_read_table();
	if (!table)
		goto out;

	for (p=pacc->devices; p; p=p->next) {
		dev = find_dev_by_pci(state, p);
		if (!dev)
			add_pci_dev(state, table, pacc, p);
	}

	pirq_free_table(table);
out:
	pci_cleanup(pacc);
	return rc;
}


static int parse_pci_name(const char *s, int *domain, int *bus, int *dev, int *func)
{
	int err;
/* The domain part was added in 2.6 kernels.  Test for that first. */
	err = sscanf(s, "%x:%2x:%2x.%x", domain, bus, dev, func);
	if (err != 4) {
		err = sscanf(s, "%2x:%2x.%x", bus, dev, func);
		if (err != 3) {
			return 1;
		}
	}
	return 0;
}

int unparse_pci_name(char *buf, int size, const struct pci_dev *pdev)
{
	return snprintf(buf, size, "%04x:%02x:%02x.%d",
			pci_domain_nr(pdev), pdev->bus, pdev->dev, pdev->func);
}

static int unparse_location(char *buf, const int size, const int location)
{
	char *s = buf;
	if (location == 0)
		s += snprintf(s, size-(s-buf), "embedded");
	else if (location == INT_MAX)
		s += snprintf(s, size-(s-buf), "unknown");
	else if (location > 0)
		s += snprintf(s, size-(s-buf), "%u", location);
	else
		s += snprintf(s, size-(s-buf), "unknown");
	return (s-buf);
}

static int unparse_smbios_type41_type(char *buf, const int size, const int type)
{
	char *s = buf;
	const char *msg[] = {"Other",
			     "Unknown",
			     "Video",
			     "SCSI Controller",
			     "Ethernet",
			     "Token Ring",
			     "Sound",
			     "PATA Controller",
			     "SATA Controller",
			     "SAS Controller",
	};
	if (type > 0 && type <= sizeof(msg))
		s += snprintf(s, size-(s-buf), "%s\n", msg[type-1]);
	else
		s += snprintf(s, size-(s-buf), "<OUT OF SPEC>\n");
	return (s-buf);
}


int unparse_pci_device(char *buf, const int size, const struct pci_device *p)
{
	char *s = buf;
	s += snprintf(s, size-(s-buf), "PCI name      : ");
	s += unparse_pci_name(s,  size-(s-buf), &p->pci_dev);
	s += snprintf(s, size-(s-buf), "\n");
	s += snprintf(s, size-(s-buf), "PCI Slot      : ");
	s += unparse_location(s, size-(s-buf), p->physical_slot);
	s += snprintf(s, size-(s-buf), "\n");
	if (p->smbios_type) {
		s += snprintf(s, size-(s-buf), "SMBIOS Device Type: ");
		s += unparse_smbios_type41_type(s, size-(s-buf), p->smbios_type);
		s += snprintf(s, size-(s-buf), "SMBIOS Instance: %u\n", p->smbios_instance);
		s += snprintf(s, size-(s-buf), "SMBIOS Enabled: %s\n", p->smbios_instance?"True":"False");
	}
	if (p->smbios_label)
		s += snprintf(s, size-(s-buf), "SMBIOS Label: %s\n", p->smbios_label);
	if (p->uses_sysfs & HAS_SYSFS_INDEX)
		s += snprintf(s, size-(s-buf), "sysfs Index: %u\n", p->sysfs_index);
	if (p->uses_sysfs & HAS_SYSFS_LABEL)
		s += snprintf(s, size-(s-buf), "sysfs Label: %s\n", p->sysfs_label);
	return (s-buf);
}

static int is_same_pci(const struct pci_dev *a, const struct pci_dev *b)
{
	if (pci_domain_nr(a) == pci_domain_nr(b) &&
	    a->bus == b->bus &&
	    a->dev == b->dev &&
	    a->func == b->func)
		return 1;
	return 0;
}

struct pci_device * find_dev_by_pci(const struct libbiosdevname_state *state,
				    const struct pci_dev *p)
{
	struct pci_device *dev;
	list_for_each_entry(dev, &state->pci_devices, node) {
		if (is_same_pci(p, &dev->pci_dev))
			return dev;
	}
	return NULL;
}

struct pci_device * find_pci_dev_by_pci_addr(const struct libbiosdevname_state *state,
					     const int domain, const int bus, const int device, const int func)
{
	struct pci_device *dev;
	struct pci_device p;

#ifdef HAVE_STRUCT_PCI_DEV_DOMAIN
	p.pci_dev.domain = domain;
#endif
	p.pci_dev.bus = bus;
	p.pci_dev.dev = device;
	p.pci_dev.func = func;

	list_for_each_entry(dev, &state->pci_devices, node) {
		if (is_same_pci(&p.pci_dev, &dev->pci_dev))
			return dev;
	}
	return NULL;
}

struct pci_device * find_dev_by_pci_name(const struct libbiosdevname_state *state,
					 const char *s)
{
	int domain=0, bus=0, device=0, func=0;
	if (parse_pci_name(s, &domain, &bus, &device, &func))
		return NULL;

	return find_pci_dev_by_pci_addr(state, domain, bus, device, func);
}
