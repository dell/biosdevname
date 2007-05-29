/*
 * Copyright (c) 2006 Dell, Inc.
 * by Matt Domsch <Matt_Domsch@dell.com>
 *
 * Partly based on tools from pcmciautils-014, which states
 * in its header:
 *  (C) 2004-2005  Dominik Brodowski <linux@brodo.de>
 *
 * Partly based on cardctl.c from pcmcia-cs-3.2.7/cardmgr/, which states
 * in its header:
 *
 * The initial developer of the original code is David A. Hinds
 * <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
 *  are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 */

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>

#include <sysfs/libsysfs.h>
#include "state.h"
#include "pcmcia.h"

#define MAX_SOCKET 8

static unsigned char get_lan_tech(cistpl_funce_t *funce)
{
	cistpl_lan_tech_t *t;

	if (funce->type == CISTPL_FUNCE_LAN_TECH) {
		t = (cistpl_lan_tech_t *)(funce->data);
		return t->tech;
	}
	return 0;
}

static unsigned char parse_tuple_for_network(tuple_t *tuple, cisparse_t *parse)
{
	static int func = 0;
	unsigned char rc = 0;

	switch (tuple->TupleCode) {
	case CISTPL_FUNCID:
		func = parse->funcid.func;
		break;
	case CISTPL_FUNCE:
		if (func == CISTPL_FUNCID_NETWORK)
			rc = get_lan_tech(&parse->funce);
		break;
	default:
		break;
	}
	return rc;
}

int get_network_type(unsigned int socket_no, unsigned char *network_type)
{
	int ret = 0;
	tuple_t tuple;
	unsigned char buf[256];
	cisparse_t parse;
	unsigned char n;

	memset(&tuple, 0, sizeof(tuple_t));

        ret = read_out_cis(socket_no, NULL);
        if (ret)
                return (ret);

	tuple.Attributes = TUPLE_RETURN_LINK | TUPLE_RETURN_COMMON;
        tuple.DesiredTuple = RETURN_FIRST_TUPLE;

	ret = pcmcia_get_first_tuple(BIND_FN_ALL, &tuple);
	if (ret)
		return (ret);

	while(tuple.TupleCode != CISTPL_END) {
		tuple.TupleData = buf;
		tuple.TupleOffset = 0;
		tuple.TupleDataMax = 255;

		pcmcia_get_tuple_data(&tuple);

		ret = pccard_parse_tuple(&tuple, &parse);
		if (ret == 0) {
			n = parse_tuple_for_network(&tuple, &parse);
			if (n) {
				*network_type = n;
				break;
			}
		}

		ret = pcmcia_get_next_tuple(BIND_FN_ALL, &tuple);
		if (ret)
			break;
	}

	return (ret);
}

static int pccardctl_socket_exists(unsigned long socket_no)
{
	char file[SYSFS_PATH_MAX];

	snprintf(file, SYSFS_PATH_MAX,
		 "/sys/class/pcmcia_socket/pcmcia_socket%lu/card_insert",
		 socket_no);

	return (!(sysfs_path_is_file(file)));
}

static int read_out_file(char * file, char **output)
{
	struct sysfs_attribute *attr = sysfs_open_attribute(file);
	int ret;
	char *result = NULL;

	*output = NULL;

	if (!attr)
		return -EIO;
	ret = sysfs_read_attribute(attr);

	if (ret || !attr->value || !attr->len || (attr->len > SYSFS_PATH_MAX))
		goto close_out;

	result = malloc(attr->len + 1);
	if (result) {
		memcpy(result, attr->value, attr->len);
		result[attr->len] = '\0';
		if (result[attr->len - 1] == '\n')
			result[attr->len - 1] = '\0';
		*output = result;
	} else
		ret = -ENOMEM;

 close_out:
	sysfs_close_attribute(attr);
	return ret;
}

static int pccardctl_get_one_f(unsigned long socket_no, unsigned int dev, const char *in_file, unsigned int *result)
{
	char *value;
	char file[SYSFS_PATH_MAX];
	int ret;

	snprintf(file, SYSFS_PATH_MAX, "/sys/bus/pcmcia/devices/%lu.%u/%s",
		 socket_no, dev, in_file);
	ret = read_out_file(file, &value);
	if (ret || !value)
		return -EINVAL;

	if (sscanf(value, "0x%X", result) != 1)
		return -EIO;
	return 0;
}

static int pccardctl_get_one(unsigned long socket_no, const char *in_file, unsigned int *result)
{
	return pccardctl_get_one_f(socket_no, 0, in_file, result);
}

static int alloc_pcmcia(struct libbiosdevname_state *state,
			unsigned long int socket_no)
{
	char file[SYSFS_PATH_MAX];
	char dev_s[SYSFS_PATH_MAX];
	char *dev;
	int ret, i;
	int rc;

	if (!pccardctl_socket_exists(socket_no))
		return -ENODEV;

	snprintf(file, SYSFS_PATH_MAX, "/sys/class/pcmcia_socket/pcmcia_socket%lu/device", socket_no);
	ret = readlink(file, dev_s, sizeof(dev_s) - 1);
	if (ret > 0) {
		dev_s[ret]='\0';
		dev = basename(dev_s);
	} else {
		snprintf(file, SYSFS_PATH_MAX, "/sys/class/pcmcia_socket/pcmcia_socket%lu", socket_no);
		ret = readlink(file, dev_s, sizeof(dev_s) - 1);
		if (ret <= 0)
			return -ENODEV;
		dev_s[ret]='\0';
		dev = basename(dirname(dev_s));
	}

	for (i=0; i<4; i++) {
		unsigned int function;
		unsigned int function_id;
		struct pcmcia_device *pdev;
		unsigned char network_type = 0;

		if (pccardctl_get_one_f(socket_no, i, "function", &function))
			continue;

		pdev = malloc(sizeof(*pdev));
		if (!pdev)
			return -ENOMEM;

		INIT_LIST_HEAD(&pdev->node);
		pdev->socket = socket_no;
		pdev->function = i;
		if (!pccardctl_get_one(socket_no, "func_id", &function_id))
			pdev->function_id = function_id;

		rc = get_network_type(socket_no, &network_type);
		if (!rc)
			pdev->network_type = network_type;

		list_add_tail(&pdev->node, &state->pcmcia_devices);
	}
	return 0;
}

void free_pcmcia_devices(struct libbiosdevname_state *state)
{
	struct pcmcia_device *pos, *next;
	list_for_each_entry_safe(pos, next, &state->pcmcia_devices, node) {
		list_del(&pos->node);
		free(pos);
	}
}

int get_pcmcia_devices(struct libbiosdevname_state *state)
{
	unsigned long int socket_no;
	for (socket_no = 0; socket_no < MAX_SOCKET; socket_no++) {
		alloc_pcmcia(state, socket_no);
	}
	return 0;
}

int unparse_pcmcia_name(char *buf, int size, const struct pcmcia_device *pdev)
{
	return snprintf(buf, size, "%u.%u", pdev->socket, pdev->function);
}

int unparse_pcmcia_device(char *buf, const int size, const struct pcmcia_device *p)
{
	char *s = buf;
	s += snprintf(s, size-(s-buf), "PCMCIA location : ");
	s += unparse_pcmcia_name(s, size-(s-buf), p);
	s += snprintf(s, size-(s-buf), "\n");
	return (s-buf);
}

