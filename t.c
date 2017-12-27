#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define PAGE_SHIFT      12              /* LOG2(PAGE_SIZE) */
#define PAGE_SIZE       (1<<PAGE_SHIFT) /* bytes/page */
#define PAGE_MASK       (PAGE_SIZE-1)

#define O_FILES 64

struct thread {
  int         td_retval[2];
  void *   o_files[O_FILES];
  char *      vm_daddr;
  int32_t     vm_dsize;
  };

#define PAD_(t) (sizeof(register_t) <= sizeof(t) ? \
                0 : sizeof(register_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define PADL_(t)        0
#define PADR_(t)        PAD_(t)
#else
#define PADL_(t)        PAD_(t)
#define PADR_(t)        0
#endif


struct freebsd6_mmap_args {
        char addr_l_[PADL_(caddr_t)]; caddr_t addr; char addr_r_[PADR_(caddr_t)];
        char len_l_[PADL_(size_t)]; size_t len; char len_r_[PADR_(size_t)];
        char prot_l_[PADL_(int)]; int prot; char prot_r_[PADR_(int)];
        char flags_l_[PADL_(int)]; int flags; char flags_r_[PADR_(int)];
        char fd_l_[PADL_(int)]; int fd; char fd_r_[PADR_(int)];
        char pad_l_[PADL_(int)]; int pad; char pad_r_[PADR_(int)];
        char pos_l_[PADL_(off_t)]; off_t pos; char pos_r_[PADR_(off_t)];
};

extern char etext, edata, end;

/* GDT Descriptor */
struct gdtDescriptor {
    unsigned short limitLow; /* Limit 0..15    */
    unsigned short baseLow; /* Base  0..15    */
    unsigned char baseMed; /* Base  16..23   */
    unsigned char access; /* Access Byte    */
    unsigned int limitHigh :4; /* Limit 16..19   */
    unsigned int granularity :4; /* Granularity    */
    unsigned char baseHigh; /* Base 24..31    */
}__attribute__ ((packed));

struct gdtGate {
    unsigned short offsetLow; /* Offset 0..15  */
    unsigned short selector; /* Selector      */
    unsigned short access; /* Access Flags  */
    unsigned short offsetHigh; /* Offset 16..31 */
}__attribute__ ((packed));

union descriptorTableUnion {
    struct gdtDescriptor descriptor; /* Normal descriptor */
    struct gdtGate gate; /* Gate descriptor */
    unsigned long dummy; /* Any other info */
};

#define ubixDescriptorTable(name, length) union descriptorTableUnion name[length] =
#define ubixStandardDescriptor(base, limit, control) {.descriptor = {(limit & 0xffff), (base & 0xffff), ((base >> 16) & 0xff), ((control + dPresent) >> 8), (limit >> 16), ((control & 0xff) >> 4), (base >> 24)}}
#define ubixGateDescriptor(offset, selector, control) {.gate = {(offset & 0xffff), selector, (control+dPresent), (offset >> 16) }}



int main() {
  int base = 0x1000;
  int size = 4096 >> PAGE_SHIFT;
  int *i = 0;
  int x1 = 0;
  int x2 = 0;
  int x3 = 0;
  int x4 = 0;

  union descriptorTableUnion *LDT;

  printf("sizeof(long) - %i\n", sizeof(long));
  printf("sizeof(uint32_t) - %i\n", sizeof(uint32_t));
  printf("end: 0x%X, etext: 0x%X, edata: 0x%X\n", &end, &etext, &edata);

  asm(
    "movl  %%gs:0,%%eax\n"
    "movl  %%eax,%0\n"
    : "=g" (x1)
    :
    : "eax"
  );

  i = (int *)x1;
  printf("GS-i: [0x%X]\n", i);
  printf("GS-*i: [0x%X]\n", *i);

  asm(
    "movl %%gs:4,%%eax\n"
    "movl %%eax, %0\n"
    : "=g" (x2)
    :
    : "eax"
  );

  i = (int *)x2;
  printf("GS-i(2): [0x%X]\n", i);
  printf("GS-*i(2): [0x%X]\n", *i);

  asm(
    "movl %%gs:8,%%eax\n"
    "movl %%eax, %0\n"
    : "=g" (x3)
    :
    : "eax"
  );

  i = (int *)x3;
  printf("GS-i(3): [0x%X]\n", i);
  //printf("GS-*i(3): [0x%X]\n", *i);

  printf("x1: 0x%X, x2: 0x%X, x3: 0x%X, x2 - x1: 0x%X, (x2-x1)/4: 0x%X, Entries: %i\n", x1, x2 , x3, x2 - x1, (x2 - x1)/4, (x2-x1)/8);

  i = (int *)x1;

/*
  for (int z = 0; z < (x2 - x1)/4;z++) {
    printf("V: [0x%X], ", i[z]);
  }
*/

  LDT = ( union descriptorTableUnion *)x1;

  printf("\n");

  printf("LDT[0].descriptor.baseLow: 0x%X\n", LDT[0].descriptor.baseLow);
  printf("LDT[0].descriptor.baseMed: 0x%X\n", LDT[0].descriptor.baseMed);
  printf("LDT[0].descriptor.baseHigh: 0x%X\n", LDT[0].descriptor.baseHigh);

  printf("LDT[0].descriptor.limitLow: 0x%X\n", LDT[0].descriptor.limitLow);
  printf("LDT[0].descriptor.limitHigh: 0x%X\n", LDT[0].descriptor.limitHigh);
  printf("LDT[0]-Limit: [0x%X]\n", LDT[0].descriptor.limitLow + (LDT[0].descriptor.limitHigh << 16));

  printf("LDT[0].descriptor.baseLow: 0x%X\n", LDT[0].descriptor.baseLow);
  printf("LDT[0].descriptor.baseMed: 0x%X\n", LDT[0].descriptor.baseMed << 16);
  printf("LDT[0].descriptor.baseHigh: 0x%X\n", LDT[0].descriptor.baseHigh << 24);
  printf("LDT[0]-Base: [0x%X]\n", LDT[0].descriptor.baseLow + (LDT[0].descriptor.baseMed << 16) + (LDT[0].descriptor.baseHigh << 24));

  printf("LDT[0].descriptor.access: 0x%X\n", LDT[0].descriptor.access);
  printf("LDT[0].descriptor.granularity: 0x%X\n", LDT[0].descriptor.granularity);

  printf("LDT[500].descriptor.baseLow: 0x%X\n", LDT[500].descriptor.baseLow);
  printf("LDT[500].descriptor.baseMed: 0x%X\n", LDT[500].descriptor.baseMed);
  printf("LDT[500].descriptor.baseHigh: 0x%X\n", LDT[500].descriptor.baseHigh);

  printf("LDT[500].descriptor.baseLow: 0x%X\n", LDT[500].descriptor.baseLow);
  printf("LDT[500].descriptor.baseMed: 0x%X\n", LDT[500].descriptor.baseMed << 16);
  printf("LDT[500].descriptor.baseHigh: 0x%X\n", LDT[500].descriptor.baseHigh << 24);
  printf("LDT[500]-Base: [0x%X]\n", LDT[500].descriptor.baseLow + (LDT[500].descriptor.baseMed << 16) + (LDT[500].descriptor.baseHigh << 24));

  printf("LDT[500].descriptor.access: 0x%X\n", LDT[500].descriptor.access);
  printf("LDT[500].descriptor.granularity: 0x%X\n", LDT[500].descriptor.granularity);

  printf("LDT[501]-Base: [0x%X]\n", LDT[501].descriptor.baseLow + (LDT[501].descriptor.baseMed << 16) + (LDT[501].descriptor.baseHigh << 24));

  printf("LDT[501].descriptor.access: 0x%X\n", LDT[501].descriptor.access);
  printf("LDT[501].descriptor.granularity: 0x%X\n", LDT[501].descriptor.granularity);


  asm(
    "movl %%gs:12,%%eax\n"
    "movl %%eax, %0\n"
    : "=g" (x4)
    :
    : "eax"
  );

  i = (int *)x4;
  printf("GS-i(3): [0x%X]\n", i);
  //printf("GS-*i(3): [0x%X]\n", *i);


  asm(
    "movl %%fs,%%eax\n"
    "movl %%eax, %0\n"
    : "=g" (x1)
    :
    : "eax"
  );

  printf("FS: [0x%X]\n", x1);

  asm(
    "movl %%ebp, %0\n"
    : "=g" (x1)
    :
    : "eax"
  );

  printf("EBP: [0x%X]\n", x1);

  asm(
    "movl %%esp, %0\n"
    : "=g" (x1)
    :
    : "eax"
  );

  printf("ESP: [0x%X]\n", x1);
  exit(0);






  printf("%i %i %i %i %i %i %i", sizeof(void *), sizeof(size_t), sizeof(int), sizeof(int), sizeof(int), sizeof(off_t), sizeof(struct freebsd6_mmap_args));
  return(0);

 // char buffer[0x10000];

//  mmap(&buffer, 0x10, 0x20, 0x30 , 0x5 ,0x0);


  printf("[%i]\n", *i);

  struct thread td;

  td.vm_daddr = (char *)0x1000;
  td.vm_dsize = 4096 >> PAGE_SHIFT;

  //td = malloc(sizeof(struct thread));
  printf("Base: 0x%X, Size: 0x%X, Shift: 0x%X\n", base, size, (base + (size << PAGE_SHIFT)));
  printf("Base: 0x%X, Size: 0x%X, Shift: 0x%X\n", (uint32_t)td.vm_daddr, td.vm_dsize, (uint32_t)td.vm_daddr + (td.vm_dsize << PAGE_SHIFT));

  printf("sizeof(int *) = %i\n", sizeof(int *));

  return(0);
}
