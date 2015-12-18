/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/sockios.h>
#include <unistd.h>
#include <net/if.h>
#include <net/if_arp.h>
#include "ethtool-util.h"
#include "pci.h"
#include "eths.h"
#include "state.h"
#include "sysfs.h"

/* Display an Ethernet address in readable format. */
char *pr_ether(char *buf, const int size, const unsigned char *s)
{
	snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X",
		 (s[0] & 0377), (s[1] & 0377), (s[2] & 0377),
		 (s[3] & 0377), (s[4] & 0377), (s[5] & 0377)
		);
	return (buf);
}

static void eths_get_devid(const char *devname, int *devid)
{
	char path[PATH_MAX];
	char *devidstr = NULL;

	*devid = -1;
	snprintf(path, sizeof(path), "/sys/class/net/%s/dev_port", devname);
	if (sysfs_read_file(path, &devidstr) == 0) {
		sscanf(devidstr, "%i", devid);
		free(devidstr);
	} else {
		snprintf(path, sizeof(path), "/sys/class/net/%s/dev_id", devname);
		if (sysfs_read_file(path, &devidstr) == 0) {
			sscanf(devidstr, "%i", devid);
			free(devidstr);
		}
	}
}

static int eths_get_devtype(struct network_device *dev)
{
	int fd;
	int ret = 0;
	ssize_t length = 0;
	char path[PATH_MAX];
	char *result = NULL, *n, *temp;
	unsigned long resultsize = 0, devtype_len = 0;

	snprintf(path, sizeof(path), "/sys/class/net/%s/uevent", dev->kernel_name);

	resultsize = getpagesize();
	result = malloc(resultsize);
	if (!result)
		return -ENOMEM;
	memset(result, 0, resultsize);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ret = fd;
		goto free_out;
	}

	length = read(fd, result, resultsize-1);
	close(fd);

	if (length < 0) {
		ret = -1;
		goto free_out;
	}
	result[length] = '\0';
	
	n = strstr(result, "DEVTYPE=");
	if (n) {
		n += strlen("DEVTYPE=");
		if (!strncmp(n, "fcoe", 4)) 
			ret = 1;
	}
			
	if (ret && (temp = strstr(n, "\n")) != NULL) {
		*temp = '\0';
		devtype_len = strlen(n);
		dev->devtype = malloc(devtype_len + 1);
		if (dev->devtype) {
			memset(dev->devtype, 0, devtype_len + 1);
			strncpy(dev->devtype, n, devtype_len);
		}
	}
free_out:
	free(result);
	return ret;
}

static int eths_get_ifindex(const char *devname, int *ifindex)
{
	int fd, err;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, devname, sizeof(ifr.ifr_name)-1);

	/* Open control socket. */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("Cannot get control socket");
		return 1;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (!err) {
	   	*ifindex = ifr.ifr_ifindex;
	}
	close(fd);
	return err;
}

static int eths_get_hwaddr(const char *devname, unsigned char *buf, int size, int *type)
{
	int fd, err;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, devname, sizeof(ifr.ifr_name)-1);

	/* Open control socket. */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("Cannot get control socket");
		return 1;
	}

	err = ioctl(fd, SIOCGIFHWADDR, &ifr);
	if (!err) {
		memcpy(buf, ifr.ifr_hwaddr.sa_data, min(size, sizeof(ifr.ifr_hwaddr.sa_data)));
		*type = ifr.ifr_hwaddr.sa_family;
	}
	close(fd);
	return err;
}

static int eths_get_info(const char *devname, struct ethtool_drvinfo *drvinfo)
{
	int fd, err;
	struct ifreq ifr;

	/* Setup our control structures. */
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, devname, sizeof(ifr.ifr_name)-1);

	/* Open control socket. */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("Cannot get control socket");
		return 1;
	}

	drvinfo->cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (caddr_t)drvinfo;
	err = ioctl(fd, SIOCETHTOOL, &ifr);
	close(fd);
	return err;
}

static int eths_get_permaddr(const char *devname, unsigned char *buf, int size)
{
	int fd, err;
	struct ifreq ifr;
	struct ethtool_perm_addr *permaddr;
	int s = sizeof(*permaddr) + MAX_ADDR_LEN;

	permaddr = malloc(s);
	if (!permaddr)
		return 1;
	memset(permaddr, 0, s);

	/* Setup our control structures. */
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, devname, sizeof(ifr.ifr_name)-1);



	/* Open control socket. */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		free(permaddr);
		perror("Cannot get control socket");
		return 1;
	}

	permaddr->cmd = ETHTOOL_GPERMADDR;
	permaddr->size = MAX_ADDR_LEN;
	ifr.ifr_data = (caddr_t)permaddr;
	err = ioctl(fd, SIOCETHTOOL, &ifr);
	if (err < 0) {
		close(fd);
		free(permaddr);
		return err;
	}
	memcpy(buf, permaddr->data, min(permaddr->size, size));
	free(permaddr);
	close(fd);
	return err;
}

