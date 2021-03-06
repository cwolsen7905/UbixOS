/***
*abs.c - find absolute value
*
*       Copyright (c) 1985-1997, Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines abs() - find the absolute value of an int.
*
*******************************************************************************/


/***
*int abs(number) - find absolute value of number
*
*Purpose:
*       Returns the absolute value of number (if number >= 0, returns number,
*       else returns -number).
*
*Entry:
*       int number - number to find absolute value of
*
*Exit:
*       returns the aboslute value of number
*
*Exceptions:
*
*******************************************************************************/

int abs(int number)
{
	return( number>=0 ? number : -number );
}
