/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#ifndef __ETHS_H_INCLUDED
#define __ETHS_H_INCLUDED

#include <net/if.h>
#include <net/if_arp.h>

/* Bogus */
#undef MAX_ADDR_LEN
#define MAX_ADDR_LEN 32

#include "list.h"
#include "ethtool-util.h"
#include "state.h"

struct network_device {
	struct list_head node;
	char kernel_name[IFNAMSIZ];          /* ethN */
	unsigned char perm_addr[MAX_ADDR_LEN];
	unsigned char dev_addr[MAX_ADDR_LEN];   /* mutable MAC address, not unparsed */
	struct ethtool_drvinfo drvinfo;
	int drvinfo_valid;
	int arphrd_type; /* e.g. ARPHDR_ETHER */
	int hardware_claimed; /* true when recognized as PCI or PCMCIA and added to list of bios_devices */
  	int ifindex;
	int devid;
	int devtype_is_fcoe;
	char *devtype;
};

extern void get_eths(struct libbiosdevname_state *state);
extern void free_eths(struct libbiosdevname_state *state);
extern int unparse_network_device(char *buf, const int size, struct network_device *dev);
extern struct network_device * find_net_device_by_bus_info(struct libbiosdevname_state *state,
							   const char *bus_info);
extern int is_ethernet(struct network_device *dev);

extern int zero_mac(const void *addr);

#define min(a,b) ((a) <= (b) ? (a) : (b))

static inline void claim_netdev(struct network_device *dev)
{
	dev->hardware_claimed = 1;
}

static inline int netdev_is_claimed(const struct network_device *dev)
{
	return dev->hardware_claimed != 0;
}

static inline int drvinfo_valid(const struct network_device *dev)
{
	return dev->drvinfo_valid != 0;
}

static inline int netdev_devtype_is_fcoe(const struct network_device *dev)
{
	return (dev->devtype_is_fcoe == 1);
}

#endif /* __ETHS_H_INCLUDED */
