/**
 * esh.c
 *
 * Error SHell for HybOS
 *
 * Exports:
 *  None.
 *
 * Imports:
 *  None.
 * 
 * Description:
 *  Since esh is called on a new thread in user-space and not in
 *  kernel-space, imports and exports are not necessary. All the
 *  kernel needs to know is what functions to call when the
 *  keyboard buffer has been filled (this even happens when the
 *  user presses the EOL key, which is the ENTER key on either
 *  the keypad or the "normal" keyboard).
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <esh/esh.h>

void dumpheapk(void);

ESHCOMMANDS eshCommands[COMMAND_COUNT];


/**
 * keyDown()
 *
 * This function is called when a key is pressed and is called
 * on a new thread which resides in user-space so that esh
 * cannot lock up the kernel.
 *
 * @param	unsigned		Which key was pressed. See keyboard.h
 *
 * @return	void
 */
void keyDown(unsigned key)
{
	key = key; /* to shut gcc up */
}

/**
 * keyUp()
 *
 * This function is called when a key is released.
 *
 * @param	unsigned		Which key was released. See keyboard.h
 *
 * @return	void
 */
void keyUp(unsigned key)
{
	key = key; /* to shut gcc up */
}

void initCommands(void)
{
	eshCommands[0].minparams = 0;
	eshCommands[0].maxparams = 0;
	strcpy(eshCommands[0].command, "dumpheap\0");
	strcpy(eshCommands[0].params[0], "-h\0");
	strcpy(eshCommands[0].description, "Print listing of heap usage and status.\0");

	eshCommands[1].minparams = 1;
	eshCommands[1].maxparams = MAX_PARAMS;
	strcpy(eshCommands[1].params[0], "-h\0");
	strcpy(eshCommands[1].command, "echo\0");
	strcpy(eshCommands[1].description, "Echo a line of text to the terminal.\0");

	/*this will be called with either "help" or "help command" */
	eshCommands[2].minparams = 0;
	eshCommands[2].maxparams = 1;
	strcpy(eshCommands[2].params[0], "-h\0");
	strcpy(eshCommands[2].command, "help\0");
	strcpy(eshCommands[2].description, "Displays general help menu or help on specific command.\0");

	/* -r|-h [time] */
	eshCommands[3].minparams = 1;
	eshCommands[3].maxparams = 2;
	strcpy(eshCommands[3].command, "shutdown\0");
	strcpy(eshCommands[3].params[0], "-r\0");
	strcpy(eshCommands[3].params[1], "-h\0");
	strcpy(eshCommands[3].params[2], "NOW\0");
	strcpy(eshCommands[3].description, "Halt or restart the system.\0");

	/* clear screen */
	eshCommands[4].minparams = 0;
	eshCommands[4].maxparams = 0;
	strcpy(eshCommands[4].params[0], "-h\0");
	strcpy(eshCommands[4].command, "cls\0");
	strcpy(eshCommands[4].description, "Clears the terminal of all output.\0");

	/* print working directory */
	eshCommands[5].minparams = 0;
	eshCommands[5].maxparams = 0;
	strcpy(eshCommands[5].params[0], "-h\0");
	strcpy(eshCommands[5].command, "pwd\0");
	strcpy(eshCommands[5].description, "Prints the current working directory.\0");
}

/**
 * mapCommand()
 *
 * Used internally by esh to map a command to it's zero-based
 * index of commands.
 *
 * @param	char *	the entire line of the command
 *
 * @return	int		index of command entry if found, otherwise -1
 */
int mapCommand(char *cmd)
{
	int i;						/* for our loops */
	int params;					/* number of parameters found for the command */
	int previdx;				/* previous index */
	char cmdName[MAX_LEN];	/* name of the command */

	i = 0;
	previdx = 0;
	params = 0;

	/**
	 * Loop while cmd[i] is not a space
	 */
	i = 0;
	for(i = 0; i < (int)strlen(cmd); i++)
	{
		if(cmd[i] == ' ')
			break;
	}

	strncpy(cmdName, cmd, i);
	cmdName[i] = '\0';

	for(i = 0; i < COMMAND_COUNT; i++)
	{
		if(!strcmp(eshCommands[i].command, cmdName))
			return i;
	}

	return -1;
}

/**
 * isParam()
 *
 * Determines if the supplied parameter is valid for the
 * given command.
 *
 * @param	int		index of command
 * @param	char *	command string
 *
 * @return	bool		true if parameter is valid, false otherwise
 */
bool isParam(int argc, char *argv)
{
	int i;

	for(i = 0; i < MAX_PARAMS; i++)
	{
		if(!strcmp(eshCommands[argc].params[i], argv))
			return true;
	}

	return false;
}

/**
 * mapParams()
 *
 * Maps each parameter to the pars struct
 *
 * @param	char *	Buffer from the command line
 * @param	struct	Parameter structure
 *
 * @return	int		Number of command line parameters (arguments) parsed
 */
