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
#include <ctype.h>

#include "config.h"
#include "types.h"
#include "util.h"
#include "dmidecode.h"
#include "dmioem.h"
#include "../state.h"
#include "../pci.h"
#include "../naming_policy.h"

extern int smver_mjr, smver_mnr, is_valid_smbios;

#ifdef DEBUG
#define dprintf printf
#else
#define dprintf(a...)
#endif

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

/*
 * Main
 */

#define        MIN(a,b) (((a)<(b))?(a):(b))

static void strip_right(char *s)
{
	int i, len = strlen(s);
	for (i=len; i>=0; i--) {
		if (isspace(s[i-1]))
			s[i-1] = '\0';
		else
			break;
	}
}

static int matchpci(struct pci_device *pdev, int domain, int bus, int device, int func)
{
	if (domain != -1 && pdev->pci_dev->domain != domain)
		return 0;
	if (bus != -1 && pdev->pci_dev->bus != bus)
		return 0;
	if (device != -1 && pdev->pci_dev->dev != device)
		return 0;
	if (func != -1 && pdev->pci_dev->func != func)
		return 0;
	return 1;
}

void smbios_setslot(const struct libbiosdevname_state *state, 
		    int domain, int bus, int device, int func,
		    int type, int slot, int index, const char *label)
{
	struct pci_device *pdev;

	dprintf("setslot: %.4x:%.2x:%.2x.%x = type:%x slot(%2d %2d) %s\n",
		domain, bus, device, func, type, slot, index, label);

	/* Don't bother with disabled devices */
	if ((domain == 0xFFFF) ||
	    (bus == 0 && device == 0 && func == 0) ||    /* bug on HP systems */
	    (bus == 0xFF && device == 0x1F && func == 0x7)) 
	{
		dprintf("  disabled\n");
		return;
	}

	list_for_each_entry(pdev, &state->pci_devices, node) {
		if (!matchpci(pdev, domain, bus, device, func))
			continue;

		dprintf("  found device: %.4x:%.2x:%.2x.%x = %lx\n",
			pdev->pci_dev->domain, pdev->pci_dev->bus, pdev->pci_dev->dev, 
			pdev->pci_dev->func, pdev->class);
    
		pdev->uses_smbios |= HAS_SMBIOS_SLOT;
		if (index != 0)
			pdev->uses_smbios |= HAS_SMBIOS_INSTANCE;
		pdev->smbios_type = type;
		pdev->smbios_enabled = 1;
		pdev->smbios_instance = index;

		pdev->physical_slot = slot;
		if (label) {
			pdev->smbios_label = strdup(label);
			pdev->uses_smbios |= HAS_SMBIOS_LABEL;
			strip_right(pdev->smbios_label);
		}
    
		/* Found a PDEV, now is it a bridge? */
		if (pdev->sbus != -1) {
			smbios_setslot(state, domain, pdev->sbus, -1, -1, type, slot, index, label);
		}
	}
}

static void dmi_decode(struct dmi_header *h, u16 ver, const struct libbiosdevname_state *state)
{
	u8 *data=h->data;

	int domain, bus, device, function;
	switch(h->type)
	{
	case 9: /* 3.3.10 System Slots */
		if (h->length >= 0x0E && h->length >=0x11) {
			domain = WORD(data+0x0D);
			bus = data[0x0F];
			device = (data[0x10]>>3)&0x1F;
			function = data[0x10] & 7;

			/* Root ports can be on multiport device.. scan single */
			if (!is_root_port(state, domain, bus, device, function))
				function = -1;
			smbios_setslot(state, domain, bus, device, function, 
				       0x00, WORD(data+0x09), 0x00,
				       dmi_string(h, data[0x04]));
		}
		else {
			dprintf("Old Slot: id:%3d, type:%.2x, label:%-7s\n", WORD(data+0x09), data[0x05], dmi_string(h, data[0x04]));
		}
		break;
	case 41: /* 3.3.xx Onboard Device Information */
		domain = WORD(data+0x07);
		bus    = data[0x09];
		device = (data[0xa]>>3) & 0x1F;
		function = data[0xa] & 0x7;

		if (data[5] == (0x80 | 0x05)) {
			// enabled and type == ethernet
			smbios_setslot(state, domain, bus, device, function,
				       data[5] & 0x7F, 0x00, data[0x06],
				       dmi_string(h, data[0x04]));
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

static int isvalidsmbios(int mjr, int mnr)
{
	if (!smver_mjr && !smver_mnr) {
		is_valid_smbios = 1;
		return 1;
	}
	if (mjr > smver_mjr) {
		is_valid_smbios = 1;
		return 1;
	}
	if ((mjr == smver_mjr) && (mnr >= smver_mnr)) {
		is_valid_smbios = 1;
		return 1;
	}
	return 0;
}

static void dmi_table(u32 base, u16 len, u16 num, u16 ver, const char *devmem, const struct libbiosdevname_state *state)
{
	u8 *buf;
	u8 *data;
	int i=0;

	/* Verify SMBIOS version */
	if (!isvalidsmbios(ver >> 8, ver & 0xFF)) {
		return;
	}
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

int dmidecode_read_file(const struct libbiosdevname_state *state)
{
#ifdef _JPH
	FILE *fp;
	const char *dmidecode_file = "dmidecode.txt";
	char line[128], *r;
	int type = -1, eth=0,s,b,d,f,slot,i;

	if ((fp = fopen(dmidecode_file, "r")) == NULL)
		return 0;
	while ((fgets(line, sizeof(line), fp)) != NULL) {
		if (strstr(line, " DMI type 41,") != NULL) {
			type = 41;
			eth = 0;
			slot = -1;
		} else if (strstr(line, " DMI type 9,") != NULL) {
			type = 9;
		} else if (strstr(line, " DMI type ") != NULL) {
			type = -1;
		}
		if (type == 41) {
			if ((r = strstr(line, "Type: Ethernet")) != NULL) {
				eth = 1;
			}
			if ((r = strstr(line, "Type Instance: ")) != NULL) {
				sscanf(r, "Type Instance: %d", &slot);
			}
			if ((r = strstr(line, "Bus Address: ")) != NULL && eth) {
				sscanf(r, "Bus Address: %x:%x:%x.%x", &s,&b,&d,&f);
				printf("bus: %.4x:%.2x:%.2x.%x\n", s, b, d, f);
				smbios_setslot(state, s, b, d, f, 0x5, 0x00, slot, "");
			}
		}
		if (type == 9) {
			/* System Slots */
			if ((r = strstr(line, "ID: ")) != NULL) {
				sscanf(r, "ID: %d", &slot);
			}
			if ((r = strstr(line, "Bus Address: ")) != NULL) {
				sscanf(r, "Bus Address: %x:%x:%x.%x", &s,&b,&d,&f);
				printf("bus: %.4x:%.2x:%.2x.%x = %d\n", s, b, d, f, slot);
				for (i=0; i<8; i++)
					smbios_setslot(state, s, b, d, i, 0x00, slot, 0x00, "");
			}
		}
	}
	return 1;
#endif
	return 0;
}

int dmidecode_main(const struct libbiosdevname_state *state)
{
	int ret=0;                  /* Returned value */
	int found=0;
	size_t fp;
	int efi;
	u8 *buf;

	if (dmidecode_read_file(state))
		return 0;

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
