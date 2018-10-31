#ifndef _KEYBOARD_H
#define _KEYBOARD_H

/**
 * For now we will default to __KB_IBM_PC_XT
 */
#define __KB_IBM_PC_XT


/**
 * Just in case we were defined someplace else
 */
#ifdef __KB_IBM_PC_XT
#undef __KB_IBM_PC_AT
#undef __KB_IBM_PS2
#elif defined(__KB_IBM_PC_AT)
#undef __KB_IBM_PC_XT
#undef __KB_IBM_PS2
#else
#undef __KB_IBM_PC_AT
#undef __KB_IBM_PC_XT
#define __KB_IBM_PC_XT
#endif


#ifdef __KB_IBM_PC_XT
/**
 * Set 1 Scancodes
 *
 * IBM PC XT
 *
 * __KB_IBM_PC_XT must be defined to use these
 */
#define	KEY_ESC			0x01
#define	KEY_F1			0x3B
#define	KEY_F2			0x3C
#define	KEY_F3			0x3D
#define	KEY_F4			0x3E
#define	KEY_F5			0x3F
#define	KEY_F6			0x40
#define	KEY_F7			0x41
#define	KEY_F8			0x42
#define	KEY_F9			0x43
#define	KEY_F10			0x44
#define	KEY_F11			0x57
#define	KEY_F12			0x58
#define	KEY_TILDA		0x29
#define	KEY_1				0x02
#define	KEY_2				0x03
#define	KEY_3				0x04
#define	KEY_4				0x05
#define	KEY_5				0x06
#define	KEY_6				0x07
#define	KEY_7				0x08
#define	KEY_8				0x09
#define	KEY_9				0x0A
#define	KEY_0				0x0B
#define	KEY_MINUS		0x0C
#define	KEY_PLUS			0x0D
#define	KEY_BKSLASH		0x2B
#define	KEY_BKSPACE		0x0E
#define	KEY_TAB			0x0F
#define	KEY_Q				0x10
#define	KEY_W				0x11
#define	KEY_E				0x12
#define	KEY_R				0x13
#define	KEY_T				0x14
#define	KEY_Y				0x15
#define	KEY_U				0x16
#define	KEY_I				0x17
#define	KEY_O				0x18
#define	KEY_P				0x19
#define	KEY_LBRACKET	0x1A
#define	KEY_RBRACKET	0x1B
#define	KEY_ENTER		0x1C
#define	KEY_CAPS			0x3A
#define	KEY_A				0x1E
#define	KEY_S				0x1F
#define	KEY_D				0x20
#define	KEY_F				0x21
#define	KEY_G				0x22
#define	KEY_H				0x23
#define	KEY_J				0x24
#define	KEY_K				0x25
#define	KEY_L				0x26
#define	KEY_SEMICOLON	0x27
#define	KEY_QUOTE		0x28
#define	KEY_LSHIFT		0x2A
#define	KEY_Z				0x2C
#define	KEY_X				0x2D
#define	KEY_C				0x2E
#define	KEY_V				0x2F
#define	KEY_B				0x30
#define	KEY_N				0x31
#define	KEY_M				0x32
#define	KEY_COMMA		0x33
#define	KEY_PERIOD		0x34
#define	KEY_SLASH		0x35
#define	KEY_RSHIFT		0x36
#define	KEY_LCTRL		0x1D
#define	KEY_LWIN			0x5B
#define	KEY_LALT			0x38
#define	KEY_SPACE		0x39
#define	KEY_RALT			0x38
#define	KEY_RWIN			0x5C
#define	KEY_MENU			0x5D
#define	KEY_RCTRL		0x1D
#define	KEY_SCRLCK		0x46		/* scroll lock */

/**
 * Keypad
 */
#define	KEYP_NUMLCK		0x45
#define	KEYP_SLASH		0x35
#define	KEYP_ASTERISK	0x37
#define	KEYP_MINUS		0x4A
#define	KEYP_7			0x47
#define	KEYP_8			0x48
#define	KEYP_9			0x49
#define	KEYP_PLUS		0x4E
#define	KEYP_4			0x4B
#define	KEYP_5			0x4C
#define	KEYP_6			0x4D
#define	KEYP_1			0x4F
#define	KEYP_2			0x50
#define	KEYP_3			0x51
#define	KEYP_ENTER		0x1C
#define	KEYP_0			0x52
#define	KEYP_PERIOD		0x53

/**
 * Insert, Home, Page Up cluster
 */
#define	KEY_INSERT		0x52
#define	KEY_HOME			0x47
#define	KEY_PGUP			0x49
#define	KEY_DEL			0x53
#define	KEY_END			0x4F
#define	KEY_PGDOWN		0x51

/**
 * Arrow key cluster
 */
