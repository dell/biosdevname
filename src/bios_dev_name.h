/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#ifndef GLUE_H_INCLUDED
#define GLUE_H_INCLUDED

struct bios_dev_name_opts {
	int argc;
	char **argv;
	int optind;
	int sortroutine;
	int namingpolicy;
	unsigned int verbose:1;
	unsigned int show_all:1;
	unsigned int debug:1;
	unsigned int interface:1;
};

#endif /* GLUE_H_INCLUDED */
