/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#ifndef NAMING_POLICY_H_INCLUDED
#define NAMING_POLICY_H_INCLUDED

#include "state.h"

extern int assign_bios_network_names(const struct libbiosdevname_state *state,
				     int namingpolicy, const char *prefix);

#endif /* NAMING_POLICY_H_INCLUDED */
