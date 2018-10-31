.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)

STRING		=strcat.o		\
				 strchr.o		\
				 strcmp.o		\
				 strcpy.o		\
				 strcspn.o		\
				 stricmp.o		\
				 strlen.o		\
				 strncmp.o		\
				 strncpy.o		\
				 strnicmp.o		\
				 strnpst.o		\
				 strnset.o		\
				 strpbrk.o		\
				 strpst.o		\
				 strrchr.o		\
				 strrev.o		\
				 strset.o		\
				 strspn.o		\
				 strstr.o		\
				 tolower.o		\
				 toupper.o

# This will be one level above where we are now
OBJ_DIR		=objects

OBJS			=$(STRING)

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\strcat.o
	del ..\$(OBJ_DIR)\strchr.o
	del ..\$(OBJ_DIR)\strcmp.o
	del ..\$(OBJ_DIR)\strcpy.o
	del ..\$(OBJ_DIR)\strcspn.o
	del ..\$(OBJ_DIR)\stricmp.o
	del ..\$(OBJ_DIR)\strlen.o
	del ..\$(OBJ_DIR)\strncmp.o
	del ..\$(OBJ_DIR)\strncpy.o
	del ..\$(OBJ_DIR)\strnicmp.o
	del ..\$(OBJ_DIR)\strnpst.o
	del ..\$(OBJ_DIR)\strnset.o
	del ..\$(OBJ_DIR)\strpbrk.o
	del ..\$(OBJ_DIR)\strpst.o
	del ..\$(OBJ_DIR)\strrchr.o
	del ..\$(OBJ_DIR)\strrev.o
	del ..\$(OBJ_DIR)\strset.o
	del ..\$(OBJ_DIR)\strspn.o
	del ..\$(OBJ_DIR)\strstr.o
	del ..\$(OBJ_DIR)\tolower.o
	del ..\$(OBJ_DIR)\toupper.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
strcat.o:	strcat.c		$(MAKEDEP)
strchr.o:	strchr.c		$(MAKEDEP)
strcmp.o:	strcmp.c		$(MAKEDEP)
strcpy.o:	strcpy.c		$(MAKEDEP)
strcspn.o:	strcspn.c	$(MAKEDEP)
stricmp.o:	stricmp.c	$(MAKEDEP)
strlen.o:	strlen.c		$(MAKEDEP)
strcmp.o:	strcmp.c		$(MAKEDEP)
strncpy.o:	strncpy.c	$(MAKEDEP)
strnicmp.o:	strnicmp.c	$(MAKEDEP)
strnpst.o:	strnpst.c	$(MAKEDEP)
strnset.o:	strnset.c	$(MAKEDEP)
strpbrk.o:	strpbrk.c	$(MAKEDEP)
strpst.o:	strpst.c		$(MAKEDEP)
strrchr.o:	strrchr.c	$(MAKEDEP)
strrev.o:	strrev.c		$(MAKEDEP)
strset.o:	strset.c		$(MAKEDEP)
strspn.o:	strspn.c		$(MAKEDEP)
strstr.o:	strstr.c		$(MAKEDEP)
tolower.o:	tolower.c	$(MAKEDEP)
toupper.o:	toupper.c	$(MAKEDEP)

