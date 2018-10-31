#include <sys/types.h>

#define N_BIOS_GEOM 8
 
struct bootinfo {
        u_int32_t       bi_version;
        u_int32_t       bi_kernelname;          /* represents a char * */
        u_int32_t       bi_nfs_diskless;        /* struct nfs_diskless * */
                                /* End of fields that are always present. */
#define bi_endcommon    bi_n_bios_used
        u_int32_t       bi_n_bios_used;
        u_int32_t       bi_bios_geom[N_BIOS_GEOM];
        u_int32_t       bi_size;
        u_int8_t        bi_memsizes_valid;
        u_int8_t        bi_bios_dev;            /* bootdev BIOS unit number */
        u_int8_t        bi_pad[2];
        u_int32_t       bi_basemem;
        u_int32_t       bi_extmem;
        u_int32_t       bi_symtab;              /* struct symtab * */
        u_int32_t       bi_esymtab;             /* struct symtab * */
                                /* Items below only from advanced bootloader */
        u_int32_t       bi_kernend;             /* end of kernel space */
        u_int32_t       bi_envp;                /* environment */
        u_int32_t       bi_modulep;             /* preloaded modules */
        uint32_t        bi_memdesc_version;     /* EFI memory desc version */
        uint64_t        bi_memdesc_size;        /* sizeof EFI memory desc */
        uint64_t        bi_memmap;              /* pa of EFI memory map */
        uint64_t        bi_memmap_size;         /* size of EFI memory map */
        uint64_t        bi_hcdp;                /* DIG64 HCDP table */
        uint64_t        bi_fpswa;               /* FPSWA interface */
        uint64_t        bi_systab;              /* pa of EFI system table */
};
