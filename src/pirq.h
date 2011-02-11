#ifndef PIRQ_H_INCLUDED
#define PIRQ_H_INCLUDED

/*
 * PCI IRQ Routing Table dumper
 *  Copyright (c) 2006 Dell, Inc.
 *  by Matt Domsch <Matt_Domsch@dell.com>
 *
 *  Based on the description of said table found at
 *  http://www.microsoft.com/hwdev/busbios/pciirq.htm
 *  Licensed under the GNU General Public license, version 2.
 */

#include <stdint.h>
#include <stdlib.h>

typedef unsigned char __u8;
typedef unsigned short int __u16;
typedef unsigned int __u32;


struct slot_entry {
	__u8  bus;
	__u8  device;
	__u8  inta_link;
	__u16 inta_irq;
	__u8  intb_link;
	__u16 intb_irq;
	__u8  intc_link;
	__u16 intc_irq;
	__u8  intd_link;
	__u16 intd_irq;
	__u8  slot;
	__u8  reserved;
} __attribute__((packed));

struct routing_table {
	__u32 signature;
	__u16 version;
	__u16 size;
	__u8  router_bus;
	__u8  router_devfn;
	__u16 exclusive_irqs;
	__u32 compatable_router;
	__u32 miniport_data;
	__u8  reserved[11];
	__u8  checksum;
	struct slot_entry slot[1];
} __attribute__((packed));


#define PCI_DEVICE(devfn)         (((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)         ((devfn) & 0x07)
extern struct routing_table * pirq_alloc_read_table(void);
extern void pirq_free_table(struct routing_table *table);
extern int pirq_pci_dev_to_slot(struct routing_table *table, int domain, int bus, int dev);

#endif /* PIRQ_H_INCLUDED */
