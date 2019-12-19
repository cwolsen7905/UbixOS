
char *toupper(char *s)
{
	char *c = '\0';
	while(*s++)
		if(*s >= 'a' && *s <= 'z')
			*s += 'A' - 'a';
	
	return c;
}