static void fill_eth_dev(struct network_device *dev)
{
	int rc, devtype;
	eths_get_ifindex(dev->kernel_name, &dev->ifindex);
	eths_get_hwaddr(dev->kernel_name, dev->dev_addr, sizeof(dev->dev_addr), &dev->arphrd_type);
	eths_get_permaddr(dev->kernel_name, dev->perm_addr, sizeof(dev->perm_addr));
	eths_get_devid(dev->kernel_name, &dev->devid);
	devtype = eths_get_devtype(dev);
	if (devtype > 0)
		dev->devtype_is_fcoe = 1;
	rc = eths_get_info(dev->kernel_name, &dev->drvinfo);
	if (rc == 0)
		dev->drvinfo_valid = 1;
}

void free_eths(struct libbiosdevname_state *state)
{
	struct network_device *pos, *next;
	list_for_each_entry_safe(pos, next, &state->network_devices, node) {
		list_del(&pos->node);
		free(pos);
	}
}

/* read_proc.c */
extern int get_interfaces(struct libbiosdevname_state *state);

void get_eths(struct libbiosdevname_state *state)
{
	struct network_device *pos;
	get_interfaces(state);
	list_for_each_entry(pos, &state->network_devices, node) {
		fill_eth_dev(pos);
	}
}

int zero_mac(const void *addr)
{
	char zero_mac[MAX_ADDR_LEN];
	memset(zero_mac, 0, sizeof(zero_mac));
	return !memcmp(zero_mac, addr, sizeof(zero_mac));
}


int unparse_network_device(char *buf, const int size, struct network_device *dev)
{
	char buffer[40];
	char *s = buf;
	s += snprintf(s, size-(s-buf), "Kernel name: %s\n", dev->kernel_name);
	if (!zero_mac(dev->perm_addr))
		s += snprintf(s, size-(s-buf), "Permanent MAC: %s\n", pr_ether(buffer, sizeof(buffer), dev->perm_addr));
	s += snprintf(s, size-(s-buf), "Assigned MAC : %s\n", pr_ether(buffer, sizeof(buffer), dev->dev_addr));
	s += snprintf(s, size-(s-buf), "ifIndex: %d\n", dev->ifindex);
	if (drvinfo_valid(dev)) {
		s += snprintf(s, size-(s-buf), "Driver: %s\n", dev->drvinfo.driver);
		s += snprintf(s, size-(s-buf), "Driver version: %s\n", dev->drvinfo.version);
		s += snprintf(s, size-(s-buf), "Firmware version: %s\n", dev->drvinfo.fw_version);
		s += snprintf(s, size-(s-buf), "Bus Info: %s\n", dev->drvinfo.bus_info);
	}
	return (s-buf);
};

struct network_device * find_net_device_by_bus_info(struct libbiosdevname_state *state,
						    const char *bus_info)
{
	struct network_device *n;
	list_for_each_entry(n, &state->network_devices, node) {
		if (!strncmp(n->drvinfo.bus_info, bus_info, sizeof(n->drvinfo.bus_info)))
			return n;
	}
	return NULL;
}

int is_ethernet(struct network_device *dev)
{
	int i;
	int rc = 0;

	/* FIXME: /sys/class/net/$kernel_name/device will be a symlink if there's underlying hardware,
	 * or not exist if it's virtual.  Try using that if this isn't good enough already.
	 */

	/* No bus means not visible to BIOS */
	if (strncmp("N/A", dev->drvinfo.bus_info, sizeof(dev->drvinfo.bus_info)) == 0)
		goto out;

	const char *nonethernet_drivers[] = {
		"bonding",
		"bridge",
		"openvswitch",
		"tun",
	};
	for (i=0; i<sizeof(nonethernet_drivers)/sizeof(nonethernet_drivers[0]); i++) {
		if (strncmp(dev->drvinfo.driver, nonethernet_drivers[i], sizeof(dev->drvinfo.driver)) == 0)
			goto out;
	}

	rc = dev->arphrd_type == ARPHRD_ETHER;
out:
	return rc;
}
