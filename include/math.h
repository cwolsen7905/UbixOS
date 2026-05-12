#pragma once

#ifdef __cplusplus
extern "C" {
#endif

double sqrt(double val);
double atan(double val);
double atan2(double y, double x);
double sin(double x);
double cos(double x);
double tan(double x);
double exp(double x);
double log(double x);
double log2(double x);
double log10(double x);
double pow(double x, double y);
double fabs(double x);
double ceil(double x);
double floor(double x);
double fmod(double x, double y);
double ldexp(double x, int exp);
double frexp(double x, int *exp);
double modf(double x, double *iptr);

#ifdef __cplusplus
}
#endif
