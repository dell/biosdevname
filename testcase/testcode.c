#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <inttypes.h>

struct pcidev;

struct netdev
{
  char                 *label;
  struct pcidev        *pd;
};

struct pcidev
{
  char                  *label;
  int 			seg,bus,dev,fun,sbus;
  int                   flag;
  int                   smbios_index, smbios_type;
  TAILQ_ENTRY(pcidev)  	node;
};

#define FLAG_EMBEDDED  0x01
#define FLAG_SLOT      0x02

TAILQ_HEAD(,pcidev) pcilist;

#define zmalloc(x) calloc(1,x)

static int cmppci(const void *a, const void *b)
{
  const struct pcidev *pa = a;
  const struct pcidev *pb = b;
  int rc;

  if ((rc = pa->seg - pb->seg) != 0)
    return rc;
  if ((rc = pa->bus - pb->bus) != 0)
    return rc;
  if ((rc = pa->dev - pb->dev) != 0)
    return rc;
  rc = pa->fun - pb->fun;
  return rc;
}

struct pcidev *
findpci(int s, int b, int d, int f)
{
  struct pcidev *pd;

  TAILQ_FOREACH(pd, &pcilist, node) {
    if ((s == -1 || s == pd->seg) &&
	(b == -1 || b == pd->bus) &&
	(d == -1 || d == pd->dev) &&
	(f == -1 || f == pd->fun))
      return pd;
  }
  return NULL;
}

struct pcidev *
addpci(int s, int b, int d, int f)
{
  struct pcidev *pd, *ld;

  pd = zmalloc(sizeof(*pd));
  pd->seg = s;
  pd->bus = b;
  pd->dev = d;
  pd->fun = f;
  pd->sbus = -1;
  pd->flag = 0;
  pd->smbios_index = -1;

  TAILQ_FOREACH(ld, &pcilist, node) {
    if (cmppci(pd, ld) < 0) {
      TAILQ_INSERT_BEFORE(ld, pd, node);
      return pd;
    }
  }
  TAILQ_INSERT_TAIL(&pcilist, pd, node);
  return pd;
}

int blankline(char *line)
{
  while (isspace(*line))
    line++;
  return (line[0] == 0);
}

int verbose=1;

char *devtype(struct pcidev *pd)
{
  if (pd == NULL)
    return "";
  if (pd->sbus != -1)
    return "bridge";
  return pd->label;
}

int slid = 0;

void _setslot(struct pcidev *pd, int type, int slot, int index, int depth)
{
  int i;

  printf("slot: %2d,%2d,%d  ", slot, index, type);
  for (i=0; i<depth; i++)
    printf(" ");
  printf("%.4x:%.2x:%.2x.%x %s\n", 
	 pd->seg, pd->bus, pd->dev, pd->fun, devtype(pd));
  if (pd->sbus == -1) {
    pd->smbios_type  = slot;
    pd->smbios_index = index;
    pd->flag = 1;
  }
}

void setslot(int seg, int bus, int dev, int fun, int type, int slot, int index, int depth)
{
  struct pcidev *pd, *cd;
  int i;

  pd = findpci(seg, bus, dev, fun);
  printf("slot: %2d,%2d,%d  ", slot, index, type);
  for (i=0; i<depth; i++)
    printf(" ");
  printf("%.4x:%.2x:%.2x.%x %s\n", 
	seg, bus, dev, fun, devtype(pd));
  if (pd && pd->sbus == -1) {
    pd->smbios_type  = slot;
    pd->smbios_index = index;
    pd->flag = 1;
  }
  if (pd && pd->sbus != -1) {
    TAILQ_FOREACH(cd, &pcilist, node) {
      if (pd->sbus == cd->bus) {
	setslot(cd->seg, cd->bus, cd->dev, cd->fun, type, slot, index, depth+1);
      }
    }
  }
  if (slot != 0) {
    TAILQ_FOREACH(cd, &pcilist, node) {
      if (cd->seg == seg && cd->bus == bus && cd->dev == dev && cd->fun != fun) {
	_setslot(cd, type, slot, index, depth);
      }
    }
  }
}

struct pirqmap {
  int bus,dev,slot;
};

int    npirq;
struct pirqmap pirqmap[128];

void addpirqslot(int bus, int dev, int slot)
{
  pirqmap[npirq].bus = bus;
  pirqmap[npirq].dev = dev;
  pirqmap[npirq].slot = slot;
  npirq++;
}

int findpirq(int slot)
{
  int i, idx=-1;
  struct pcidev *pd;

  for (i=0; i<npirq; i++) {
    if (pirqmap[i].slot == slot) {
      pd = findpci(0, pirqmap[i].bus, pirqmap[i].dev>>3, pirqmap[i].dev&7);
      printf("found pirq: %d = %.2x:%.2x.%x  %s\n",
	     slot, pirqmap[i].bus,
	     pirqmap[i].dev >> 3,
	     pirqmap[i].dev & 7,
	     devtype(pd));
    }
  }
  return 0;
}

