/*
 * PCI IRQ Routing Table dumper
 * Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *
 * Based on the description of said table found at
 * http://www.microsoft.com/hwdev/busbios/pciirq.htm
 * Licensed under the GNU General Public license, version 2.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <sys/mman.h>
#include "pirq.h"

extern int nopirq;

/* If unknown, use INT_MAX so they get sorted last */
int pirq_pci_dev_to_slot(struct routing_table *table, int domain, int bus, int dev)
{
	int i, num_slots;
	struct slot_entry *slot;

	if (!table)
		return INT_MAX;
	if (domain != 0) /* can't represent non-zero domains in PIRQ */
		return INT_MAX;

	num_slots = (table->size - 32) / sizeof(*slot);
	for (i=0; i<num_slots; i++) {
		slot = &table->slot[i];
		if (slot->bus == bus &&
		    PCI_DEVICE(slot->device) == dev) {
		  	if (slot->slot >= '1' && slot->slot <= '9')
				return slot->slot - '0';
			return slot->slot;
		}
	}
	return INT_MAX;
}

struct routing_table *pirq_read_file()
{
#ifdef _JPH
	FILE *fp;
	char  line[128];
	struct routing_table *table;
	char *r;
	int count, bus, dev, slot;
	const char *pirq_file = "biosdecode.txt";

	/* Get count of entries */
	if ((fp = fopen(pirq_file, "r")) == NULL)
		return NULL;
	count = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (strstr(line, "Slot Entry") != NULL)
			count++;
	}
	fclose(fp);

	/* Read table */
	table = malloc(sizeof(*table) + count * sizeof(struct slot_entry));
	table->size = 32 + (sizeof(struct slot_entry) * count);
	if ((fp = fopen(pirq_file, "r")) == NULL)
		return NULL;
	count = 0;
	while (fgets(line, sizeof(line), fp) != NULL) {
		if ((r = strstr(line, "Slot Entry")) == NULL)
			continue;
		if (sscanf(r, "Slot Entry %*d: ID %x:%x", &bus, &dev) == 2) {
			table->slot[count].bus = bus;
			table->slot[count].device = dev << 3;
			if ((r = strstr(line, "on-board")) != NULL)
				table->slot[count].slot = 0;
			else if ((r = strstr(line, "slot number ")) != NULL) {
				sscanf(r, "slot number %d", &slot);
				table->slot[count].slot = slot;
			}
			printf("%d = %.2x:%.2x = %d\n", count, bus, dev, table->slot[count].slot);
			count++;
		}
	}
	fclose(fp);
	return table;
#endif
	return NULL;
}

struct routing_table * pirq_alloc_read_table()
{
	struct routing_table *table = NULL;
	uint16_t size = 0;
	uint8_t checksum = 0;
	int i;
	void *mem;
	off_t offset=0L;
	int fd;

	/* Skip PIRQ table parsing */
	if (nopirq) {
		return NULL;
	}
	if ((table = pirq_read_file()) != NULL)
		return table;

	fd = open("/dev/mem", O_RDONLY);
	if(fd==-1)
		return NULL;

	mem = mmap(0, 64*1024, PROT_READ, MAP_SHARED, fd, 0xF0000L);
	if (mem == (void *)-1LL)
		goto out;

	while( offset < 0xFFFF)
	{
		if(memcmp(mem+offset, "$PIR", 4)==0)
		{
			table = (struct routing_table *)(mem+offset);
			size = table->size;
			/* quick sanity checks */
			if (size == 0) {
				table = NULL;
				break;
			}
			/* Version must be 1.0 */
			if (!((table->version >> 8) == 1 &&
			      (table->version & 0xFF) == 0)) {
				table = NULL;
				break;
			}

			table = malloc(size);
			if (!table) break;

			memcpy(table, mem+offset, size);
			for (i=0; i<size; i++)
				checksum +=*(((uint8_t *)table)+i);
			if (checksum) {
				free (table);
				table = NULL;
			}
			break;
		}
		offset += 16;
	}
	munmap(mem, 64*1024);
out:
	close(fd);
	return table;
}

void pirq_free_table(struct routing_table *table)
{
	if (table)
		free(table);
}


#ifdef UNIT_TEST_PIRQ

static void
pirq_unparse_slot(struct slot_entry *slot)
{
	printf("Slot %d: PCI %x:%x. ",
	       slot->slot, slot->bus, PCI_DEVICE(slot->device));
	printf("INTA link %x irq %x ", slot->inta_link, slot->inta_irq);
	printf("B link %x irq %x ", slot->intb_link, slot->intb_irq);
	printf("C link %x irq %x ", slot->intc_link, slot->intc_irq);
	printf("D link %x irq %x ", slot->intd_link, slot->intd_irq);
	printf("\n");
}

static void
pirq_unparse_routing_table(struct routing_table *table)
{
	int i, num_slots;
	struct slot_entry *slot;
	char buf[5]; buf[4] = 0;
	memcpy(buf, &table->signature, 4);

	printf("PCI IRQ Routing Table\n");
	printf("Signature: %s\n", buf);
	printf("Version  : %x\n", table->version);
	printf("Size     : %xh\n", table->size);
	printf("Bus      : %x\n", table->router_bus);
	printf("DevFn    : %x\n", table->router_devfn);
	printf("Exclusive IRQs : %x\n", table->exclusive_irqs);
	printf("Compatible Router: %x\n", table->compatable_router);

	num_slots = (table->size - 32) / sizeof(*slot);
	slot = &table->slot[0];
	for (i=0; i<num_slots; i++) {
		pirq_unparse_slot(&slot[i]);
	}
}

int main(int argc, char *argv[])
{
	struct routing_table *table;
	table = pirq_alloc_read_table();
	if (!table)
		return 1;
	pirq_unparse_routing_table(table);
	pirq_free_table(table);
	return 0;
}

#endif
