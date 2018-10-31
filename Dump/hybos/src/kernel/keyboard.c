/**
 * keyboard.c
 *
 * Main keyboard handling routines.
 *
 * Exports:
 *  keyboard_irq();
 *  init_keyboard();
 *
 * Imports:
 *  video.c	console_t _vc[];
 *  video.c	select_vc();
 *  video.c	putch();
 *  main.c	printf();
 *  main.c	printk();
 */

//#include <conio.h>	/* key scancode definitions */
#include <keyboard.h>
#include <x86.h>		/* outportb, inportb(), etc */
#include <string.h>
#include <esh/esh.h>	/* shell commands */
#include "_krnl.h"	/* MAX_VC */
#include "bootlog.h"	/* klog() */

#define	KBD_BUF_SIZE		64

/**
 * Imports
 */
extern console_t _vc[];
void select_vc(unsigned which_vc);
void putch(unsigned c);
void printf(const char *fmt, ...);
void printk(int type, const char *fmt, ...);
void dumpheapk(void);
void testheap(void);

unsigned get_current_vc();

static int rawkey, keys[128];
static int numkeysbuffer;

static char szInBuf[KBD_BUF_SIZE];

/**
 * 0 if not set
 * 1 if make code
 * 2 if break code
 */
static int makebreak;

/**
 * reboot()
 *
 */
static void reboot(void)
{
	unsigned temp;

	disable();

	/**
	 * flush the keyboard controller
	 */
	do
	{
		temp = inportb(0x64);
		if((temp & 0x01) != 0)
		{
			(void)inportb(0x60);
			continue;
		}
	} while((temp & 0x02) != 0);

	/**
	 * now pulse the cpu reset line
	 */
	outportb(0x64, 0xFE);

	/**
	 * if that didn't work, just halt
	 */
	while(1);
}

/**
 * XXX
 *
 * I'm not even sure if we need the following functions yet,
 * however they are here just in case. Leave them alone.
 */

/**
 * _write_kb()
 * 
 */
static void _write_kb(unsigned adr, unsigned d)
{
	unsigned long t;
	unsigned s;

	for(t = 5000000L; t != 0; t--)
	{
		s = inportb(0x64);

		/**
		 * loop until 8042 input buffer is empty
		 */
		if((s & 0x02) == 0)
			break;
	}

	if(t != 0)
		outportb(adr, d);
}

/**
 * _kb_wait()
 *
 */
static inline void _kb_wait(void)
{
	int i;

	for(i = 0; i < 0x1000000; i++)
		if((inportb(0x64) & 0x02) == 0)
			return;

	printk(0, "Keyboard timeout\n");
}

/**
 * _kb_send()
 *
 */
static inline void _kb_send(unsigned char c)
{
	_kb_wait();
	outportb(c, 0x64);
}

/**
 * _translate_sc()
 *
 * Translates a scancode from the keyboard
 */
