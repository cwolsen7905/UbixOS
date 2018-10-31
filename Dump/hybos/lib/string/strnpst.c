
char *strnpst(char *s, const char *t, unsigned n)
{
	while(*t && n)
	{
		*(s++) = *(t++);
		n--;
	};

	return s;
}
