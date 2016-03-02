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
#include <fcntl.h>
#include "pci.h"
#include "sysfs.h"
#include "dmidecode/dmidecode.h"
#include "pirq.h"

extern int is_valid_smbios;

#ifndef PCI_CB_CAPABILITY_LIST
#define PCI_CB_CAPABILITY_LIST	0x14
#endif

/* Borrowed from kernel vpd code */
#define PCI_VPD_LRDT			0x80
#define PCI_VPD_SRDT_END		0x78
#define PCI_VPDI_TAG			0x82
#define PCI_VPDR_TAG			0x90

#define PCI_VPD_SRDT_LEN_MASK		0x7
#define PCI_VPD_LRDT_TAG_SIZE		3
#define PCI_VPD_SRDT_TAG_SIZE		1
#define PCI_VPD_INFO_FLD_HDR_SIZE	3

struct vpd_tag
{
	char	cc[2];
	u8	len;
	char	data[1];
};

static inline u16 pci_vpd_lrdt_size(const u8 *lrdt)
{
	return (u16)lrdt[0] + ((u16)lrdt[1] << 8L);
}

static inline u8 pci_vpd_srdt_size(const u8* srdt)
{
	return (*srdt) & PCI_VPD_SRDT_LEN_MASK;
}

static int pci_vpd_readtag(int fd, int *len)
{
	u8 tag, tlen[2];

	if (read(fd, &tag, 1) != 1)
		return -1;
	if (tag == 0x00 || tag == 0xFF || tag == 0x7F)
		return -1;
	if (tag & PCI_VPD_LRDT) {
		if (read(fd, tlen, 2) != 2)
			return -1;
		*len = pci_vpd_lrdt_size(tlen);
		/* Check length of VPD-R */
		if (*len  >= 1024)
			return -1;
		return tag;
	}
	*len = pci_vpd_srdt_size(&tag);
	return (tag & ~0x7);
}

static void *pci_vpd_findtag(void *buf, int len, const char *sig)
{
        int off, siglen;
        struct vpd_tag *t;

        off = 0;
        siglen = strlen(sig);
        while (off < len) {
                t = (struct vpd_tag *)((u8 *)buf + off);
                if (!memcmp(t->data, sig, siglen))
                        return t;
                off += (t->len + 3);
        }
        return NULL;
}

/* Add port identifier(s) to PCI device */
static void add_port(struct pci_device *pdev, int port, int pfi)
{
	struct pci_port *p;

	list_for_each_entry(p, &pdev->ports, node) {
		if (p->port == port && p->pfi == pfi)
			return;
	}
	p = malloc(sizeof(*p));
	if (p == NULL)
		return;
	memset(p, 0, sizeof(*p));
	INIT_LIST_HEAD(&p->node);
	p->port = port;
	p->pfi = pfi;
	list_add_tail(&p->node, &pdev->ports);
}

static void parse_dcm(struct libbiosdevname_state *state, struct pci_device *pdev,
		      void *vpd, int len)
{
	int i, port, devfn, pfi, step;
	struct pci_device *vf;
	struct vpd_tag *dcm;
	const char *fmt;

	fmt = "%1x%1x%2x";
	step = 10;
	dcm = pci_vpd_findtag(vpd, len, "DCM");
	if (dcm == NULL) {
		dcm = pci_vpd_findtag(vpd, len, "DC2");
		if (dcm == NULL)
			return;
		fmt = "%1x%2x%2x";
		step = 11;
	}
	for (i = 3; i < dcm->len; i += step) {
		if (i+step > dcm->len) {
			/* DCM is truncated */
			return;
		}
		if (sscanf(dcm->data+i, fmt, &port, &devfn, &pfi) != 3)
			break;
		vf = find_pci_dev_by_pci_addr(state, pdev->pci_dev->domain,
					      pdev->pci_dev->bus,
					      devfn >> 3, devfn & 7);
		if (vf != NULL) {
			add_port(vf, port, pfi);
			if (vf->vpd_port == INT_MAX) {
				vf->vpd_port = port;
				vf->vpd_pfi = pfi;
			}
		}
	}
}

