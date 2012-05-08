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
#define PCI_CB_CAPABILITY_LIST  0x14
#endif

/* Borrowed from kernel vpd code */
#define PCI_VPD_LRDT 			0x80
#define PCI_VPD_SRDT_END 		0x78

#define PCI_VPD_SRDT_LEN_MASK		0x7
#define PCI_VPD_LRDT_TAG_SIZE		3
#define PCI_VPD_SRDT_TAG_SIZE		1
#define PCI_VPD_INFO_FLD_HDR_SIZE	3

static inline u16 pci_vpd_lrdt_size(const u8 *lrdt)
{
	return (u16)lrdt[1] + ((u16)lrdt[2] << 8L);
}

static inline u8 pci_vpd_srdt_size(const u8* srdt)
{
	return (*srdt) & PCI_VPD_SRDT_LEN_MASK;
}

static inline u8 pci_vpd_info_field_size(const u8 *info_field)
{
	return info_field[2];
}

static int pci_vpd_find_tag(const u8 *buf, unsigned int off, unsigned int len, u8 rdt)
{
	int i;

	for (i = off; i < len;) {
		u8 val = buf[i];

		if (val & PCI_VPD_LRDT) {
			if (i + PCI_VPD_LRDT_TAG_SIZE > len)
				break;
			if (val == rdt)
				return i;
			i += PCI_VPD_LRDT_TAG_SIZE + pci_vpd_lrdt_size(&buf[i]);
		} else {
			u8 tag = val & ~PCI_VPD_SRDT_LEN_MASK;
			
			if (tag == rdt)
				return i;
			if (tag == PCI_VPD_SRDT_END)
				break;
			i += PCI_VPD_SRDT_TAG_SIZE + pci_vpd_srdt_size(&buf[i]);
		}
	}
	return -1;
}

/* Search for matching key/subkey in the VPD data */
static int pci_vpd_find_info_subkey(const u8 *buf, unsigned int off, unsigned int len, 
	const char *kw, const char *skw)
{
	int i;

	for (i = off; i + PCI_VPD_INFO_FLD_HDR_SIZE <= off+len;) {
		/* Match key and subkey names, can use * for regex */
		if ((kw[0] == '*' || buf[i+0] == kw[0]) &&
		    (kw[1] == '*' || buf[i+1] == kw[1]) &&
		    (skw[0] == '*' || !memcmp(&buf[i+3], skw, 3)))
			return i;
		i += PCI_VPD_INFO_FLD_HDR_SIZE + pci_vpd_info_field_size(&buf[i]);
	}
	return -1;
}

static int parse_vpd(struct libbiosdevname_state *state, struct pci_device *pdev, int len, unsigned char *vpd)
{
	int i, j, k, isz, jsz, port, func, pfi;
	struct pci_device *vf;

	i = pci_vpd_find_tag(vpd, 0, len, 0x90);
	if (i < 0)
		return 1;
	isz = pci_vpd_lrdt_size(&vpd[i]);
	i += PCI_VPD_LRDT_TAG_SIZE;

	/* Lookup Version */
	j = pci_vpd_find_info_subkey(vpd, i, isz, "**", "DSV");
	if (j < 0)
		return 1;
	jsz = pci_vpd_info_field_size(&vpd[j]);
	j += PCI_VPD_INFO_FLD_HDR_SIZE;
	if (memcmp(vpd+j+3, "1028VPDR.VER1.0", 15))
		return 1;
	
	/* Lookup Port Mappings */
	j = pci_vpd_find_info_subkey(vpd, i, isz, "**", "DCM");
	if (j < 0)
		return 1;
	jsz = pci_vpd_info_field_size(&vpd[j]);
	j += PCI_VPD_INFO_FLD_HDR_SIZE;

	for (k=3; k<jsz; k+=10) {
		/* Parse Port Info */
		sscanf((char *)vpd+j+k, "%1x%1x%2x", &port, &func, &pfi);
		if ((vf = find_pci_dev_by_pci_addr(state, pdev->pci_dev->domain,
						   pdev->pci_dev->bus,
						   pdev->pci_dev->dev,
						   func)) != NULL) {
			if (vf->vpd_port == INT_MAX) {
				vf->vpd_port = port;
				vf->vpd_pfi = pfi;
			}
		}
	}
	return 0;
}

/* Read and parse PCI VPD section if it exists */
static int read_pci_vpd(struct libbiosdevname_state *state, struct pci_device *pdev)
{
	uint8_t buf[3], tag, len;
	unsigned char *vpd;
	off_t off = 0;

	for(;;) {
		if (!pci_read_vpd(pdev->pci_dev, off, buf, 1))
			break;
		if (buf[0] & PCI_VPD_LRDT) {
			if (!pci_read_vpd(pdev->pci_dev, off+1, buf+1, 2))
				break;
			tag = buf[0];
			len = pci_vpd_lrdt_size(buf) + PCI_VPD_LRDT_TAG_SIZE;
		} else {
			tag = buf[0] & ~PCI_VPD_SRDT_LEN_MASK;
			len = pci_vpd_srdt_size(buf) + PCI_VPD_SRDT_TAG_SIZE;
		}
		if (tag == 0 || tag == 0xFF || tag == PCI_VPD_SRDT_END)
			break;

		printf("found tag: %x\n", tag);
		vpd = malloc(len);
		if (!pci_read_vpd(pdev->pci_dev, off, vpd, len)) {
			free(vpd);
			break;
		}
		parse_vpd(state, pdev, len, vpd);
		free(vpd);
	
		off += len;
	}
	return 0;
}