unsigned _translate_sc(unsigned k)
{
	unsigned c;
	static unsigned altk;
	unsigned donefirst = 0;

	if(k == KEY_BKSPACE)
	{
		if(numkeysbuffer - 1 < 0)
		{
			numkeysbuffer = 0;
			return 0;
		}
	}

	switch(k)
	{
		case 0xE0:
			altk = 1; c = 0; donefirst = 1; break;
		case KEY_TILDA:	/* ` or ~ */
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 126 : 126; break;
		case KEY_END: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 49; break;
		case KEY_1:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 33 : 49; break;
		case KEY_DOWN: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 50; break;
		case KEY_2:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 64 : 50; break;
		case KEY_PGDOWN: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 51; break;
		case KEY_3:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 35 : 51; break;
		case KEY_LEFT: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 52; break;
		case KEY_4:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 36 : 52; break;
		case KEYP_5: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 53; break;
		case KEY_5:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 35 : 53; break;
		case KEY_RIGHT: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 54; break;
		case KEY_6:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 94 : 54; break;
		case KEY_HOME: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 55; break;
		case KEY_7:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 38 : 55; break;
		case KEY_UP: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 56; break;
		case KEYP_ASTERISK: c = 42; break;
		case KEY_8:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 42 : 56; break;
		case KEY_PGUP: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 57; break;
		case KEY_9:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 40 : 57; break;
		case KEY_INSERT: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 48; break;
		case KEY_0:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 41 : 48; break;
		case KEYP_MINUS: c = 45; break;
		case KEY_MINUS:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 95 : 45; break;
		case KEYP_PLUS: c = 43; break;
		case KEY_PLUS:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 61 : 43; break;
		case KEY_BKSLASH:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 124 : 92; break;
		case KEY_Q:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 81 : 113; break;
		case KEY_W:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 87 : 119; break;
		case KEY_E:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 69 : 101; break;
		case KEY_R:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 82 : 114; break;
		case KEY_T:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 84 : 116; break;
		case KEY_Y:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 89 : 121; break;
		case KEY_U:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 85 : 117; break;
		case KEY_I:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 73 : 105; break;
		case KEY_O:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 79 : 111; break;
		case KEY_P:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 80 : 112; break;
		case KEY_LBRACKET:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 123 : 91; break;
		case KEY_RBRACKET:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 125 : 93; break;
		case KEY_ENTER:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 10 : 10; break;
		case KEY_A:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 65 : 97; break;
		case KEY_S:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 83 : 115; break;
		case KEY_D:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 68 : 100; break;
		case KEY_F:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 70 : 102; break;
		case KEY_G:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 71 : 103; break;
		case KEY_H:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 72 : 104; break;
		case KEY_J:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 74 : 106; break;
		case KEY_K:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 75 : 107; break;
		case KEY_L:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 76 : 108; break;
		case KEY_SEMICOLON:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 58 : 59; break;
		case KEY_QUOTE:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 34 : 39; break;
		case KEY_Z:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 90 : 122; break;
		case KEY_X:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 88 : 120; break;
		case KEY_C:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 67 : 99; break;
		case KEY_V:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 86 : 118; break;
		case KEY_B:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 66 : 98; break;
		case KEY_N:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 78 : 110; break;
		case KEY_M:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 77 : 109; break;
		case KEY_COMMA:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 60 : 44; break;
		case KEY_DEL: c = 0; if(keys[KEYP_NUMLCK] && altk == 0) c = 46; break;
		case KEY_PERIOD:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 62 : 46; break;
		case KEY_SLASH:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 63 : 47; break;
		case KEY_SPACE:
			c = (keys[KEY_LSHIFT] || keys[KEY_RSHIFT]) ? 32 : 32; break;
		case KEY_BKSPACE: c = '\b'; break; /* just for now */
		default:
			c = 0;
	}

	if(donefirst == 0)
		altk = 0;

	if(keys[KEY_CAPS])
	{
		if(keys[KEY_LSHIFT] || keys[KEY_RSHIFT])
		{
			if(c >= 'A' && c <= 'Z')
				c += 32;
		}
		else
		{
			if(c >= 'a' && c <= 'z')
				c -= 32;
		}
	}

	/**
	 * Simple shell for now
	 */
	if(c != 0 && c != '\n' && c != '\b')
	{
		if((numkeysbuffer - 1) == KBD_BUF_SIZE)
		{
			numkeysbuffer = 0;
			szInBuf[0] = '\0';
		
			szInBuf[numkeysbuffer] = c;
			numkeysbuffer++;
		}
		else
		{
			szInBuf[numkeysbuffer] = c;
			numkeysbuffer++;
		}
	}
	else if(c == '\n')
	{
		printf("\n");
		/**
		 * Make it a real string
		 */
		szInBuf[numkeysbuffer] = '\0';

		/**
		 * Process command
		 */
		processCommand(&szInBuf[0], numkeysbuffer - 1);

		/**
		 * Clear buffer
		 */
		numkeysbuffer = 0;
		szInBuf[0] = '\0';

		/**
		 * Print "line"
		 */
		printf("$ ");

		c = 0;
	}
	else if(c == '\b')
	{
		szInBuf[numkeysbuffer] = '\0';
		numkeysbuffer--;
		printf("\b \b");

		c = 0;
	}
	
	return c;
}

/**
 * handle_meta_key()
 *
 * I'll pretty this up later
 */
void handle_meta_key(unsigned k)
{
	int i;
	k = k; /* to shut gcc up */

	/**
	 * Check for the infamous three finger salute
	 */
	if((keys[KEY_RCTRL] || keys[KEY_LCTRL]) &&
			(keys[KEY_RALT] || keys[KEY_LALT]) &&
			keys[KEY_DEL])
	{
		/**
		 * FIXME
		 *
		 * This should call _send_signal()
		 */
		reboot();
	}

	/**
	 * Check for Alt + F1-F12 for virtual terminals
	 */
	for(i = 0; i < 10; i++)
	{
		if((keys[KEY_LALT] || keys[KEY_RALT]) && keys[i + KEY_F1])
		{
			select_vc(i);
			return;
		}
	}
	
	if((keys[KEY_LALT] || keys[KEY_RALT]) && keys[KEY_F11])
	{
		select_vc(10);
		return;
	}

	if((keys[KEY_LALT] || keys[KEY_RALT]) && keys[KEY_F12])
	{
		select_vc(11);
		return;
	}
}

/**
 * keyboard_irq()
 *
 * Called when a keyboard interrupt is generated.
 */
