/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#ifndef ETHTOOL_UTIL_H__
#define ETHTOOL_UTIL_H__

#include <pci/pci.h>

#include <limits.h>
#if ULONG_MAX > 0xffffffff
typedef unsigned long u64;
#else
typedef unsigned long long u64;
#endif

#include "ethtool-copy.h"

int ethtool_get_info(const char *devname, struct ethtool_drvinfo *drvinfo);

#endif
