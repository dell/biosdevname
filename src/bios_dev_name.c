/*
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *  Licensed under the GNU General Public license, version 2.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "libbiosdevname.h"
#include "bios_dev_name.h"

static struct bios_dev_name_opts opts;

static void usage(void)
{
	fprintf(stderr, "Usage:  biosdevname [options] [args]...\n");
	fprintf(stderr, " Options:\n");
	fprintf(stderr, "   -i        or --interface           treat [args] as ethernet devs\n");
	fprintf(stderr, "   -d        or --debug               enable debugging\n");
	fprintf(stderr, "   --policy [physical | all_ethN ]\n");
	fprintf(stderr, "   --prefix [string]                  string use for embedded NICs (default='em')\n");
	fprintf(stderr, " Example:  biosdevname -i eth0\n");
	fprintf(stderr, "  returns: em1\n");
	fprintf(stderr, "  when eth0 is an embedded NIC with label '1' on the chassis.\n");
	fprintf(stderr, " You must be root to run this, as it must read from /dev/mem.\n");
}

static int
set_policy(const char *arg)
{
	int rc = physical;

	if (!strncmp("physical", arg, sizeof("physical")))
		rc = physical;
	else if (!strncmp("all_ethN", arg, sizeof("all_ethN")))
		rc = all_ethN;
	return rc;
}

static void
parse_opts(int argc, char **argv)
{
	int c;
	int option_index = 0;

	while (1) {
		static struct option long_options[] =
			/* name, has_arg, flag, val */
		{
			{"debug",             no_argument, 0, 'd'},
			{"interface",         no_argument, 0, 'i'},
			{"policy",      required_argument, 0, 'p'},
			{"prefix",      required_argument, 0, 'P'},
			{0, 0, 0, 0}
		};
		c = getopt_long(argc, argv,
				"dinp:",
				long_options, &option_index);
		if (c == -1)
			break;
		switch(c) {
		case 'd':
			opts.debug = 1;
			break;
		case 'i':
			opts.interface = 1;
			break;
		case 'p':
			opts.namingpolicy = set_policy(optarg);
			break;
		case 'P':
			opts.prefix = optarg;
			break;
		default:
			usage();
			exit(1);
		}
	}

	if (optind < argc) {
		opts.argc = argc-optind;
		opts.argv = &argv[optind];
		opts.optind = optind;
	}

	if (opts.prefix == NULL)
		opts.prefix = "em";
}

int main(int argc, char *argv[])
{
	int i, rc=0;
	char *name;
	void *cookie = NULL;

	parse_opts(argc, argv);
	cookie = setup_bios_devices(opts.namingpolicy, opts.prefix);
	if (!cookie) {
		rc = 1;
		goto out;
	}

	if (opts.debug) {
		unparse_bios_devices(cookie);
		rc = 0;
		goto out_cleanup;
	}


	if (!opts.interface) {
		fprintf(stderr, "Unknown device type, try passing an option like -i\n");
		rc = 1;
		goto out_usage;
	}

	for (i=0; i<opts.argc; i++) {
		name = kern_to_bios(cookie, opts.argv[i]);
		if (name) {
			printf("%s\n", name);
		}
		else
			rc |= 2; /* one or more given devices weren't found */
	}
	goto out_cleanup;

 out_usage:
	usage();
 out_cleanup:
	cleanup_bios_devices(cookie);
 out:
	return rc;
}