/* Read and parse PCI VPD section if it exists */
static int read_pci_vpd(struct libbiosdevname_state *state, struct pci_device *pdev)
{
	char path[PATH_MAX];
	char pci_name[16];
	int fd, len;
	unsigned char *vpd;

	if (!is_pci_network(pdev))
		return 1;
	unparse_pci_name(pci_name, sizeof(pci_name), pdev->pci_dev);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/physfn/vpd", pci_name);
	fd = open(path, O_RDONLY|O_SYNC);
	if (fd < 0) {
		snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vpd", pci_name);
		fd = open(path, O_RDONLY|O_SYNC);
		if (fd < 0)
			return 1;
	}
	if (pci_vpd_readtag(fd, &len) != PCI_VPDI_TAG)
		goto done;
	lseek(fd, len, SEEK_CUR);
	if (pci_vpd_readtag(fd, &len) != PCI_VPDR_TAG)
		goto done;
	vpd = alloca(len);
	if (read(fd, vpd, len) != len)
		goto done;
	/* Check for DELL VPD tag */
	if (!pci_vpd_findtag(vpd, len, "DSV1028VPDR.VER"))
		goto done;
	parse_dcm(state, pdev, vpd, len);
 done:
	close(fd);
	return 0;
}

static void set_pci_vpd_instance(struct libbiosdevname_state *state)
{
	struct pci_device *dev, *dev2;
	int fd;
	char sys_vendor[10] = {0};

	/* Read VPD-R on Dell systems only */
	if ((fd = open("/sys/devices/virtual/dmi/id/sys_vendor", O_RDONLY)) >= 0) {
		if (read(fd, sys_vendor, 9) != 9) {
			close(fd);
			return;
		}
		if (strncmp(sys_vendor, "Dell Inc.", 9)) {
			close(fd);
			return;
		}
	} else
		return;

	/* Read VPD information for each device */
	list_for_each_entry(dev, &state->pci_devices, node) {
		/* RedHat bugzilla 801885, 789635, 781572 */
		if (dev->pci_dev->vendor_id == 0x1969 ||
		    dev->pci_dev->vendor_id == 0x168c)
			continue;
		if (dev->vpd_port != INT_MAX) {
			/* Ignore already parsed devices */
			continue;
		}
		read_pci_vpd(state, dev);
	}

	/* Now match VPD master device */
	list_for_each_entry(dev, &state->pci_devices, node) {
		if (dev->vpd_port == INT_MAX)
			continue;
		list_for_each_entry(dev2, &state->pci_devices, node) {
			if (dev2->pci_dev->domain == dev->pci_dev->domain &&
			    dev2->pci_dev->bus == dev->pci_dev->bus &&
			    dev2->vpd_port == dev->vpd_port) {
				dev2->vpd_count++;
				dev->vpd_pf = dev2;
				if (dev2->physical_slot == 0)
					dev->physical_slot = 0;
				break;
			}
		}
	}

	/* Delete all VPD devices with single function */
	list_for_each_entry(dev, &state->pci_devices, node) {
		if (dev->vpd_count == 1) {
			dev->vpd_port = INT_MAX;
			dev->vpd_pfi = INT_MAX;
			dev->vpd_pf = NULL;
		}
	}
	close(fd);
}

static int pci_find_capability(struct pci_dev *p, int cap)
{
	u16 status;
	u8 hdr, id;
	int pos, ttl = 48;

	status = pci_read_word(p, PCI_STATUS);
	if (!(status & PCI_STATUS_CAP_LIST))
		return 0;
	hdr = pci_read_byte(p, PCI_HEADER_TYPE);
	switch(hdr & 0x7F) {
	case PCI_HEADER_TYPE_NORMAL:
	case PCI_HEADER_TYPE_BRIDGE:
		pos = PCI_CAPABILITY_LIST;
		break;
	case PCI_HEADER_TYPE_CARDBUS:
		pos = PCI_CB_CAPABILITY_LIST;
		break;
	default:
		return 0;
	}

	while (ttl--) {
		pos = pci_read_byte(p, pos);
		if (pos < 0x40)
			break;
		pos &= ~3;
		id = pci_read_byte(p, pos+PCI_CAP_LIST_ID);
		if (id == 0xFF)
			break;
		if (id == cap)
			return pos;
		pos += PCI_CAP_LIST_NEXT;
	}
	return 0;
}

