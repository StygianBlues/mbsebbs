/*****************************************************************************
 *
 * File ..................: mbcico/m7send.c
 * Purpose ...............: Fidonet mailer 
 * Last modification date : 02-Jan-2001
 *
 *****************************************************************************
 * Copyright (C) 1997-2001
 *   
 * Michiel Broek		FIDO:	2:280/2802
 * Beekmansbos 10
 * 1971 BV IJmuiden
 * the Netherlands
 *
 * This file is part of MBSE BBS.
 *
 * This BBS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * MBSE BBS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MBSE BBS; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include "../lib/libs.h"
#include "../lib/structs.h"
#include "../lib/common.h"
#include "../lib/clcomm.h"
#include "statetbl.h"
#include "ttyio.h"
#include "m7send.h"

static int m7_send(void);

static char *fn;

int m7send(char *fname)
{
	fn=fname;
	return m7_send();
}



SM_DECL(m7_send,(char *)"m7send")
SM_STATES
	waitnak,
	sendack,	
	sendchar,
	waitack,
	sendsub,
	waitcheck,
	ackcheck
SM_NAMES
	(char *)"waitnak",
	(char *)"sendack",
	(char *)"sendchar",
	(char *)"waitack",
	(char *)"sendsub",
	(char *)"waitcheck",
	(char *)"ackcheck"
SM_EDECL

	char	buf[12],*p;
	int	i,c,count=0;
	char	cs=SUB;

	memset(buf,' ',sizeof(buf));
	for (i=0,p=fn; (i<8) && (*p) && (*p != '.'); i++,p++)
		buf[i] = toupper(*p);
	if (*p == '.') 
		p++;
	for (; (i<11) && (*p); i++,p++)
		buf[i] = toupper(*p);
	for (i=0; i<11; i++)
		cs += buf[i];
	buf[11]='\0';
	Syslog('x', "modem7 filename \"%s\", checksum %02x", buf,(unsigned char)cs);

SM_START(sendack)

SM_STATE(waitnak)

	Syslog('x', "m7send WAITNAK");
	if (count++ > 20) {
		Syslog('+', "too many tries sending modem7 name");
		SM_ERROR;
	}

	c=GETCHAR(5);
	if (c == TIMEOUT) {
		Syslog('x', "m7 got timeout waiting NAK");
		SM_PROCEED(waitnak);
	} else if (c < 0) {
		SM_ERROR;
	} else if (c == NAK) {
		SM_PROCEED(sendack);
	} else {
		Syslog('x', "m7 got '%s' waiting NAK", printablec(c));
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	}

SM_STATE(sendack)

	Syslog('x', "m7send SENDACK");
	i = 0;
	PUTCHAR(ACK);
	if (STATUS) {
		SM_ERROR;
	} else {
		SM_PROCEED(sendchar);
	}

SM_STATE(sendchar)

	Syslog('x', "m7send SENDCHAR");
	if (i > 11) {
		SM_PROCEED(sendsub);
	}

	PUTCHAR(buf[i++]);
	if (STATUS) {
		SM_ERROR;
	} else {
		SM_PROCEED(waitack);
	}

SM_STATE(waitack)

	Syslog('x', "m7send WAITACK");
	c = GETCHAR(1);
	if (c == TIMEOUT) {
		Syslog('x', "m7 got timeout waiting ACK for char %d",i);
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	} else if (c < 0) {
		SM_ERROR;
	} else if (c == ACK) {
		SM_PROCEED(sendchar);
	} else {
		Syslog('x', "m7 got '%s' waiting ACK for char %d", printablec(c),i);
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	}

SM_STATE(sendsub)

	Syslog('x', "m7send SENDSUB");
	PUTCHAR(SUB);
	SM_PROCEED(waitcheck);

SM_STATE(waitcheck)

	Syslog('x', "m7send WAITCHECK");
	c = GETCHAR(1);
	if (c == TIMEOUT) {
		Syslog('x', "m7 got timeout waiting check");
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	} else if (c < 0) {
		SM_ERROR;
	} else if (c == cs) {
		SM_PROCEED(ackcheck);
	} else {
		Syslog('x', "m7 got %02x waiting check %02x", (unsigned char)c,(unsigned char)cs);
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	}

SM_STATE(ackcheck)

	Syslog('x', "m7send ACKCHECK");
	PUTCHAR(ACK);
	if (STATUS) {
		SM_ERROR;
	} else {
		SM_SUCCESS;
	}

SM_END
SM_RETURN


