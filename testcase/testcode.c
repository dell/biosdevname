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

struct pcidev
{
  int 			seg,bus,dev,fun,sbus;
  TAILQ_ENTRY(pcidev)  	node;
};

TAILQ_HEAD(,pcidev) pcilist;

#define zmalloc(x) calloc(1,x)

static int cmppci(const void *a, const void *b)
{
  const struct pcidev *pa = a;
  const struct pcidev *pb = b;

  if (pa->seg != pb->seg)
    return pa->seg < pb->seg ? -1 : 1;
  if (pa->bus != pb->bus)
    return pa->bus < pb->bus ? -1 : 1;
  if (pa->dev != pb->dev)
    return pa->dev < pb->dev ? -1 : 1;
  if (pa->fun != pb->fun)
    return pa->fun < pb->fun ? -1 : 1;
  return 0;
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

int main(int argc, char *argv[])
{
  char path[PATH_MAX];
  FILE *fp;
  char  line[256];
  int seg,bus,dev,fun;
  int type,id, enabled;
  struct pcidev *pd;
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
      }
      if ((rp = strstr(line, "subordinate=")) != NULL) {
	sscanf(rp, "subordinate=%x", &pd->sbus);
	printf("%.4x:%.2x:%.2x.%x = %x\n", pd->seg, pd->bus, pd->dev, pd->fun, pd->sbus);
      }
    }
    fclose(fp);
  }
	
  /*===================================================================
   * Parse dmidecode
   *===================================================================*/
  snprintf(path, sizeof(path), "%s/dmidecode.txt", argv[1]);
  if ((fp = fopen(path, "r")) != NULL) {
    enabled = 0;
    type = seg = bus = dev = fun = id = -2;
    while (fgets(line, sizeof(line), fp) != NULL) {
      if (!strncmp(line, "Handle ", 7)) {
	enabled = 0;
	type = seg = bus = dev = fun = id = -2;
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
	if ((rp = strstr(line, "ID: ")) != NULL)
	  sscanf(rp, "ID: %d", &id);
	if ((rp = strstr(line, "Bus Address: ")) != NULL)
	  sscanf(rp, "Bus Address: %4x:%2x:%2x.%1x", &seg, &bus, &dev, &fun);
      }

      /* Check Embedded Devices */
      if (type == 41) {
	if ((rp = strstr(line, "Status: Enabled")) != NULL)
	  enabled = 1;
	if ((rp = strstr(line, "Type Instance: ")) != NULL)
	  sscanf(rp, "Type Instance: %d", &id);
	if ((rp = strstr(line, "Bus Address: ")) != NULL)
	  sscanf(rp, "Bus Address: %4x:%2x:%2x.%1x", &seg, &bus, &dev, &fun);
      }
      if ((type == 9 || type == 41) && blankline(line) && enabled) {
	printf("--- %s %.4x:%.2x:%.2x.%x %x %s\n", type == 9 ? "slot" : "onboard", seg, bus, dev, fun, id, findpci(seg, bus, dev, fun) ? " **" : "");
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
      }
      printf("%.4x:%.2x:%.2x.%x   slot %x %s\n", 0, bus, dev, fun, id, findpci(0, bus, dev, -1) ? " **" : "");
    }
  }
  
  printf("showlist ===\n");
  TAILQ_FOREACH(pd, &pcilist, node) {
    printf("%.4x:%.2x:%.2x.%x\n", pd->seg, pd->bus, pd->dev, pd->fun);
    if (pd->sbus != -1 && findpci(0, pd->sbus, -1, -1) != NULL)
      printf("  children\n");
  }
}

