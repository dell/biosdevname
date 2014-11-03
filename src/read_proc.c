/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 *   Copied from from net-tools-1.60, also under the GNU GPL v2,
 *   and modified for use here.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include "eths.h"

#define _PATH_PROCNET_DEV "/proc/net/dev"

static struct network_device *add_interface(struct libbiosdevname_state *state,
					    const char *name)
{
	struct network_device *i;
	i = malloc(sizeof(*i));
	if (!i)
		return NULL;
	memset(i, 0, sizeof(*i));
	INIT_LIST_HEAD(&i->node);
	strncpy(i->kernel_name, name, sizeof(i->kernel_name)-1);
	list_add_tail(&i->node, &state->network_devices);
	return i;
}


static char *get_name(char **namep, char *p)
{
    int count = 0;
    char *name;
    while (isspace(*p))
	p++;
    name = *namep = p;
    while (*p) {
	if (isspace(*p))
	    break;
	if (*p == ':') {	/* could be an alias */
	    char *dot = p, *dotname = name;
	    *name++ = *p++;
	    count++;
	    while (isdigit(*p)){
		*name++ = *p++;
	        count++;
	 	if (count == (IFNAMSIZ-1))
	    	      break;
	    }
	    if (*p != ':') {	/* it wasn't, backup */
		p = dot;
		name = dotname;
	    }
	    if (*p == '\0')
		return NULL;
	    p++;
	    break;
	}
	*name++ = *p++;
	count++;
	if (count == (IFNAMSIZ-1))
    	      break;
    }
    *name++ = '\0';
    return p;
}



int get_interfaces(struct libbiosdevname_state *state)
{
	FILE *fh;
	int err;
	char *line = NULL;
	size_t linelen = 0;

	fh = fopen(_PATH_PROCNET_DEV, "r");
	if (!fh) {
		fprintf(stderr, "Error: cannot open %s (%s).\n",
			_PATH_PROCNET_DEV, strerror(errno));
		return 1;
	}
	if (getline(&line, &linelen, fh) == -1 /* eat line */
	    || getline(&line, &linelen, fh) == -1) {
		err = -1;
		goto out;
	}

	err = 0;
	while (getline(&line, &linelen, fh) != -1) {
		char *name;
		get_name(&name, line);
		add_interface(state, name);
	}
	if (ferror(fh))
		err = -1;

out:
	free(line);
	fclose(fh);
	return err;
}
