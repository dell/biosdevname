/*
 * Trimmed from DMI Decode http://savannah.nongnu.org/projects/dmidecode
 *
 *   (C) 2000-2002 Alan Cox <alan@redhat.com>
 *   (C) 2002-2007 Jean Delvare <khali@linux-fr.org>
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
 *
 *   For the avoidance of doubt the "preferred form" of this code is one which
 *   is in an open unpatent encumbered format. Where cryptographic key signing
 *   forms part of the process of creating an executable the information
 *   including keys needed to generate an equivalently functional executable
 *   are deemed to be part of the source code.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "types.h"
#include "util.h"
#include "dmidecode.h"
#include "dmioem.h"
#include "../state.h"
#include "../pci.h"

static const char *out_of_spec = "<OUT OF SPEC>";
static const char *bad_index = "<BAD INDEX>";

/*
 * Type-independant Stuff
 */

const char *dmi_string(struct dmi_header *dm, u8 s)
{
	char *bp=(char *)dm->data;
	size_t i, len;

	if(s==0)
		return "Not Specified";

	bp+=dm->length;
	while(s>1 && *bp)
	{
		bp+=strlen(bp);
		bp++;
		s--;
	}

	if(!*bp)
		return bad_index;

	/* ASCII filtering */
	len=strlen(bp);
	for(i=0; i<len; i++)
		if(bp[i]<32 || bp[i]==127)
			bp[i]='.';

	return bp;
}

static const char *dmi_slot_current_usage(u8 code)
{
	/* 3.3.10.3 */
	static const char *usage[]={
		"Other", /* 0x01 */
		"Unknown",
		"Available",
		"In Use" /* 0x04 */
	};

	if(code>=0x01 && code<=0x04)
		return usage[code-0x01];
	return out_of_spec;
}

static void dmi_slot_segment_bus_func(u16 code1, u8 code2, u8 code3, u8 type, const char *prefix)
{
	/* 3.3.10.8 */
        if (!(code1==0xFFFF && code2==0xFF && code3==0xFF))
	      printf("%sSegment Group %u, Bus %u, Device %u, Function %u ",
		     prefix, code1, code2, (code3>>3)&0x1F, (code3&0x7));
	switch(type)
	{
		case 0x06: /* PCI */
		case 0x0E: /* PCI */
		case 0x0F: /* AGP */
		case 0x10: /* AGP */
		case 0x11: /* AGP */
		case 0x12: /* PCI-X */
		case 0x13: /* AGP */
		case 0xA5: /* PCI Express */
			printf("\n");
			break;
	        default:
			if (code1 != 0xFF || code2 != 0xFF || code3 != 0xFF)
				printf("%s\n", out_of_spec);
			break;
	}
}

static u8 onboard_device_type(u8 code, const char *prefix)
{
	/* 3.3.x.2 */
       u8 e = (code & 0x80)>>7;
	static const char *type[]={
		"Other", /* 1 */
		"Unknown",
		"Video",
		"SCSI Controller",
		"Ethernet",
		"Token Ring",
		"Sound",
		"PATA Controller",
		"SATA Controller",
		"SAS Controller" /* 0x0A */
	};
	code = code & 0x7F;
	if(code>=0x01 && code<=0x0A) {
		printf("%sStatus: %s\n", prefix, e?"Enabled":"Disabled");
		printf("%sDevice Type: %s\n", prefix, type[code-0x01]);
	}
	else
		printf("%sDevice Type: %s\n", prefix, out_of_spec);
}

/*
 * Main
 */

#define        MIN(a,b) (((a)<(b))?(a):(b))

static void dmi_decode(struct dmi_header *h, u16 ver, const struct libbiosdevname_state *state)
{
	u8 *data=h->data;
	int domain, bus, device, function;
	struct pci_device *pdev;
	switch(h->type)
	{
		case 9: /* 3.3.10 System Slots */
			if (h->length >= 0x0E && h->length >=0x11) {
				domain = WORD(data+0x0D);
				bus = data[0x0F];
				device = (data[0x10]>>3)&0x1F;
				function = data[0x10] & 7;
				if (! (domain == 0xFFFF && bus == 0xFF && data[0x10] == 0xFF)) {
					device = (data[0x10]>>3)&0x1F;
					function = data[0x10] & 7;
					pdev = find_pci_dev_by_pci_addr(state, domain, bus, device, function);
					if (pdev) {
						pdev->physical_slot = WORD(data+0x09);
						pdev->smbios_type = 0;
						pdev->smbios_instance = 0;
					}
				}
			}
			break;
		case 41: /* 3.3.xx Onboard Device Information */
			domain = WORD(data+0x07);
			bus    = data[0x09];
			device = (data[0xa]>>3) & 0x1F;
			function = data[0xa] & 0x7;
			pdev = find_pci_dev_by_pci_addr(state, domain, bus, device, function);
			if (pdev) {
				pdev->physical_slot = 0;
				pdev->smbios_enabled = !!(data[0x05] & 0x80);
				pdev->smbios_type = data[0x05] & 0x7F;
				pdev->smbios_instance = data[0x06];
			}
			break;

		default:
			if(dmi_decode_oem(h, state))
				break;
	}
}

