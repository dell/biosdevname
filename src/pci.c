/*
 *  Copyright (c) 2006-2010 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <pci/pci.h>
#include "pci.h"
#include "sysfs.h"
#include "dmidecode/dmidecode.h"
#include "pirq.h"

static int read_pci_sysfs_path(char *buf, size_t bufsize, const struct pci_dev *pdev)
{
	char path[PATH_MAX];
	char pci_name[16];
	ssize_t size;
	unparse_pci_name(pci_name, sizeof(pci_name), pdev);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s", pci_name);
	size = readlink(path, buf, bufsize);
	if (size == -1)
		return 1;
	return 0;
}

static int read_pci_sysfs_physfn(char *buf, size_t bufsize, const struct pci_dev *pdev)
{
	char path[PATH_MAX];
	char pci_name[16];
	ssize_t size;
	unparse_pci_name(pci_name, sizeof(pci_name), pdev);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/physfn", pci_name);
	size = readlink(path, buf, bufsize);
	if (size == -1)
		return 1;
	return 0;
}

static int virtfn_filter(const struct dirent *dent)
{
        return (!strncmp(dent->d_name,"virtfn",6));
}

static int _read_virtfn_index(unsigned int *index, const char *path, const char *basename, const char *pci_name)
{
	char buf[PATH_MAX], *b;
	char fullpath[PATH_MAX];
	ssize_t size;
	unsigned int u=INT_MAX;
	int scanned, rc=1;

	snprintf(fullpath, sizeof(fullpath), "%s/%s", path, basename);
	size = readlink(fullpath, buf, sizeof(buf));
	if (size > 0) {
		/* form is ../0000:05:10.0 */
		b=buf+3; /* skip ../ */
		if (strlen(b) == strlen(pci_name) &&
		    !strncmp(b, pci_name, strlen(pci_name))) {
			scanned = sscanf(basename, "virtfn%u", &u);
			if (scanned == 1) {
				rc = 0;
				*index = u;
			}
		}
	}
	return rc;
}

