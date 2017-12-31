#include "crypt.h"
#include <sys/cdefs.h>

bool_t
xdr_des_dir(register XDR *xdrs, des_dir *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_des_mode(register XDR *xdrs, des_mode *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_desargs(register XDR *xdrs, desargs *objp)
{

	if (!xdr_vector(xdrs, (char *)objp->des_key, 8,
		sizeof (u_char), (xdrproc_t) xdr_u_char))
		return (FALSE);
	if (!xdr_des_dir(xdrs, &objp->des_dir))
		return (FALSE);
	if (!xdr_des_mode(xdrs, &objp->des_mode))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->des_ivec, 8,
		sizeof (u_char), (xdrproc_t) xdr_u_char))
		return (FALSE);
	if (!xdr_bytes(xdrs, (char **)&objp->desbuf.desbuf_val, (u_int *) &objp->desbuf.desbuf_len, ~0))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_desresp(register XDR *xdrs, desresp *objp)
{

	if (!xdr_bytes(xdrs, (char **)&objp->desbuf.desbuf_val, (u_int *) &objp->desbuf.desbuf_len, ~0))
		return (FALSE);
	if (!xdr_vector(xdrs, (char *)objp->des_ivec, 8,
		sizeof (u_char), (xdrproc_t) xdr_u_char))
		return (FALSE);
	if (!xdr_int(xdrs, &objp->stat))
		return (FALSE);
	return (TRUE);
}
