/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */

#ifndef PCMCIA_H_INCLUDED
#define PCMCIA_H_INCLUDED

#include "list.h"
#include "cistpl.h"
#include "state.h"

struct pcmcia_device {
	struct list_head node;
	unsigned int socket;
	unsigned int function;
	int function_id;
	int network_type;
};

extern int get_pcmcia_devices(struct libbiosdevname_state *state);
extern void free_pcmcia_devices(struct libbiosdevname_state *state);
extern int unparse_pcmcia_name(char *buf, int size, const struct pcmcia_device *pdev);
extern int unparse_pcmcia_device(char *buf, const int size, const struct pcmcia_device *p);

static inline int is_pcmcia_network(struct pcmcia_device *dev)
{
	return dev->function_id == CISTPL_FUNCID_NETWORK &&
		(dev->network_type == CISTPL_LAN_TECH_ETHERNET ||
		 dev->network_type == CISTPL_LAN_TECH_WIRELESS);
}


#endif /* PCMCIA_H_INCLUDED */
