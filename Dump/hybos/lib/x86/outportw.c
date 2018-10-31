void outportw(int port, unsigned short data)
{
	__asm__ __volatile__
	(
		"outw %%ax, %%dx\n\t" 
		:
		: "a" (data), "d" (port)
	);
}