#define	KEY_UP			0x48
#define	KEY_LEFT			0x4B
#define	KEY_DOWN			0x50
#define	KEY_RIGHT		0x4D

/**
 * FIXME: Experimental
 */
//#define	KEY_PRTSCR		0xE02AE037		/* print screen */
//#define	KEY_PAUSE		0xE11D45E19DC5	/* pause/break */

#endif /* defined(__KB_IBM_PC_XT) */

#ifdef __KB_IBM_PC_AT
/**
 * Set 2 Scancodes
 *
 * IBM PC AT
 *
 * __KB_IBM_PC_AT must be defined to use these
 */
#define	KEY_ESC			0x76
#define	KEY_F1			0x05
#define	KEY_F2			0x06
#define	KEY_F3			0x04
#define	KEY_F4			0x0C
#define	KEY_F5			0x03
#define	KEY_F6			0x0B
#define	KEY_F7			0x83
#define	KEY_F8			0x0A
#define	KEY_F9			0x01
#define	KEY_F10			0x09
#define	KEY_F11			0x78
#define	KEY_F12			0x07
#define	KEY_TILDA		0x0E
#define	KEY_1				0x16
#define	KEY_2				0x1E
#define	KEY_3				0x26
#define	KEY_4				0x25
#define	KEY_5				0x2E
#define	KEY_6				0x36
#define	KEY_7				0x3D
#define	KEY_8				0x3E
#define	KEY_9				0x46
#define	KEY_0				0x45
#define	KEY_MINUS		0x4E
#define	KEY_PLUS			0x55
#define	KEY_BKSLASH		0x5D
#define	KEY_BKSPACE		0x66
#define	KEY_TAB			0x0D
#define	KEY_Q				0x15
#define	KEY_W				0x1D
#define	KEY_E				0x24
#define	KEY_R				0x2D
#define	KEY_T				0x2C
#define	KEY_Y				0x35
#define	KEY_U				0x3C
#define	KEY_I				0x43
#define	KEY_O				0x44
#define	KEY_P				0x4D
#define	KEY_LBRACKET	0x54
#define	KEY_RBRACKET	0x5B
#define	KEY_ENTER		0x5A
#define	KEY_CAPS			0x58
#define	KEY_A				0x1C
#define	KEY_S				0x1B
#define	KEY_D				0x23
#define	KEY_F				0x2B
#define	KEY_G				0x34
#define	KEY_H				0x33
#define	KEY_J				0x3B
#define	KEY_K				0x42
#define	KEY_L				0x4B
#define	KEY_SEMICOLON	0x4C
#define	KEY_QUOTE		0x52
#define	KEY_LSHIFT		0x12
#define	KEY_Z				0x1A
#define	KEY_X				0x22
#define	KEY_C				0x21
#define	KEY_V				0x2A
#define	KEY_B				0x32
#define	KEY_N				0x31
#define	KEY_M				0x3A
#define	KEY_COMMA		0x41
#define	KEY_PERIOD		0x49
#define	KEY_SLASH		0x4A
#define	KEY_RSHIFT		0x59
#define	KEY_LCTRL		0x14
#define	KEY_LWIN			0xE01F
#define	KEY_LALT			0x11
#define	KEY_SPACE		0x29
#define	KEY_RALT			0xE011
#define	KEY_RWIN			0xE027
#define	KEY_MENU			0xE02F
#define	KEY_RCTRL		0xE014
#define	KEY_SCRLCK		0x7E		/* scroll lock */

/**
 * Keypad
 */
#define	KEYP_NUMLCK		0x77
#define	KEYP_SLASH		0xE04A
#define	KEYP_ASTERISK	0x7C
#define	KEYP_MINUS		0x7B
#define	KEYP_7			0x6C
#define	KEYP_8			0x75
#define	KEYP_9			0x7D
#define	KEYP_PLUS		0x79
#define	KEYP_4			0x6B
#define	KEYP_5			0x73
#define	KEYP_6			0x74
#define	KEYP_1			0x69
#define	KEYP_2			0x72
#define	KEYP_3			0x7A
#define	KEYP_ENTER		0xE05A
#define	KEYP_0			0x70
#define	KEYP_PERIOD		0x71

/**
 * Insert, Home, Page Up cluster
 */
#define	KEY_INSERT		0xE070
#define	KEY_HOME			0xE06C
#define	KEY_PGUP			0xE07D
#define	KEY_DEL			0xE071
#define	KEY_END			0xE069
#define	KEY_PGDOWN		0xE07A

/**
 * Arrow key cluster
 */
#define	KEY_UP			0xE075
#define	KEY_LEFT			0xE06B
#define	KEY_DOWN			0xE072
#define	KEY_RIGHT		0xE074

