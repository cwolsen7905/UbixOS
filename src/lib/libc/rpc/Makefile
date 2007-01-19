# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../Makefile.inc
#include ../../Makefile.inc
#include ../Makefile.inc
include ../libc.inc

#INCLUDES += -I../locale/
#CFLAGS += -DPORTMAP

#Objects
OBJS = auth_des.o auth_none.o auth_time.o auth_unix.o authdes_prot.o authunix_prot.o bindresvport.o clnt_bcast.o clnt_dg.o clnt_generic.o clnt_perror.o clnt_raw.o clnt_simple.o clnt_vc.o crypt_client.o des_crypt.o des_soft.o getnetconfig.o getnetpath.o getpublickey.o getrpcent.o getrpcport.o key_call.o key_prot_xdr.o mt_misc.o netname.o netnamer.o pmap_clnt.o pmap_getmaps.o pmap_getport.o pmap_prot.o pmap_prot2.o pmap_rmt.o rpc_callmsg.o rpc_commondata.o rpc_dtablesize.o rpc_generic.o rpc_prot.o rpc_soc.o rpcb_clnt.o rpcb_prot.o rpcb_st_xdr.o rpcdname.o rtime.o svc.o svc_auth.o svc_auth_des.o svc_auth_unix.o svc_dg.o svc_generic.o svc_raw.o svc_run.o svc_simple.o svc_vc.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

