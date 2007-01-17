#include "yp.h"
#include <sys/cdefs.h>

bool_t
xdr_ypstat(register XDR *xdrs, ypstat *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypxfrstat(register XDR *xdrs, ypxfrstat *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_domainname(register XDR *xdrs, domainname *objp)
{

	if (!xdr_string(xdrs, objp, YPMAXDOMAIN))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_mapname(register XDR *xdrs, mapname *objp)
{

	if (!xdr_string(xdrs, objp, YPMAXMAP))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_peername(register XDR *xdrs, peername *objp)
{

	if (!xdr_string(xdrs, objp, YPMAXPEER))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_keydat(register XDR *xdrs, keydat *objp)
{

	if (!xdr_bytes(xdrs, (char **)&objp->keydat_val, (u_int *) &objp->keydat_len, YPMAXRECORD))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_valdat(register XDR *xdrs, valdat *objp)
{

	if (!xdr_bytes(xdrs, (char **)&objp->valdat_val, (u_int *) &objp->valdat_len, YPMAXRECORD))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypmap_parms(register XDR *xdrs, ypmap_parms *objp)
{

	if (!xdr_domainname(xdrs, &objp->domain))
		return (FALSE);
	if (!xdr_mapname(xdrs, &objp->map))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->ordernum))
		return (FALSE);
	if (!xdr_peername(xdrs, &objp->peer))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypreq_key(register XDR *xdrs, ypreq_key *objp)
{

	if (!xdr_domainname(xdrs, &objp->domain))
		return (FALSE);
	if (!xdr_mapname(xdrs, &objp->map))
		return (FALSE);
	if (!xdr_keydat(xdrs, &objp->key))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypreq_nokey(register XDR *xdrs, ypreq_nokey *objp)
{

	if (!xdr_domainname(xdrs, &objp->domain))
		return (FALSE);
	if (!xdr_mapname(xdrs, &objp->map))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypreq_xfr(register XDR *xdrs, ypreq_xfr *objp)
{

	if (!xdr_ypmap_parms(xdrs, &objp->map_parms))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->transid))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->prog))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->port))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypresp_val(register XDR *xdrs, ypresp_val *objp)
{

	if (!xdr_ypstat(xdrs, &objp->stat))
		return (FALSE);
	if (!xdr_valdat(xdrs, &objp->val))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypresp_key_val(register XDR *xdrs, ypresp_key_val *objp)
{

	if (!xdr_ypstat(xdrs, &objp->stat))
		return (FALSE);
	if (!xdr_valdat(xdrs, &objp->val))
		return (FALSE);
	if (!xdr_keydat(xdrs, &objp->key))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypresp_master(register XDR *xdrs, ypresp_master *objp)
{

	if (!xdr_ypstat(xdrs, &objp->stat))
		return (FALSE);
	if (!xdr_peername(xdrs, &objp->peer))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypresp_order(register XDR *xdrs, ypresp_order *objp)
{

	if (!xdr_ypstat(xdrs, &objp->stat))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->ordernum))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypresp_all(register XDR *xdrs, ypresp_all *objp)
{

	if (!xdr_bool(xdrs, &objp->more))
		return (FALSE);
	switch (objp->more) {
	case TRUE:
		if (!xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val))
			return (FALSE);
		break;
	case FALSE:
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresp_xfr(register XDR *xdrs, ypresp_xfr *objp)
{

	if (!xdr_u_int(xdrs, &objp->transid))
		return (FALSE);
	if (!xdr_ypxfrstat(xdrs, &objp->xfrstat))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypmaplist(register XDR *xdrs, ypmaplist *objp)
{

	if (!xdr_mapname(xdrs, &objp->map))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->next, sizeof (ypmaplist), (xdrproc_t) xdr_ypmaplist))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypresp_maplist(register XDR *xdrs, ypresp_maplist *objp)
{

	if (!xdr_ypstat(xdrs, &objp->stat))
		return (FALSE);
	if (!xdr_pointer(xdrs, (char **)&objp->maps, sizeof (ypmaplist), (xdrproc_t) xdr_ypmaplist))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_yppush_status(register XDR *xdrs, yppush_status *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_yppushresp_xfr(register XDR *xdrs, yppushresp_xfr *objp)
{

	if (!xdr_u_int(xdrs, &objp->transid))
		return (FALSE);
	if (!xdr_yppush_status(xdrs, &objp->status))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypbind_resptype(register XDR *xdrs, ypbind_resptype *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypbind_binding(register XDR *xdrs, ypbind_binding *objp)
{

	if (!xdr_opaque(xdrs, objp->ypbind_binding_addr, 4))
		return (FALSE);
	if (!xdr_opaque(xdrs, objp->ypbind_binding_port, 2))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypbind_resp(register XDR *xdrs, ypbind_resp *objp)
{

	if (!xdr_ypbind_resptype(xdrs, &objp->ypbind_status))
		return (FALSE);
	switch (objp->ypbind_status) {
	case YPBIND_FAIL_VAL:
		if (!xdr_u_int(xdrs, &objp->ypbind_resp_u.ypbind_error))
			return (FALSE);
		break;
	case YPBIND_SUCC_VAL:
		if (!xdr_ypbind_binding(xdrs, &objp->ypbind_resp_u.ypbind_bindinfo))
			return (FALSE);
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypbind_setdom(register XDR *xdrs, ypbind_setdom *objp)
{

	if (!xdr_domainname(xdrs, &objp->ypsetdom_domain))
		return (FALSE);
	if (!xdr_ypbind_binding(xdrs, &objp->ypsetdom_binding))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->ypsetdom_vers))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypreqtype(register XDR *xdrs, ypreqtype *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_ypresptype(register XDR *xdrs, ypresptype *objp)
{

	if (!xdr_enum(xdrs, (enum_t *)objp))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_yprequest(register XDR *xdrs, yprequest *objp)
{

	if (!xdr_ypreqtype(xdrs, &objp->yp_reqtype))
		return (FALSE);
	switch (objp->yp_reqtype) {
	case YPREQ_KEY:
		if (!xdr_ypreq_key(xdrs, &objp->yprequest_u.yp_req_keytype))
			return (FALSE);
		break;
	case YPREQ_NOKEY:
		if (!xdr_ypreq_nokey(xdrs, &objp->yprequest_u.yp_req_nokeytype))
			return (FALSE);
		break;
	case YPREQ_MAP_PARMS:
		if (!xdr_ypmap_parms(xdrs, &objp->yprequest_u.yp_req_map_parmstype))
			return (FALSE);
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresponse(register XDR *xdrs, ypresponse *objp)
{

	if (!xdr_ypresptype(xdrs, &objp->yp_resptype))
		return (FALSE);
	switch (objp->yp_resptype) {
	case YPRESP_VAL:
		if (!xdr_ypresp_val(xdrs, &objp->ypresponse_u.yp_resp_valtype))
			return (FALSE);
		break;
	case YPRESP_KEY_VAL:
		if (!xdr_ypresp_key_val(xdrs, &objp->ypresponse_u.yp_resp_key_valtype))
			return (FALSE);
		break;
	case YPRESP_MAP_PARMS:
		if (!xdr_ypmap_parms(xdrs, &objp->ypresponse_u.yp_resp_map_parmstype))
			return (FALSE);
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}
