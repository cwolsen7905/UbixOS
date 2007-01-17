#include <string.h> /* for memset */
#include "crypt.h"
#include <sys/cdefs.h>

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

desresp *
des_crypt_1(desargs *argp, CLIENT *clnt)
{
	static desresp clnt_res;

	memset((char *)&clnt_res, 0, sizeof (clnt_res));
	if (clnt_call(clnt, DES_CRYPT,
		(xdrproc_t) xdr_desargs, (caddr_t) argp,
		(xdrproc_t) xdr_desresp, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}
