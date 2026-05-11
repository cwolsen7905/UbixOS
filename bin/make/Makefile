# Makefile for UbixOS minimal make
include ../../Makefile.incl
include ../../bin/Makefile.incl

PROG = make
OBJS = make.o

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	    -o $(BUILD_DIR)/bin/$(PROG) \
	    $(STARTUP) $(LIBRARIES) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -Wall -c -o $@ $<

clean:
	$(REMOVE) $(OBJS)

.for _d in ${OBJS:.o=.d}
.sinclude "${_d}"
.endfor