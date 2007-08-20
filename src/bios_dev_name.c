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
	fprintf(stderr, "   -n        or --nosort              don't sort the PCI device list breadth-first\n");
	fprintf(stderr, "   --policy [kernelnames | all_ethN | all_names | embedded_ethN_slots_names]\n");
	fprintf(stderr, " Example:  biosdevname -i eth0\n");
	fprintf(stderr, "  returns: eth0\n");
	fprintf(stderr, "  when the BIOS name and kernel name are both eth0.\n");
	fprintf(stderr, " --nosort implies --policy kernelnames.\n");
	fprintf(stderr, " You must be root to run this, as it must read from /dev/mem.\n");
}

static int
set_policy(const char *arg)
{
	int rc = all_ethN;
	if (!strncmp("kernelnames", arg, sizeof("kernelnames")))
		rc = kernelnames;
	else if (!strncmp("all_ethN", arg, sizeof("all_ethN")))
		rc = all_ethN;
	else if (!strncmp("all_names", arg, sizeof("all_names")))
		rc = all_names;
	else if (!strncmp("embedded_ethN_slots_names", arg, sizeof("embedded_ethN_slots_names")))
		rc = embedded_ethN_slots_names;
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
			{"nosort",            no_argument, 0, 'n'},
			{"policy",      required_argument, 0, 'p'},
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
		case 'n':
			opts.sortroutine = nosort;
			break;
		case 'p':
			opts.namingpolicy = set_policy(optarg);
			break;
		default:
			usage();
			exit(1);
		}
	}

	if (optind < argc) {
		opts.argc = argc;
		opts.argv = argv;
		opts.optind = optind;
	}

	if (opts.sortroutine == nosort)
		opts.namingpolicy = kernelnames;
}

int main(int argc, char *argv[])
{
	int i, rc=0;
	char *name;
	void *cookie = NULL;

	parse_opts(argc, argv);
	cookie = setup_bios_devices(opts.sortroutine, opts.namingpolicy);
	if (!cookie) {
		usage();
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