int mapParams(char *buf, ESHCURRCOMMAND *pars)
{
	int i;						/* for our loops */
	int previdx;				/* previous index */
	int idx;						/* current index */
	int j;						/* loops */

	i = 0;
	j = 0;
	previdx = 0;
	pars->count = 0;

	previdx = mapCommand(buf);

	if(previdx == -1)
		return 0;

	strcpy(pars->param[0], eshCommands[previdx].command);
	j++;
	pars->count++;

	i = 0;
	idx = 0;

	for(i = 0; i < (int)strlen(buf); i++)
	{
		/* we have encountered a seperator */
		if(buf[i] == ' ')
		{
			if(j > MAX_PARAMS)
				break;
			
			i++; /* skip one space */

			idx = i;

			if(buf[i] == '"')
			{
				i++;
				idx++;

				while(buf[i] != '"' && i != (int)strlen(buf))
					i++;

				strncpy(pars->param[j], &buf[idx], (i - idx));
				pars->param[j][i - idx] = '\0';

				idx = i;
				j++;
				pars->count++;
			}
			else
			{
				while(buf[i] != ' ' && i != (int)strlen(buf))
					i++;
			
				strncpy(pars->param[j], &buf[idx], i - idx);
				pars->param[j][i - idx] = '\0';
				i = idx;
				j++;
				pars->count++;
			}
		}
	}
	
	return pars->count;
}

/**
 * processCommand()
 *
 * This function is called when the user has pressed
 * the ENTER key.
 *
 * @param	char *	The contents of the current buffer or NULL if empty
 * @param	int		Size of the buffer (number of characters)
 *
 * @return	void
 */
void processCommand(char *line, int count)
{
	int i;
	int cmd;		/* stores the numeric index of the command */
	ESHCURRCOMMAND params;

	count = count; /* to shut gcc up */

	/*for(i = 0; i < MAX_PARAMS; i++)
		params.param[i][0] = '\0';*/
	params.param[0][0] = '\0';
	
	initCommands();

	cmd = mapCommand(line);
	mapParams(line, &params);

	switch(cmd)
	{
		case 0:	/* dumpheap */
			dumpheapk();
			//testheap();
			break;
		case 1:	/* echo */
			for(i = 1; i < params.count; i++)
				printf("%s", params.param[i]);

			printf("\n");
			break;
		case 2:	/* help */
			if(params.count == 1)
			{
				printf("HybOS EShell Commands:\n");
				for(i = 0; i < COMMAND_COUNT; i++)
					if(strlen(eshCommands[i].command) > 0)
						printf("%10s %-s\n", eshCommands[i].command, eshCommands[i].description);
			}
			else
			{
				cmd = mapCommand(params.param[0]);
				mapParams(params.param[1], &params);
				
				//if(isParam(cmd, params.param[1]))
				if(cmd != -1)
				{
					printf("Usage: %s %s\n", params.param[0], eshCommands[cmd].params[1]);
				}
				else
					printf("esh: '%s' not found.\n", params.param[1]);
			}
			break;
		default:
			if(strlen(params.param[0]) > 0 && strcmp(params.param[0], "help"))
				printf("esh: '%s' not found.\n", params.param[0]);
			else
				printf("esh: '%s' not found.\n", &line[0]);
			break;
	}

	/*for(i = 0; i < params.count; i++)
		printf("param[%i]: %s\n", i, params.param[i]);*/

	//if(isParam(3, &params.param[1]))
	//	printf("valid parameter\n");
	//else
	//	printf("invalid parameter\n");

	return;

	if(!strcmp(line, "dumpheap"))
		dumpheapk();
	else if(!strncmp(line, "echo", 4))
		printf("%s\n", line[4] == ' ' ? &line[5] : &line[4]);
	else if(!strcmp(line, "help"))
	{
		printf("HybOS EShell Commands:\n");
		printf("dumpheap\tPrint listing of heap usage and status\n");
		printf("testheap\tTest the heap and print out results\n");
		printf("shutdown -r\tRestart the system.\n");
		printf("pwd\t\tPrint the current working directory.\n");
	}
	else if(!strncmp(line, "shutdown", 8))
	{
		if(strlen(line) > 9 && !strncmp(&line[9], "-r", 2))
		{
			printf("\nSystem shutdown from vtty%u\n", get_current_vc());
			printf("Restarting...");
			//reboot();
		}
		else
		{
			if((strlen(line) > 9) && (strlen(&line[9]) > 0))
				printf("shutdown: Invalid argument \"%s\".\n", &line[9]);
			else
				printf("Usage: shutdown -r\n");
		}
	}
	else if((strlen(line) > 0) && (!strcmp(line, "cls")))
	{
		printf("\x1B[2J");
	}
	else if((strlen(line) >= 8) && (!strcmp(line, "testheap")))
	{
		//testheap();
	}
	else if((strlen(line) > 0) && (!strcmp(line, "pwd")))
		printf("/\n");
	else if(strlen(line) > 0)
		printf("eshell: \"%s\" not found.\n", line);
}