static struct pci_device *
find_parent(struct libbiosdevname_state *state, struct pci_device *dev);

static int pcie_get_slot(struct libbiosdevname_state *state, struct pci_device *p)
{
	int pos;
	u32 slot, flag;

	while (p) {
		/* Return PCIE physical slot number */
		if ((pos = pci_find_capability(p->pci_dev, PCI_CAP_ID_EXP)) != 0) {
			flag = pci_read_word(p->pci_dev, pos + PCI_EXP_FLAGS);
			slot = (pci_read_long(p->pci_dev, pos + PCI_EXP_SLTCAP) >> 19);
			if ((flag & PCI_EXP_FLAGS_SLOT) && slot)
				return slot;
		}
		p = find_parent(state, p);
	}
	return PHYSICAL_SLOT_UNKNOWN;
}

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

static int parse_pci_name(const char *s, int *domain, int *bus, int *dev, int *func)
{
	int err;
	const char *r;

	/* Allow parsing pathnames */
	if ((r = strrchr(s, '/')) != NULL)
		s = r+1;

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

static int is_same_pci(const struct pci_dev *a, const struct pci_dev *b)
{
	if (pci_domain_nr(a) == pci_domain_nr(b) &&
	    a->bus == b->bus &&
	    a->dev == b->dev &&
	    a->func == b->func)
		return 1;
	return 0;
}

static struct pci_device *
find_parent(struct libbiosdevname_state *state, struct pci_device *dev)
{
	int rc;
	char path[PATH_MAX];
	char *c;
	struct pci_dev *pdev;
	memset(path, 0, sizeof(path));

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
 * in the SMBIOS table.	 This has a problem, as
 * our parent bridge on a card may not be included
 * in the SMBIOS table.	 In that case, it falls back to "unknown".
 */
static inline int pci_dev_to_slot(struct libbiosdevname_state *state, struct pci_device *dev)
{
	return dev->physical_slot;
}

static inline int pirq_dev_to_slot(struct libbiosdevname_state *state, struct pci_device *dev)
{
	return pirq_pci_dev_to_slot(state->pirq_table, pci_domain_nr(dev->pci_dev), dev->pci_dev->bus, dev->pci_dev->dev);
}

static void dev_to_slot(struct libbiosdevname_state *state, struct pci_device *dev)
{
	struct pci_device *d = dev;
	int slot;
	do {
		slot = pci_dev_to_slot(state, d);
		if (slot == PHYSICAL_SLOT_UNKNOWN && is_valid_smbios)
			slot = pcie_get_slot(state, d);
		if (slot == PHYSICAL_SLOT_UNKNOWN)
			slot = pirq_dev_to_slot(state, d);
		if (slot == PHYSICAL_SLOT_UNKNOWN)
			d = find_parent(state, d);
	} while (d && slot == PHYSICAL_SLOT_UNKNOWN);

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
		free(indexstr);
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
	uint8_t hdr;
	dev = malloc(sizeof(*dev));
	if (!dev) {
		fprintf(stderr, "out of memory\n");
		return;
	}
	memset(dev, 0, sizeof(*dev));
	INIT_LIST_HEAD(&dev->node);
	INIT_LIST_HEAD(&dev->vfnode);
	INIT_LIST_HEAD(&dev->vfs);
	INIT_LIST_HEAD(&dev->ports);
	dev->pci_dev = p;
	dev->physical_slot = PHYSICAL_SLOT_UNKNOWN;
	dev->class	   = pci_read_word(p, PCI_CLASS_DEVICE);
	dev->vf_index = INT_MAX;
	dev->vpd_port = INT_MAX;
	dev->vpd_pfi  = INT_MAX;
	dev->vpd_pf = NULL;
	fill_pci_dev_sysfs(dev, p);
	list_add(&dev->node, &state->pci_devices);

	/* Get subordinate bus if this is a bridge */
	hdr = pci_read_byte(p, PCI_HEADER_TYPE);
	switch (hdr & 0x7F) {
	case PCI_HEADER_TYPE_BRIDGE:
	case PCI_HEADER_TYPE_CARDBUS:
		dev->sbus = pci_read_byte(p, PCI_SECONDARY_BUS);
		break;
	default:
		dev->sbus = -1;
		break;
	}
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
	int index=1;

	/* only iterate over the PCI devices, because the bios_device list may be incomplete due to renames happening in parallel */
	list_for_each_entry(pcidev, &state->pci_devices, node) {
		if (pcidev->physical_slot == 0) /* skip embedded devices */
			continue;
		if (!is_pci_network(pcidev)) /* only look at PCI network devices */
			continue;
		if (pcidev->is_sriov_virtual_function) /* skip sriov VFs, they're handled later */
			continue;
		if (pcidev->physical_slot != prevslot) {
			index=1;
			prevslot = pcidev->physical_slot;
		}
		else
			index++;
		pcidev->index_in_slot = index;
	}
	return 0;
}

static int set_embedded_index(struct libbiosdevname_state *state)
{
	struct pci_device *pcidev;
	int index=1;

	list_for_each_entry(pcidev, &state->pci_devices, node) {
		if (pcidev->physical_slot != 0) /* skip non-embedded devices */
			continue;
		if (!is_pci_network(pcidev)) /* only look at PCI network devices */
			continue;
		if (pcidev->is_sriov_virtual_function) /* skip sriov VFs, they're handled later */
			continue;
		if (pcidev->vpd_port != INT_MAX)
			continue;
		pcidev->embedded_index = index;
		pcidev->embedded_index_valid = 1;
		index++;
	}
	return 0;
}

static int virtfn_filter(const struct dirent *dent)
{
	return (!strncmp(dent->d_name,"virtfn",6));
}

/* Assign Virtual Function to Physical Function */
static void set_sriov(struct libbiosdevname_state *state, struct pci_device *pf, const char *virtpath)
{
	struct pci_device *vf;
	char pci_name[32];
	char path[PATH_MAX], cpath[PATH_MAX];
	unsigned vf_index;

	if (sscanf(virtpath, "virtfn%u", &vf_index) != 1)
		return;
	unparse_pci_name(pci_name, sizeof(pci_name), pf->pci_dev);
	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/%s", pci_name, virtpath);

	memset(cpath, 0, sizeof(cpath));
	if (readlink(path, cpath, sizeof(cpath) - 1) < 0)
		return;
	if ((vf = find_dev_by_pci_name(state, cpath)) != NULL) {
		vf->is_sriov_virtual_function = 1;
		vf->vf_index = vf_index;
		vf->pf = pf;
		pf->is_sriov_physical_function = 1;
		if (pf->smbios_enabled) {
			vf->smbios_instance = pf->smbios_instance;
			vf->physical_slot = pf->physical_slot;
		}
		list_add_tail(&vf->vfnode, &pf->vfs);
	}
}

static void scan_sriov(struct libbiosdevname_state *state)
{
	struct pci_device *pf;
	char path[PATH_MAX];
	char pci_name[32];
	struct dirent **namelist;
	int n;

	list_for_each_entry(pf, &state->pci_devices, node) {
		unparse_pci_name(pci_name, sizeof(pci_name), pf->pci_dev);
		snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s", pci_name);

		namelist = NULL;
		n = scandir(path, &namelist, virtfn_filter, versionsort);
		if (n <= 0)
			continue;
		while (n--) {
			set_sriov(state, pf, namelist[n]->d_name);
			free(namelist[n]);
		}
		free(namelist);
	}
}

/*
 * This sorts the PCI devices by breadth-first domain/bus/dev/fn.
 */
static int sort_pci(const struct pci_device *a, const struct pci_device *b)
{

	if	(pci_domain_nr(a->pci_dev) < pci_domain_nr(b->pci_dev)) return -1;
	else if (pci_domain_nr(a->pci_dev) > pci_domain_nr(b->pci_dev)) return	1;

	if	(a->pci_dev->bus < b->pci_dev->bus) return -1;
	else if (a->pci_dev->bus > b->pci_dev->bus) return  1;

	if	(a->pci_dev->dev < b->pci_dev->dev) return -1;
	else if (a->pci_dev->dev > b->pci_dev->dev) return  1;

	if	(a->pci_dev->func < b->pci_dev->func) return -1;
	else if (a->pci_dev->func > b->pci_dev->func) return  1;

	return 0;
}

static void insertion_sort_devices(struct pci_device *a, struct list_head *list,
				   int (*cmp)(const struct pci_device *, const struct pci_device *))
{
	struct pci_device *b;
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
	struct pci_device *dev, *tmp;
	list_for_each_entry_safe(dev, tmp, &state->pci_devices, node) {
		insertion_sort_devices(dev, &sorted_devices, sort_pci);
	}
	list_splice(&sorted_devices, &state->pci_devices);
}

int get_pci_devices(struct libbiosdevname_state *state)
{
	struct pci_access *pacc;
	struct pci_dev *p;
	struct routing_table *table;

	table = pirq_alloc_read_table();
	if (table)
		state->pirq_table = table;

	pacc = pci_alloc();
	if (!pacc)
		return 0;
#if 0
	pci_set_param(pacc, "dump.name", "lspci.txt");
	pacc->method = PCI_ACCESS_DUMP;
#endif
	state->pacc = pacc;
	pci_init(pacc);
	pci_scan_bus(pacc);

	for (p=pacc->devices; p; p=p->next) {
		add_pci_dev(state, p);
	}
	/* ordering here is important */
	dmidecode_main(state);	/* this will fail on Xen guests, that's OK */
	sort_device_list(state);
	scan_sriov(state);
	set_pci_vpd_instance(state);
	set_pci_slots(state);
	set_embedded_index(state);
	set_pci_slot_index(state);

	return 0;
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
	if (type > 0 && type <= (sizeof(msg)/sizeof(msg[0])))
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
		if (p->smbios_instance)
			s += snprintf(s, size-(s-buf), "SMBIOS Instance: %u\n", p->smbios_instance);
	}
	if (p->uses_smbios & HAS_SMBIOS_LABEL && p->smbios_label)
		s += snprintf(s, size-(s-buf), "SMBIOS Label: %s\n", p->smbios_label);
	if (p->uses_sysfs & HAS_SYSFS_INDEX)
		s += snprintf(s, size-(s-buf), "sysfs Index: %u\n", p->sysfs_index);
	if (p->uses_sysfs & HAS_SYSFS_LABEL)
		s += snprintf(s, size-(s-buf), "sysfs Label: %s\n", p->sysfs_label);
	if (p->physical_slot > 0 && !p->is_sriov_virtual_function)
		s += snprintf(s, size-(s-buf), "Index in slot: %u\n", p->index_in_slot);
	if (p->embedded_index_valid)
		s += snprintf(s, size-(s-buf), "Embedded Index: %u\n", p->embedded_index);
	if (p->vpd_port < INT_MAX) {
		s += snprintf(s, size-(s-buf), "VPD Port: %u\n", p->vpd_port);
		s += snprintf(s, size-(s-buf), "VPD Index: %u\n", p->vpd_pfi);
		if (p->vpd_pf) {
			s += snprintf(s, size-(s-buf), "VPD PCI master: ");
			s += unparse_pci_name(s, size-(s-buf), p->vpd_pf->pci_dev);
			s += snprintf(s, size-(s-buf), " count %d\n", p->vpd_pf->vpd_count);
		}
	}
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

int is_root_port(const struct libbiosdevname_state *state,
		int domain, int bus, int device, int func)
{
       struct pci_device *pdev;
       int pos;
       u16 flag;

       pdev = find_pci_dev_by_pci_addr(state, domain, bus, device, func);

       if (!pdev || !pdev->pci_dev)
	       return 0;

       pos = pci_find_capability(pdev->pci_dev, PCI_CAP_ID_EXP);
       if (pos != 0) {
	       u8 type;

	       flag = pci_read_word(pdev->pci_dev, pos + PCI_EXP_FLAGS);

	       type = (flag & PCI_EXP_FLAGS_TYPE) >> 4;

	       if (type == PCI_EXP_TYPE_ROOT_PORT)
		       return 1;
       }

       return 0;
}

