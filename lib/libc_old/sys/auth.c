#include <stdlib.h>

typedef 	struct _UbixUser UbixUser;
struct _UbixUser
{
	char *username;
	char *password;
	int uid;
	int gid;
	char *home;
	char *shell;
};


int auth(char *username, char *password)
{
	UbixUser *uu;

	uu = malloc(sizeof *uu);
	uu->uid = -1;
	uu->username = username;
	uu->password = password;

	asm volatile(
	"int %0\n"
	: : "i" (0x80), "a" (35), "b" (uu)
	);
	if(uu->uid == -1)
		return 0;
	return 1;
}
