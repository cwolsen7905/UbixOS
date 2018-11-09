unsigned short inportw(int port)
{
	register unsigned short r;

	__asm__ __volatile__
	(
		"inw %%dx, %%ax\n"
		: "=a" (r)
		: "d" (port)
	);

	return r;
}
