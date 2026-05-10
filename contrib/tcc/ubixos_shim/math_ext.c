/*
 * Missing math functions for TCC on UbixOS
 * Copyright (c) 2002-2026 The UbixOS Project.
 */

double ldexp(double x, int exp)
{
	/* multiply x by 2^exp */
	if (exp >= 0) {
		double m = 1.0;
		while (exp-- > 0) m *= 2.0;
		return x * m;
	} else {
		double m = 1.0;
		while (exp++ < 0) m *= 2.0;
		return x / m;
	}
}

double frexp(double x, int *exp)
{
	/* stub — return x unchanged with exp=0 */
	if (exp) *exp = 0;
	return x;
}

double modf(double x, double *iptr)
{
	double i = (double)(long long)x;
	if (iptr) *iptr = i;
	return x - i;
}