/**
 * FIXME: Experimental
 */
#define	KEY_PRTSCR		0xE012E07C				/* print screen */
#define	KEY_PAUSE		0xE11477E1F014F077	/* pause/break */

#endif /* defined(__KB_IBM_PC_AT) */

#ifdef __KB_IBM_PS2
/**
 * Set 1 Scancodes
 *
 * IBM PC XT
 *
 * __KB_IBM_PS2 must be defined to use these
 */
#define	KEY_ESC			0x08
#define	KEY_F1			0x07
#define	KEY_F2			0x0F
#define	KEY_F3			0x17
#define	KEY_F4			0x1F
#define	KEY_F5			0x27
#define	KEY_F6			0x2F
#define	KEY_F7			0x37
#define	KEY_F8			0x3F
#define	KEY_F9			0x47
#define	KEY_F10			0x4F
#define	KEY_F11			0x56
#define	KEY_F12			0x5E
#define	KEY_TILDA		0x0E
#define	KEY_1				0x16
#define	KEY_2				0x1E
#define	KEY_3				0x26
#define	KEY_4				0x25
#define	KEY_5				0x2E
#define	KEY_6				0x36
#define	KEY_7				0x3D
#define	KEY_8				0x3E
#define	KEY_9				0x46
#define	KEY_0				0x45
#define	KEY_MINUS		0x4E
#define	KEY_PLUS			0x55
#define	KEY_BKSLASH		0x5C
#define	KEY_BKSPACE		0x66
#define	KEY_TAB			0x0D
#define	KEY_Q				0x15
#define	KEY_W				0x1D
#define	KEY_E				0x24
#define	KEY_R				0x2D
#define	KEY_T				0x2C
#define	KEY_Y				0x35
#define	KEY_U				0x3C
#define	KEY_I				0x43
#define	KEY_O				0x44
#define	KEY_P				0x4D
#define	KEY_LBRACKET	0x54
#define	KEY_RBRACKET	0x5B
#define	KEY_ENTER		0x5A
#define	KEY_CAPS			0x14
#define	KEY_A				0x1C
#define	KEY_S				0x1B
#define	KEY_D				0x23
#define	KEY_F				0x2B
#define	KEY_G				0x34
#define	KEY_H				0x33
#define	KEY_J				0x3B
#define	KEY_K				0x42
#define	KEY_L				0x4B
#define	KEY_SEMICOLON	0x4C
#define	KEY_QUOTE		0x52
#define	KEY_LSHIFT		0x12
#define	KEY_Z				0x1A
#define	KEY_X				0x22
#define	KEY_C				0x21
#define	KEY_V				0x2A
#define	KEY_B				0x32
#define	KEY_N				0x31
#define	KEY_M				0x3A
#define	KEY_COMMA		0x41
#define	KEY_PERIOD		0x49
#define	KEY_SLASH		0x4A
#define	KEY_RSHIFT		0x59
#define	KEY_LCTRL		0x11
#define	KEY_LWIN			0x8B
#define	KEY_LALT			0x19
#define	KEY_SPACE		0x29
#define	KEY_RALT			0x39
#define	KEY_RWIN			0x8C
#define	KEY_MENU			0x8D
#define	KEY_RCTRL		0x58
#define	KEY_SCRLCK		0x5F		/* scroll lock */
#define	KEY_PRTSCR		0x57		/* print screen */
#define	KEY_PAUSE		0x62		/* pause/break */

/**
 * Keypad
 */
#define	KEYP_NUMLCK		0x76
#define	KEYP_SLASH		0x77
#define	KEYP_ASTERISK	0x7E
#define	KEYP_MINUS		0x84
#define	KEYP_7			0x6C
#define	KEYP_8			0x75
#define	KEYP_9			0x7D
#define	KEYP_PLUS		0x7C
#define	KEYP_4			0x6B
#define	KEYP_5			0x73
#define	KEYP_6			0x74
#define	KEYP_1			0x69
#define	KEYP_2			0x72
#define	KEYP_3			0x7A
#define	KEYP_ENTER		0x79
#define	KEYP_0			0x70
#define	KEYP_PERIOD		0x71

/**
 * Insert, Home, Page Up cluster
 */
#define	KEY_INSERT		0x67
#define	KEY_HOME			0x6E
#define	KEY_PGUP			0x6F
#define	KEY_DEL			0x64
#define	KEY_END			0x65
#define	KEY_PGDOWN		0x6D

/**
 * Arrow key cluster
 */
#define	KEY_UP			0x63
#define	KEY_LEFT			0x61
#define	KEY_DOWN			0x60
#define	KEY_RIGHT		0x6A

#endif /* defined(__KB_IBM_PS2) */

#endif /* ! _KEYBOARD_H */
