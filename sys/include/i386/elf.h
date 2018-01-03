#ifndef _I386_ELF_H_
#define _i386_ELF_H_ 1

#define elfExecutable 0x002
#define elfLibrary    0x003

typedef struct {
    uInt8 eIdent[16]; /* File identification. */
    uInt16 eType; /* File type. */
    uInt16 eMachine; /* Machine architecture. */
    uInt32 eVersion; /* ELF format version. */
    uInt32 eEntry; /* Entry point. */
    uInt32 ePhoff; /* Program header file offset. */
    uInt32 eShoff; /* Section header file offset. */
    uInt32 eFlags; /* Architecture-specific flags. */
    uInt16 eEhsize; /* Size of ELF header in bytes. */
    uInt16 ePhentsize; /* Size of program header entry. */
    uInt16 ePhnum; /* Number of program header entries. */
    uInt16 eShentsize; /* Size of section header entry. */
    uInt16 eShnum; /* Number of section header entries. */
    uInt16 eShstrndx; /* Section name strings section. */
} elfHeader;

typedef struct {
    uInt32 phType; /* Entry type. */
    uInt32 phOffset; /* File offset of contents. */
    uInt32 phVaddr; /* Virtual address in memory image. */
    uInt32 phPaddr; /* Physical address (not used). */
    uInt32 phFilesz; /* Size of contents in file. */
    uInt32 phMemsz; /* Size of contents in memory. */
    uInt32 phFlags; /* Access permission flags. */
    uInt32 phAlign; /* Alignment in memory and file. */
} elfProgramHeader;

typedef struct {
    uInt32 shName; /* Section name (index into the section header string table). */
    uInt32 shType; /* Section type. */
    uInt32 shFlags; /* Section flags. */
    uInt32 shAddr; /* Address in memory image. */
    uInt32 shOffset; /* Offset in file. */
    uInt32 shSize; /* Size in bytes. */
    uInt32 shLink; /* Index of a related section. */
    uInt32 shInfo; /* Depends on section type. */
    uInt32 shAddralign; /* Alignment in bytes. */
    uInt32 shEntsize; /* Size of each entry in section. */
} elfSectionHeader;

typedef struct {
    uInt32 pltOffset;
    uInt32 pltInfo;
} elfPltInfo;

typedef struct {
    uInt32 dynName;
    uInt32 dynValue;
    uInt32 dynSize;
    uInt32 dynInfo;
} elfDynSym;

typedef struct {
    uInt32 dynVal;
    uInt32 dynPtr;
} elfDynamic;

typedef struct {
    int32_t execfd;
    uint32_t phdr;
    uint32_t phent;
    uint32_t phnum;
    uint32_t pagesz;
    uint32_t base;
    uint32_t flags;
    uint32_t entry;
    uint32_t trace;
} Elf_Auxargs;

typedef struct {        /* Auxiliary vector entry on initial stack */
        int     a_type;                 /* Entry type. */
        union {
                int     a_val;          /* Integer value. */
        } a_un;
} Elf32_Auxinfo;

char *elfGetShType( int );
char *elfGetPhType( int );
char *elfGetRelType( int );
int elf_loadfile( kTask_t *p, const char *file, uint32_t *addr, uint32_t *entry );

#define ELF32_R_SYM(i)      ((i)>>8)
#define ELF32_R_TYPE(i)     ((unsigned char)(i))
#define ELF32_R_INFO(s, t)  ((s)<<8+(unsigned char)(t))

#endif
