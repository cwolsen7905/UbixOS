#include <stdint.h>
#include "../kernel/bootlog.h"

extern console_t _vc[];

unsigned *buffer;
unsigned *bufferIterator;

#define PAGESIZE	4096

char ts[4096*3];
char td[4096*3];

void _mm_physical_init(void);
unsigned _mm_physical_alloc(void);
void _mm_physical_free(unsigned page);
void _mm_page_copy_byte(uint32_t dest, uint32_t src);
void _mm_page_copy_word(uint32_t dest, uint32_t src);
void _mm_page_copy_dword(uint32_t dest, uint32_t src);
void _mm_virtual_init(void);

void _mm_init(void)
{
	klog("init", "Initializing memory management", K_KLOG_PENDING, &_vc[0]);
	_mm_physical_init();
	_mm_virtual_init();
	klog((void *)0, (void *)0, K_KLOG_SUCCESS, &_vc[0]);
}

void _mm_physical_init(void)
{
	unsigned i;
	unsigned size = 16 * 1024 * 1024;

	size /= PAGESIZE;
	size++;
	size /= 32;

	buffer = (unsigned *)0x40000;
	bufferIterator = (unsigned *)0x40000;

	for(i = 0; i < 72; i++)
		buffer[i] = 0xFFFFFFFF;

	for(i = 72; i < size; i++)
		buffer[i] = 0x00000000;
}

unsigned _mm_physical_alloc(void)
{
	unsigned mask = 0x00000001;
	unsigned bit = 0;

	/**
	 * Search for a free space
	 */
	while(*bufferIterator == 0xFFFFFFFF)
		bufferIterator++;

	/**
	 * Search for a bit that indicates a free page
	 */
	while(*bufferIterator & mask)
	{
		mask <<= 1;
		bit++;
	}

	*bufferIterator |= mask;

	return 32 * (bufferIterator - buffer) + bit;
}

void _mm_physical_free(unsigned page)
{
	buffer[page >> 5] &= ~(1 << (page & 0x1F));	/* confused yet?!? */
}

void _mm_virtual_init(void)
{
}

void _mm_page_copy_byte(uint32_t dest, uint32_t src)
{
	__asm__ __volatile__
		(
		 	"cld;"
			"rep; movsb;"
			:
			: "c" (1024*1024), "D" (dest), "S" (src)
			: "memory"
		);
}

void _mm_page_copy_word(uint32_t dest, uint32_t src)
{
	__asm__ __volatile__
		(
		 	"cld;"
			"rep; movsw;"
			: 
			: "c" (512*1024), "D" (dest), "S" (src)
			: "memory"
		);
}

void _mm_page_copy_dword(uint32_t dest, uint32_t src)
{
	__asm__ __volatile__
		(
		 	"cld;"
			"rep; movsl;"
			: 
			: "c" (256*1024), "D" (dest), "S" (src)
			: "memory"
		);
}