void keyboard_irq(void)
{
	register char a;
	unsigned c;
	unsigned short kbdstat;

	rawkey = inportb(0x60);
	outportb(0x61, (a=inportb(0x61)|0x82));
	outportb(0x61, a & 0x7F);
	
	/**
	 * If it's less than 0x80 then it's definatelly
	 * a make code or a repeat code
	 */
	if(rawkey < 0x80)
	{
		/**
		 * We don't want to gunk up the numlock key
		 * because we will define it's state in the
		 * break code a bit later
		 */
		if((rawkey != KEYP_NUMLCK) && (rawkey != KEY_SCRLCK) && (rawkey != KEY_CAPS))
			keys[rawkey] = 1;

		keyDown(rawkey);
	}
	else /* rawkey >= 0x80 */
	{
		if(rawkey == 0xE0)
		{
			/**
			 * It's either a make code, break code, or repeat code
			 */
			rawkey = inportb(0x60);
			outportb(0x61, (a=inportb(0x61)|0x82));
			outportb(0x61, a & 0x7F);

			if(rawkey < 0x80)
			{
				/**
				 * Ok, it's a make code or repeat code for the numeric
				 * keypad (gray keys)
				 */

				keys[rawkey] = 1;

				keyDown(rawkey);
			}
			else /* rawkey >= 0x80 */
			{
				/**
				 * It's either a make code for the numeric keypad or
				 * a break code for the numeric keypad.
				 */
				if(rawkey == 0x2A)
				{
					/**
					 * Ok, we have a make code for the numeric keypad
					 * and NUMLOCK is on. The second byte is what we
					 * want since what we have so far is this:
					 *
					 * 0xE0 0x2A
					 */
					rawkey = inportb(0x60);
					outportb(0x61, (a=inportb(0x61)|0x82));
					outportb(0x61, a & 0x7F);

					rawkey = inportb(0x60);
					outportb(0x61, (a=inportb(0x61)|0x82));
					outportb(0x61, a & 0x7F);

					keys[rawkey] = 1;

					keyDown(rawkey);
				}
				else
				{
					/**
					 * It's a break code from the numeric keypad.
					 */
					keys[rawkey] = 0;

					keyUp(rawkey);
				}
			}
		}
		else /* rawkey != 0xE0 */
		{
			/**
			 * It's a break code
			 *
			 * Make sure we toggle the numlock, scroll lock, and caps lock key.
			 */
			if(((rawkey - 0x80) == KEYP_NUMLCK) ||
					((rawkey - 0x80) == KEY_SCRLCK) ||
					((rawkey - 0x80) == KEY_CAPS))
			{
				keys[rawkey - 0x80] = !keys[rawkey - 0x80];

				kbdstat = 0;
				if(keys[KEY_SCRLCK])
					kbdstat |= 1;
				if(keys[KEYP_NUMLCK])
					kbdstat |= 2;
				if(keys[KEY_CAPS])
					kbdstat |= 4;

				_write_kb(0x60, 0xED);
				_write_kb(0x60, kbdstat);
				outportb(0x20, 0x20);

				keyUp(rawkey);
				return;
			}
			
			keys[rawkey - 0x80] = 0;

			keyUp(rawkey);
		}
	}

	c = _translate_sc(rawkey);

	if(c != 0)
		printf("%c", c);
	else
	{
		/**
		 * We need to check for meta-key-crap here
		 */
		handle_meta_key(rawkey);
	}

	//enable();
	outportb(0x20, 0x20);
}

/**
 * init_keyboard()
 *
 */
void init_keyboard(void)
{
	static unsigned char buffers[KBD_BUF_SIZE * MAX_VC];

	int i;

	//klog("init", "keyboard %2u buf, %2ub each", K_KLOG_PENDING, &_vc[0]);
	for(i = 0; i < MAX_VC; i++)
	{
		_vc[i].keystrokes.data = buffers + KBD_BUF_SIZE * i;
		_vc[i].keystrokes.size = KBD_BUF_SIZE;
	}

	for(i = 0; i < 128; i++)
		keys[i] = 0;

	makebreak = 0;
	//klog(NULL, K_KLOG_SUCCESS, &_vc[0], NULL);
	//kprintf("init_kbd: %u buffers, %u bytes each\n",
	//	MAX_VC, KBD_BUF_SIZE);
	
	//kprintf("[ Entering Runlevel 0 ].......................................................Ok");
	_vc[0].attrib = 8;
	printf("[ ");
	_vc[0].attrib = 15;
	printf("init: keyboard %2u buf, %2ub each ", MAX_VC, KBD_BUF_SIZE);
	_vc[0].attrib = 8;
	printf("]...........................................");
	_vc[0].attrib = 2;
	printf("Ok");
	_vc[0].attrib = 7;
}