int main(int argc, char *argv[])
{
  char path[PATH_MAX];
  FILE *fp;
  char  line[256];
  int seg,bus,dev,fun;
  int type,id, enabled, stype;
  struct pcidev *pd, *ld;
  char *rp;

  TAILQ_INIT(&pcilist);
  if (argc < 2)
    return;

  /*===================================================================
   * Parse lspci
   *===================================================================*/
  snprintf(path, sizeof(path), "%s/lspci.txt", argv[1]);
  if ((fp = fopen(path, "r")) != NULL) {
    while (fgets(line, sizeof(line), fp) != NULL) {
      if (sscanf(line, "%2x:%2x.%x", &bus, &dev, &fun) == 3) {
	pd = addpci(0, bus, dev, fun);
	pd->label = strdup(line);
	if ((rp = strchr(pd->label, '\r')) != NULL)
	  *rp = 0;
	if ((rp = strchr(pd->label, '\n')) != NULL)
	  *rp = 0;
	if ((rp = strchr(line, ' ')) != NULL) {
	  if (!strncmp(rp+1, "Ethernet", 8)) {
	    printf("%s\n", pd->label);
	  }
	}
      }
      if ((rp = strstr(line, "secondary=")) != NULL) {
	sscanf(rp, "secondary=%x", &pd->sbus);
      }
    }
    fclose(fp);
  }
	
  /*===================================================================
   * Parse biosdecode
   *===================================================================*/
  printf("=================== PIRQ\n");
  snprintf(path, sizeof(path), "%s/biosdecode.txt", argv[1]);
  if ((fp = fopen(path, "r")) != NULL) {
    while (fgets(line, sizeof(line), fp) != NULL) {
      if ((rp = strstr(line, "Slot Entry ")) == NULL)
	continue;
      
      fun = id = 0;
      sscanf(rp, "Slot Entry %d: ID %2x:%2x", &type, &bus, &dev);
      if ((rp = strstr(line, "slot number")) != NULL) {
	sscanf(rp, "slot number %d", &id);
	addpirqslot(bus, dev, id);
      }
      if (verbose) {
	printf ("%s", line);
      }
    }
  }
  
  /*===================================================================
   * Parse dmidecode
   *===================================================================*/
  snprintf(path, sizeof(path), "%s/dmidecode.txt", argv[1]);
  if ((fp = fopen(path, "r")) != NULL) {
    enabled = 0;
    type = seg = bus = dev = fun = id = -2;
    stype = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
      if (!strncmp(line, "Handle ", 7)) {
	enabled = 0;
	type = seg = bus = dev = fun = id = -2;
	stype = 0;
	if ((rp = strstr(line, "DMI type")) != NULL)
	  sscanf(rp, "DMI type %d,", &type);
      }

      /* Check System Info */
      if (type == 1) {
	if ((rp = strstr(line, "Product Name:")) != NULL)
	  printf(rp);
	if ((rp = strstr(line, "Manufacturer:")) != NULL)
	  printf(rp);
      }

      /* Check System Slots */
      if (type == 9) {
	enabled = 1;
	stype = 0;
	if ((rp = strstr(line, "ID: ")) != NULL)
	  sscanf(rp, "ID: %d", &id);
	if ((rp = strstr(line, "Bus Address: ")) != NULL) {
	  sscanf(rp, "Bus Address: %4x:%2x:%2x.%1x", &seg, &bus, &dev, &fun);
	}
	if (blankline(line)) {
	  if (seg == -2) {
	    printf("lookup slot by pirq: %d\n", id);
	    findpirq(id);
	  }
	  else {
	    /* add-in slot, unknown type/instance */
	    slid = 0;
	    setslot(seg, bus, dev, fun, 0, id, -1, 0);
	  }
	}
      }
      
      /* Check Embedded Devices */
      if (type == 41) {
	if ((rp = strstr(line, "Status: Enabled")) != NULL)
	  enabled = 1;
	if (strstr(line, "Type: Ethernet"))
	  stype = 1;
	if ((rp = strstr(line, "Type Instance: ")) != NULL)
	  sscanf(rp, "Type Instance: %d", &id);
	if ((rp = strstr(line, "Bus Address: ")) != NULL) {
	  sscanf(rp, "Bus Address: %4x:%2x:%2x.%1x", &seg, &bus, &dev, &fun);
	}
	if (blankline(line) && seg != -2 && stype == 1 && enabled == 1) {
	  /* embeded device, known type/instance */
	  setslot(seg, bus, dev, fun, 5, 0, id, 0);
	}
      }
      
      if (type == 209) {
	if ((rp = strstr(line, "NIC")) != NULL) {
	  seg = 0;
	  if (sscanf(rp, "NIC %d: PCI device %x:%x.%x", &id, &bus, &dev, &fun) == 4) {
	    /* hp nic; known type, instance */
	    setslot(seg, bus, dev, fun, 5, 0, id, 0);
	  }
	}
      }

      if (type == 221) {
	if ((rp = strstr(line, "NIC")) != NULL) {
	  seg = 0;
	  if (sscanf(rp, "NIC %d: PCI device %x:%x.%x", &id, &bus, &dev, &fun) == 4) {
	    /* hp nic: known type, instance */
	    setslot(seg, bus, dev, fun, 1, 0, id, 0);
	  }
	}
      }
    }
    fclose(fp);
  }
  
  printf("showlist ===\n");
  TAILQ_FOREACH(pd, &pcilist, node) {
    if (pd->flag) {
      printf("%.4x:%.2x:%.2x.%x  %.2x slot:(%.2x %.2x) %s\n", pd->seg, pd->bus, pd->dev, pd->fun, pd->flag, pd->smbios_type, pd->smbios_index, pd->label);
    }
  }
}