static void set_pci_vpd_instance(struct libbiosdevname_state *state)
{
	struct pci_device *dev, *dev2;
	int fd;
	char sys_vendor[10] = {0};

	/* Read VPD-R on Dell systems only */
	if ((fd = open("/sys/devices/virtual/dmi/id/sys_vendor", O_RDONLY)) >= 0) {
		if (read(fd, sys_vendor, 9) != 9)
			return;
		if (strncmp(sys_vendor, "Dell Inc.", 9)) 
			return;
	} else
		return;

	/* Read VPD information for each device */
	list_for_each_entry(dev, &state->pci_devices, node) {
		/* RedHat bugzilla 801885, 789635, 781572 */
		if (dev->pci_dev->vendor_id == 0x1969 ||
		    dev->pci_dev->vendor_id == 0x168c)
			continue;
		read_pci_vpd(state, dev);
	}

	/* Now match VPD master device */
	list_for_each_entry(dev, &state->pci_devices, node) {
		if (dev->vpd_port == INT_MAX)
			continue;
		list_for_each_entry(dev2, &state->pci_devices, node) {
			if (dev2->pci_dev->domain == dev->pci_dev->domain &&
			    dev2->pci_dev->bus == dev->pci_dev->bus &&
			    dev2->pci_dev->dev == dev->pci_dev->dev &&
			    dev2->vpd_port == dev->vpd_port) {
			  	dev2->vpd_count++;
				dev->vpd_pf = dev2;
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

static int pcie_get_slot(struct pci_dev *p)
{
  	int pos;
	u32 slot, flag;

	/* Return PCIE physical slot number */
	if ((pos = pci_find_capability(p, PCI_CAP_ID_EXP)) != 0) {
	  	flag = pci_read_word(p, pos + PCI_EXP_FLAGS);
		slot = (pci_read_long(p, pos + PCI_EXP_SLTCAP) >> 19);
		if ((flag & PCI_EXP_FLAGS_SLOT) && slot)
			return slot;
	}
	return PHYSICAL_SLOT_UNKNOWN;
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

static int is_same_pci(const struct pci_dev *a, const struct pci_dev *b)
{
	if (pci_domain_nr(a) == pci_domain_nr(b) &&
	    a->bus == b->bus &&
	    a->dev == b->dev &&
	    a->func == b->func)
		return 1;
	return 0;
}

/*
 * Check our parents in case the device itself isn't listed
 * in the SMBIOS table.  This has a problem, as
 * our parent bridge on a card may not be included
 * in the SMBIOS table.  In that case, it falls back to "unknown".
 */
static inline int pci_dev_to_slot(struct libbiosdevname_state *state, struct pci_device *dev)
{
	return dev->physical_slot;
}

static inline int pirq_dev_to_slot(struct libbiosdevname_state *state, struct pci_device *dev)
{
	return pirq_pci_dev_to_slot(state->pirq_table, pci_domain_nr(dev->pci_dev), dev->pci_dev->bus, dev->pci_dev->dev);
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
	dev->vpd_port = INT_MAX;
	dev->vpd_pfi  = INT_MAX;
	dev->vpd_pf = NULL;
	dev->pirq_slot = PHYSICAL_SLOT_UNKNOWN;
	dev->pcie_slot = pcie_get_slot(p);
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

/* Scan all devices for virtfnX (physical function) -> physfn mapping */
static void scan_sriov(struct libbiosdevname_state *state)
{
	struct pci_device *pf, *vf;
	char path[PATH_MAX], rpath[PATH_MAX], *rp;
	char pci_name[16];
	struct dirent *de;
	int vf_index;
	DIR *dir;

	list_for_each_entry(pf, &state->pci_devices, node) {
		unparse_pci_name(pci_name, sizeof(pci_name), pf->pci_dev);
		snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s", pci_name);

		if ((dir = opendir(path)) == NULL)
			continue;
		while ((de = readdir(dir)) != NULL) {
			if (sscanf(de->d_name, "virtfn%u", &vf_index) != 1)
				continue;

			/* find virtual function from virtfnX link */
			snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/%s", pci_name, de->d_name);
			if (realpath(path, rpath) == NULL)
				continue;
			if ((rp = strrchr(rpath, '/')) == NULL)
				continue;
			vf = find_dev_by_pci_name(state, rp+1);
			if (vf == NULL)
				continue;

			/* Ok we now have virtual function and physical function */
			pf->is_sriov_physical_function = 1;
			vf->is_sriov_virtual_function = 1;
			list_add_tail(&vf->vfnode, &pf->vfs);
			vf->vf_index = vf_index;
			vf->pf = pf;
		}
		closedir(dir);
	}
}

static void scan_npar(struct libbiosdevname_state *state)
{
	struct pci_device *pf, *vf;
	char path[PATH_MAX];
	char pci_name[16];

	list_for_each_entry(pf, &state->pci_devices, node) {
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

	/* Search for SR-IOV and NPAR */
	scan_sriov(state);
	scan_npar(state);

	/* ordering here is important */
	dmidecode_main(state);	/* this will fail on Xen guests, that's OK */

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