static int read_virtfn_index(unsigned int *index, const struct pci_dev *pdev)
{
	char pci_name[16];
	char path[PATH_MAX];
	char cpath[PATH_MAX];
	struct dirent **namelist;
	int n, rc=1;

	unparse_pci_name(pci_name, sizeof(pci_name), pdev);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/physfn", pci_name);
	if (realpath(path, cpath) == NULL)
		return rc;

	n = scandir(cpath, &namelist, virtfn_filter, versionsort);
	if (n < 0)
		return rc;
	else {
		while (n--) {
			if (rc)
				rc = _read_virtfn_index(index, cpath, namelist[n]->d_name, pci_name);
			free(namelist[n]);
		}
		free(namelist);
	}

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

static struct pci_dev * find_pdev_by_pci_name(struct pci_access *pacc, const char *s)
{
	int domain=0, bus=0, device=0, func=0;
	if (parse_pci_name(s, &domain, &bus, &device, &func))
		return NULL;
	return pci_get_dev(pacc, domain, bus, device, func);
}

static struct pci_device *
find_physfn(struct libbiosdevname_state *state, struct pci_device *dev)
{
	int rc;
	char path[PATH_MAX];
	char *c;
	struct pci_dev *pdev;
	memset(path, 0, sizeof(path));
	rc = read_pci_sysfs_physfn(path, sizeof(path), dev->pci_dev);
	if (rc != 0)
		return NULL;
	/* we get back a string like
	   ../0000:05:0.0
	   where the last component is the parent device
	*/
	/* find the last backslash */
	c = rindex(path, '/');
	c++;
	pdev = find_pdev_by_pci_name(state->pacc, c);
	dev = find_dev_by_pci(state, pdev);
	return dev;
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

static void try_add_vf_to_pf(struct libbiosdevname_state *state, struct pci_device *vf)
{
	struct pci_device *pf;
	unsigned int index=0;
	int rc;
	pf = find_physfn(state, vf);

	if (!pf)
		return;
	list_add_tail(&vf->vfnode, &pf->vfs);
	rc = read_virtfn_index(&index, vf->pci_dev);
	if (!rc) {
		vf->vf_index = index;
		pf->is_sriov_physical_function = 1;
	}
	vf->pf = pf;
	vf->physical_slot = pf->physical_slot;
}

static struct pci_device *
find_parent(struct libbiosdevname_state *state, struct pci_device *dev)
{
	int rc;
	char path[PATH_MAX];
	char *c;
	struct pci_device *physfn;
	struct pci_dev *pdev;
	memset(path, 0, sizeof(path));
	/* if this device has a physfn pointer, then treat _that_ as the parent */
	physfn = find_physfn(state, dev);
	if (physfn) {
		dev->is_sriov_virtual_function=1;
		return physfn;
	}

	rc = read_pci_sysfs_path(path, sizeof(path), dev->pci_dev);
	if (rc != 0)
		return NULL;
	/* we get back a string like
	   ../../../devices/pci0000:00/0000:00:09.0/0000:05:17.4
	   where the last component is the device we asked for
	*/
	/* find the last backslash */
	c = rindex(path, '/');
	*c = '\0';
	/* find the last backslash again */
	c = rindex(path, '/');
	c++;
	pdev = find_pdev_by_pci_name(state->pacc, c);
	if (pdev) {
		dev = find_dev_by_pci(state, pdev);
		return dev;
	}
	return NULL;
}

/*
 * Check our parents in case the device itself isn't listed
 * in the SMBIOS table.  This has a problem, as
 * our parent bridge on a card may not be included
 * in the SMBIOS table.  In that case, it falls back to "unknown".
 */
static int pci_dev_to_slot(struct libbiosdevname_state *state, struct pci_device *dev)
{
	struct pci_device *d = dev;
	int slot = d->physical_slot;
	while (d && slot == PHYSICAL_SLOT_UNKNOWN) {
		d = find_parent(state, d);
		if (d)
			slot = d->physical_slot;
	}
	return slot;
}

static int pirq_dev_to_slot(struct libbiosdevname_state *state, struct pci_device *dev)
{
	struct pci_device *d = dev;
	int slot = pirq_pci_dev_to_slot(state->pirq_table, d->pci_dev->bus, d->pci_dev->dev);
	while (d && slot == PHYSICAL_SLOT_UNKNOWN) {
		d = find_parent(state, d);
		if (d)
			slot = pirq_pci_dev_to_slot(state->pirq_table, d->pci_dev->bus, d->pci_dev->dev);
	}
	return slot;
}

static void dev_to_slot(struct libbiosdevname_state *state, struct pci_device *dev)
{
	int slot;
	slot = pci_dev_to_slot(state, dev);
	if (slot == PHYSICAL_SLOT_UNKNOWN)
		slot = pirq_dev_to_slot(state, dev);
	dev->physical_slot = slot;
}

static char *read_pci_sysfs_label(const struct pci_dev *pdev)
{
	char path[PATH_MAX];
	char pci_name[16];
	int rc;
	char *label = NULL;

	unparse_pci_name(pci_name, sizeof(pci_name), pdev);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/label", pci_name);
	rc = sysfs_read_file(path, &label);
	if (rc == 0)
		return label;
	return NULL;
}

static int read_pci_sysfs_index(unsigned int *index, const struct pci_dev *pdev)
{
	char path[PATH_MAX];
	char pci_name[16];
	int rc;
	char *indexstr = NULL;
	unsigned int i;
	unparse_pci_name(pci_name, sizeof(pci_name), pdev);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/index", pci_name);
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

static void fill_pci_dev_sysfs(struct pci_device *dev, struct pci_dev *p)
{
	int rc;
	unsigned int index = 0;
	char *label = NULL;
	char buf[PATH_MAX];
	unparse_pci_name(buf, sizeof(buf), p);
	rc = read_pci_sysfs_index(&index, p);
	if (!rc) {
		dev->sysfs_index = index;
		dev->uses_sysfs |= HAS_SYSFS_INDEX;
	}
	label = read_pci_sysfs_label(p);
	if (label) {
		dev->sysfs_label = label;
		dev->uses_sysfs |= HAS_SYSFS_LABEL;
	}
}

static void add_pci_dev(struct libbiosdevname_state *state,
			struct pci_dev *p)
{
	struct pci_device *dev;
	dev = malloc(sizeof(*dev));
	if (!dev) {
		fprintf(stderr, "out of memory\n");
		return;
	}
	memset(dev, 0, sizeof(*dev));
	INIT_LIST_HEAD(&dev->node);
	INIT_LIST_HEAD(&dev->vfnode);
	INIT_LIST_HEAD(&dev->vfs);
	dev->pci_dev = p;
	dev->physical_slot = PHYSICAL_SLOT_UNKNOWN;
	dev->class         = pci_read_word(p, PCI_CLASS_DEVICE);
	dev->vf_index = INT_MAX;
	fill_pci_dev_sysfs(dev, p);
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

static void set_pci_slots(struct libbiosdevname_state *state)
{
	struct pci_device *dev;

	list_for_each_entry(dev, &state->pci_devices, node) {
		dev_to_slot(state, dev);
	}
}


static int set_pci_slot_index(struct libbiosdevname_state *state)
{
	struct pci_device *pcidev;
	int prevslot=-1;
	int index=0;

	/* only iterate over the PCI devices, because the bios_device list may be incomplete due to renames happening in parallel */
	list_for_each_entry(pcidev, &state->pci_devices, node) {
		if (pcidev->physical_slot == 0) /* skip embedded devices */
			continue;
		if (is_pci_bridge(pcidev))
			continue;
		if (pcidev->is_sriov_virtual_function)
			continue;
		if (pcidev->physical_slot != prevslot) {
			index=0;
			prevslot = pcidev->physical_slot;
		}
		else
			index++;
		pcidev->index_in_slot = index;
	}
	return 0;
}

static void set_sriov_pf_vf(struct libbiosdevname_state *state)
{
	struct pci_device *vf;
	list_for_each_entry(vf, &state->pci_devices, node) {
		if (!vf->is_sriov_virtual_function)
			continue;
		try_add_vf_to_pf(state, vf);
	}
}

int get_pci_devices(struct libbiosdevname_state *state)
{
	struct pci_access *pacc;
	struct pci_dev *p;
	struct routing_table *table;
	int rc=0;

	table = pirq_alloc_read_table();
	if (table)
		state->pirq_table = table;

	pacc = pci_alloc();
	if (!pacc)
		return rc;
	state->pacc = pacc;
	pci_init(pacc);
	pci_scan_bus(pacc);

	for (p=pacc->devices; p; p=p->next) {
		add_pci_dev(state, p);
	}
	/* ordering here is important */
	dmidecode_main(state);	/* this will fail on Xen guests, that's OK */
	set_pci_slots(state);
	set_pci_slot_index(state);
	set_sriov_pf_vf(state);

	return rc;
}

int unparse_pci_name(char *buf, int size, const struct pci_dev *pdev)
{
	return snprintf(buf, size, "%04x:%02x:%02x.%x",
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
	struct pci_device *dev;
	char pci_name[16];
	s += snprintf(s, size-(s-buf), "PCI name      : ");
	s += unparse_pci_name(s,  size-(s-buf), p->pci_dev);
	s += snprintf(s, size-(s-buf), "\n");
	s += snprintf(s, size-(s-buf), "PCI Slot      : ");
	if (p->physical_slot < INT_MAX)
		s += unparse_location(s, size-(s-buf), p->physical_slot);
	else
		s += snprintf(s, size-(s-buf), "Unknown");
	s += snprintf(s, size-(s-buf), "\n");
	if (p->smbios_type) {
		s += snprintf(s, size-(s-buf), "SMBIOS Device Type: ");
		s += unparse_smbios_type41_type(s, size-(s-buf), p->smbios_type);
		s += snprintf(s, size-(s-buf), "SMBIOS Instance: %u\n", p->smbios_instance);
		s += snprintf(s, size-(s-buf), "SMBIOS Enabled: %s\n", p->smbios_instance?"True":"False");
	}
	if (p->uses_smbios & HAS_SMBIOS_LABEL && p->smbios_label)
		s += snprintf(s, size-(s-buf), "SMBIOS Label: %s\n", p->smbios_label);
	if (p->uses_sysfs & HAS_SYSFS_INDEX)
		s += snprintf(s, size-(s-buf), "sysfs Index: %u\n", p->sysfs_index);
	if (p->uses_sysfs & HAS_SYSFS_LABEL)
		s += snprintf(s, size-(s-buf), "sysfs Label: %s\n", p->sysfs_label);
	if (p->physical_slot > 0 && !p->is_sriov_virtual_function)
		s += snprintf(s, size-(s-buf), "Index in slot: %u\n", p->index_in_slot);

	if (!list_empty(&p->vfs)) {
		s += snprintf(s, size-(s-buf), "Virtual Functions:\n");
		list_for_each_entry(dev, &p->vfs, vfnode) {
			unparse_pci_name(pci_name, sizeof(pci_name), dev->pci_dev);
			s += snprintf(s, size-(s-buf), "%s\n", pci_name);
		}
	}

	return (s-buf);
}

struct pci_device * find_dev_by_pci(const struct libbiosdevname_state *state,
				    const struct pci_dev *p)
{
	struct pci_device *dev;
	list_for_each_entry(dev, &state->pci_devices, node) {
		if (is_same_pci(p, dev->pci_dev))
			return dev;
	}
	return NULL;
}

struct pci_device * find_pci_dev_by_pci_addr(const struct libbiosdevname_state *state,
					     const int domain, const int bus, const int device, const int func)
{
	struct pci_device *dev;
	struct pci_dev p;
	memset(&p, 0, sizeof(p));

#ifdef HAVE_STRUCT_PCI_DEV_DOMAIN
	p.domain = domain;
#endif
	p.bus = bus;
	p.dev = device;
	p.func = func;

	list_for_each_entry(dev, &state->pci_devices, node) {
		if (is_same_pci(&p, dev->pci_dev))
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
