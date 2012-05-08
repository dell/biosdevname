/*
 * This file is part of the dmidecode project.
 *
 *   (C) 2005-2007 Jean Delvare <khali@linux-fr.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

struct dmi_header
{
	u8 type;
	u8 length;
	u16 handle;
	u8 *data;
};

const char *dmi_string(struct dmi_header *dm, u8 s);

enum dmi_onboard_device_type {
	DMI_OTHER=1,
	DMI_UNKNOWN,
	DMI_VIDEO,
	DMI_SCSI,
	DMI_ETHERNET,
	DMI_TOKEN_RING,
	DMI_SOUND,
	DMI_PATA,
	DMI_SATA,
	DMI_SAS,
};

struct libbiosdevname_state;
int dmidecode_main(const struct libbiosdevname_state *state);

struct dmi_ep
{
	u8               	anchor[5];
	u8               	checksum;
	u16              	table_len;
	u32              	table_addr;
	u16              	count;
	u8               	rev;
} __attribute__((packed));

struct smbios_ep
{
	u8               	anchor[4];
	u8               	checksum;
	u8               	ep_length;
	u8               	mjr_rev;
	u8               	mnr_rev;
	u16              	max_size;
	u8             		ep_rev;
	u8             	  	fmtarea[5];
	struct dmi_ep         	dmi;
} __attribute__((packed));

struct smbios_hdr
{
	u8               	type;
	u8               	length;
	u16              	handle;
} __attribute__((packed));

struct smbios_type9
{
	struct smbios_hdr     	hdr;
	u8               	ref;
	u8              	type;
	u8               	buswidth;
	u8               	usage;
	u8               	length;
	u16              	id;
	u8               	flags1;
	/* 2.1+ */
	u8               	flags2;
	/* 2.6+ */
	u16              	segment;
	u8               	bus;
	u8               	devfn;
} __attribute__((packed));

struct smbios_type10
{
	struct smbios_hdr     	hdr;
	struct {
		u8              type;
		u8              desc;
	} dev[1];
};

/* 2.6+ */
struct smbios_type41
{
	struct smbios_hdr     	hdr;
	u8               	ref;
	u8              	type;
	u8               	instance;
	u16              	segment;
	u8              	bus;
	u8               	devfn;
} __attribute__((packed));

