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

struct dmi_embedded_device
{
	u8 type;
	u8 instance;
	u16 domain;
	u8 bus;
	u8 device;
	u8 function;
	const char *reference_designation;
};

struct dmi_addon_device
{
	u8 type;
	u8 instance;
	u16 domain;
	u8 bus;
	u8 device;
	u8 function;
	const char *reference_designation;
};

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

void smbios_setslot(const struct libbiosdevname_state *state,
		    int domain, int bus, int device, int func,
		    int type, int slot, int index, const char *label);
