/*
 * Decoding of OEM-specific entries
 * This file is part of the dmidecode project.
 *
 *   (C) 2007 Jean Delvare <khali@linux-fr.org>
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

#include <stdio.h>
#include <string.h>

#include "types.h"
#include "dmidecode.h"
#include "dmioem.h"
#include "../pci.h"

/*
 * Globals for vendor-specific decodes
 */

enum DMI_VENDORS { VENDOR_UNKNOWN, VENDOR_HP };

static enum DMI_VENDORS dmi_vendor=VENDOR_UNKNOWN;

/*
 * Remember the system vendor for later use. We only actually store the
 * value if we know how to decode at least one specific entry type for
 * that vendor.
 */
void dmi_set_vendor(const char *s)
{
	if(strcmp(s, "HP")==0)
		dmi_vendor=VENDOR_HP;
}

/*
 * HP-specific data structures are decoded here.
 *
 * Code contributed by John Cagle.
 */

static int dmi_decode_hp(struct dmi_header *h, const struct libbiosdevname_state *state)
{
	u8 *data=h->data;
	int nic, ptr;
	u8 smbios_type = 0;
	u8 bus, device, func;

	switch(h->type)
	{
		case 209:
			/*
			 * Vendor Specific: HP ProLiant NIC MAC Information
			 *
			 * This prints the BIOS NIC number,
			 * PCI bus/device/function, and MAC address
			 */
			smbios_type=0x05;

			nic=1;
			ptr=4;
			while(h->length>=ptr+8)
			{
				bus = data[ptr+1];
				device = data[ptr]>>3;
				func = data[ptr]&7;

				smbios_setslot(state, 0, bus, device, func, smbios_type, 0, nic, NULL);

				nic++;
				ptr += 8;
			}
			break;

		default:
			return 0;
	}
	return 1;
}

/*
 * Dispatch vendor-specific entries decoding
 * Return 1 if decoding was successful, 0 otherwise
 */
int dmi_decode_oem(struct dmi_header *h, const struct libbiosdevname_state *state)
{
	switch(dmi_vendor)
	{
		case VENDOR_HP:
			return dmi_decode_hp(h, state);
		default:
			return 0;
	}
}
