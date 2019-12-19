#ifndef _CHAR_H
#define _CHAR_H

#include <stdbool.h>

long atoi(const char *nptr);
bool isalnum(const char c);
bool isalpha(const char c);
bool isascii(const unsigned char c);
bool iscsym(const char c);
bool iscsymf(const char c);
bool isctrl(const char c);
bool isdigit(const char c);
bool isgraph(const unsigned char c);
bool islowwer(const char c);
bool isprint(const char c);
bool ispunct(const char c);
bool isspace(const char c);
bool isupper(const char c);
bool isxdigit(const char c);
int tolower(int c);
int toupper(int c);

#endif /* !defined(_CHAR_H) */
