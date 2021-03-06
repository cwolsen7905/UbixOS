#include <stdint.h>

void i32toa(int32_t value, char *string, uint8_t radix)
{
	char *i, *s, t, d;

	i = string;

	if(value < 0)
	{
		*i++ = '-';
		value = -value;
	}

	s = i;

	do
	{
		d = value % radix;
		value /= radix;

		if (d > 9)
			*i++ = d + 'A' - 10;
		else
			*i++ = d + '0';
	} while (value > 0);
	
	*i-- = '\0';

	do 
	{
		t = *i;
		*i = *s;
		*s = t;
	
		--i;
		++s;
	} while (s < i);
}
