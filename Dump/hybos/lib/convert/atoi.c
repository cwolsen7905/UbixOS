#include <char.h>

long atoi(const char *nptr)
{
	int c;			/* current char */
	long total;		/* current total */
	int sign;		/* if '-', then negative, otherwise positive */

	/* skip whitespace */
	while(isspace((int)(unsigned char)*nptr))
		++nptr;

	c = (int)(unsigned char)*nptr++;
	sign = c; /* save sign indication */
	
	/* skip sign */
	if(c == '-' || c == '+')
		c = (int)(unsigned char)*nptr++;

	total = 0;

	while(isdigit(c))
	{
		total = 10 * total + (c - '0');		/* accumulate digit */
		c = (int)(unsigned char)*nptr++;		/* get next char */
	}

	/* return result, negated if necessary */
	if(sign == '-')
		return -total;
	else
		return total;
}