static void to_dmi_header(struct dmi_header *h, u8 *data)
{
	h->type=data[0];
	h->length=data[1];
	h->handle=WORD(data+2);
	h->data=data;
}

static void dmi_table(u32 base, u16 len, u16 num, u16 ver, const char *devmem, const struct libbiosdevname_state *state)
{
	u8 *buf;
	u8 *data;
	int i=0;

	if((buf=mem_chunk(base, len, devmem))==NULL)
	{
#ifndef USE_MMAP
		printf("Table is unreachable, sorry. Try compiling dmidecode with -DUSE_MMAP.\n");
#endif
		return;
	}

	data=buf;
	while(i<num && data+4<=buf+len) /* 4 is the length of an SMBIOS structure header */
	{
		u8 *next;
		struct dmi_header h;

		to_dmi_header(&h, data);

		/*
		 * If a short entry is found (less than 4 bytes), not only it
		 * is invalid, but we cannot reliably locate the next entry.
		 * Better stop at this point, and let the user know his/her
		 * table is broken.
		 */
		if(h.length<4)
			break;

		/* assign vendor for vendor-specific decodes later */
		if(h.type==0 && h.length>=5)
			dmi_set_vendor(dmi_string(&h, data[0x04]));

		/* look for the next handle */
		next=data+h.length;
		while(next-buf+1<len && (next[0]!=0 || next[1]!=0))
			next++;
		next+=2;
		if(next-buf<=len)
			dmi_decode(&h, ver, state);

		data=next;
		i++;
	}
	free(buf);
}


static int smbios_decode(u8 *buf, const char *devmem, const struct libbiosdevname_state *state)
{
	if(checksum(buf, buf[0x05])
	 && memcmp(buf+0x10, "_DMI_", 5)==0
	 && checksum(buf+0x10, 0x0F))
	{
		dmi_table(DWORD(buf+0x18), WORD(buf+0x16), WORD(buf+0x1C),
			  (buf[0x06]<<8)+buf[0x07], devmem, state);
		return 1;
	}

	return 0;
}

static int legacy_decode(u8 *buf, const char *devmem, const struct libbiosdevname_state *state)
{
	if(checksum(buf, 0x0F))
	{
		dmi_table(DWORD(buf+0x08), WORD(buf+0x06), WORD(buf+0x0C),
			  ((buf[0x0E]&0xF0)<<4)+(buf[0x0E]&0x0F), devmem, state);
		return 1;
	}

	return 0;
}

/*
 * Probe for EFI interface
 */
#define EFI_NOT_FOUND   (-1)
#define EFI_NO_SMBIOS   (-2)
static int address_from_efi(size_t *address)
{
	FILE *efi_systab;
	const char *filename;
	char linebuf[64];
	int ret;

	*address=0; /* Prevent compiler warning */

	/*
	 * Linux up to 2.6.6: /proc/efi/systab
	 * Linux 2.6.7 and up: /sys/firmware/efi/systab
	 */
	if((efi_systab=fopen(filename="/sys/firmware/efi/systab", "r"))==NULL
	&& (efi_systab=fopen(filename="/proc/efi/systab", "r"))==NULL)
	{
		/* No EFI interface, fallback to memory scan */
		return EFI_NOT_FOUND;
	}
	ret=EFI_NO_SMBIOS;
	while((fgets(linebuf, sizeof(linebuf)-1, efi_systab))!=NULL)
	{
		char *addrp=strchr(linebuf, '=');
		*(addrp++)='\0';
		if(strcmp(linebuf, "SMBIOS")==0)
		{
			*address=strtoul(addrp, NULL, 0);
			ret=0;
			break;
		}
	}
	if(fclose(efi_systab)!=0)
		perror(filename);

	if(ret==EFI_NO_SMBIOS)
		fprintf(stderr, "%s: SMBIOS entry point missing\n", filename);
	return ret;
}

static const char *devmem = "/dev/mem";

int dmidecode_main(const struct libbiosdevname_state *state)
{
	int ret=0;                  /* Returned value */
	int found=0;
	size_t fp;
	int efi;
	u8 *buf;

	/* First try EFI (ia64, Intel-based Mac) */
	efi=address_from_efi(&fp);
	switch(efi)
	{
		case EFI_NOT_FOUND:
			goto memory_scan;
		case EFI_NO_SMBIOS:
			ret=1;
			goto exit_free;
	}

	if((buf=mem_chunk(fp, 0x20, devmem))==NULL)
	{
		ret=1;
		goto exit_free;
	}

	if(smbios_decode(buf, devmem, state))
		found++;
	goto done;

memory_scan:
	/* Fallback to memory scan (x86, x86_64) */
	if((buf=mem_chunk(0xF0000, 0x10000, devmem))==NULL)
	{
		ret=1;
		goto exit_free;
	}

	for(fp=0; fp<=0xFFF0; fp+=16)
	{
		if(memcmp(buf+fp, "_SM_", 4)==0 && fp<=0xFFE0)
		{
			if(smbios_decode(buf+fp, devmem, state))
			{
				found++;
				fp+=16;
			}
		}
		else if(memcmp(buf+fp, "_DMI_", 5)==0)
		{
			if (legacy_decode(buf+fp, devmem, state))
				found++;
		}
	}

done:
	free(buf);


exit_free:
	return ret;
}