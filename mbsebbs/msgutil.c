/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Utilities for message handling.
 *
 *****************************************************************************
 * Copyright (C) 1997-2005
 *   
 * Michiel Broek		FIDO:		2:280/2802
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
 * Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/mbselib.h"
#include "../lib/mbse.h"
#include "../lib/users.h"
#include "../lib/msgtext.h"
#include "../lib/msg.h"
#include "oneline.h"
#include "msgutil.h"



int	BaseWrite = FALSE;
int	NewMessages = FALSE;

static char *wdays[]={(char *)"Sun",(char *)"Mon",(char *)"Tue",(char *)"Wed",
		      (char *)"Thu",(char *)"Fri",(char *)"Sat"};

static char *months[]={(char *)"Jan",(char *)"Feb",(char *)"Mar",
		       (char *)"Apr",(char *)"May",(char *)"Jun",
		       (char *)"Jul",(char *)"Aug",(char *)"Sep",
		       (char *)"Oct",(char *)"Nov",(char *)"Dec"};



char *rfcdate(time_t now)
{
    static char	buf[40];
    struct tm	ptm, gtm;
    char	sign;
    int		hr, min;
    long	offset;

    if (!now) 
	now = time(NULL);
    ptm = *localtime(&now);

    /*
     * To get the timezone, compare localtime with GMT.
     */
    gtm = *gmtime(&now);

    /*
     * Assume we are never more than 24 hours away.
     */
    offset = gtm.tm_yday - ptm.tm_yday;
    if (offset > 1)
	offset = -24;
    else if (offset < -1)
	offset = 24;
    else
	offset *= 24;

    /*
     * Scale in the hours and minutes; ignore seconds.
     */
    offset += gtm.tm_hour - ptm.tm_hour;
    offset *= 60;
    offset += gtm.tm_min - ptm.tm_min;

    if (offset <= 0) {
	sign='+';
	offset=-offset;
    } else 
	sign='-';
    hr=offset/60L;
    min=offset%60L;

    sprintf(buf,"%s, %02d %s %04d %02d:%02d:%02d %c%02d%02d",
		wdays[gtm.tm_wday],gtm.tm_mday,months[gtm.tm_mon],
		gtm.tm_year+1900,gtm.tm_hour,gtm.tm_min,gtm.tm_sec,
		sign,hr,min);
    return(buf);
}



/*
 *  Open specified message base for read or write.
 */
int Open_Msgbase(char *Base, int Mode)
{
    BaseWrite = FALSE;
    NewMessages = FALSE;

    if (!Msg_Open(Base))
	return FALSE;

    if (Mode != 'w')
	return TRUE;

    if (!Msg_Lock(30L)) {
	Msg_Close();
	return FALSE;
    }

    BaseWrite = TRUE;
    return TRUE;
}



/*
 *  Close current messagebase.
 */
void Close_Msgbase(char *Base)
{
    int	    rc;

    Syslog('b', "Close_Msgbase(%s) BaseWrite=%s NewMessages=%s", Base, BaseWrite?"TRUE":"FALSE", NewMessages?"TRUE":"FALSE");
    if (BaseWrite) {
	Msg_UnLock();
	BaseWrite = FALSE;
	if (NewMessages) {
	    rc = Msg_Link(Base, TRUE, CFG.slow_util);
	    if (rc != -1)
		Syslog('+', "Linked %d message%s", rc, (rc != 1) ? "s":"");
	    else
		Syslog('+', "Could not link messages");
	}
    }
    Msg_Close();
}


char *getrfcchrs(int val)
{
    switch (val) {
	case FTNC_NONE:     return (char *)"iso-8859-1";
	case FTNC_CP437:    return (char *)"cp437";
	case FTNC_CP850:    return (char *)"cp850";
	case FTNC_CP865:    return (char *)"cp865";
	case FTNC_CP866:    return (char *)"cp866";
	case FTNC_CP852:    return (char *)"cp852";
	case FTNC_CP895:    return (char *)"cp895";
	case FTNC_LATIN_1:  return (char *)"iso-8859-1";
	case FTNC_LATIN_2:  return (char *)"iso-8859-2";
	case FTNC_LATIN_5:  return (char *)"iso-8859-5";
	case FTNC_MAC:      return (char *)"MAC 2";
	case FTNC_KOI8_R:   return (char *)"koi8-r";
	case FTNC_CP936:    return (char *)"hz-gb-2312";
	default:            return (char *)"iso-8859-1";
    }
}



void Add_Headkludges(faddr *dest, int IsReply)
{
    char	    *temp;
    unsigned long   crc = -1;
    time_t	    tt;
    int		    i;
    faddr	    *Node;

    temp = calloc(128, sizeof(char));

    switch (msgs.Type) {
	case LOCALMAIL:	Msg.Localmail = TRUE;
			break;

	case NETMAIL:	Msg.Netmail = TRUE;
			sprintf(Msg.FromAddress, "%s", aka2str(msgs.Aka));
			sprintf(Msg.ToAddress, "%s", ascfnode(dest, 0x1f));
			if (msgs.Aka.point) {
			    sprintf(temp, "\001FMPT %d", msgs.Aka.point);
			    MsgText_Add2(temp);
			}
			if (dest->point) {
			    sprintf(temp, "\001TOPT %d", dest->point);
			    MsgText_Add2(temp);
			}
			sprintf(temp, "\001INTL %d:%d/%d %d:%d/%d", dest->zone, dest->net, 
				dest->node, msgs.Aka.zone, msgs.Aka.net, msgs.Aka.node);
			MsgText_Add2(temp);
			break;

	case LIST:	Msg.Echomail = TRUE;
			sprintf(Msg.FromAddress, "%s", aka2str(msgs.Aka));
			break;

	case ECHOMAIL:	Msg.Echomail = TRUE;
			sprintf(Msg.FromAddress, "%s", aka2str(msgs.Aka));
			break;

	case NEWS:	/*
			 * Header style is the same as GoldED does.
			 */
			Msg.News = TRUE;
			sprintf(temp, "\001Date: %s", rfcdate(Msg.Written));
			MsgText_Add2(temp);
			Node = fido2faddr(msgs.Aka);
			sprintf(temp, "\001From: %s", Msg.From);
			MsgText_Add2(temp);
			sprintf(temp, "\001Subject: %s", Msg.Subject);
			MsgText_Add2(temp);
			sprintf(temp, "\001Sender: %s", Msg.From);
			MsgText_Add2(temp);
			tidy_faddr(Node);
			MsgText_Add2((char *)"\001To: All");
			MsgText_Add2((char *)"\001MIME-Version: 1.0");
			if (exitinfo.Charset != FTNC_NONE) {
			    sprintf(temp, "\001Content-Type: text/plain; charset=%s", getrfcchrs(exitinfo.Charset));
			} else if (msgs.Charset != FTNC_NONE) {
			    sprintf(temp, "\001Content-Type: text/plain; charset=%s", getrfcchrs(msgs.Charset));
			} else {
			    sprintf(temp, "\001Content-Type: text/plain; charset=iso8859-1");
			}
			MsgText_Add2(temp);
			MsgText_Add2((char *)"\001Content-Transfer-Encoding: 8bit");
			sprintf(temp, "\001X-Mailreader: MBSE BBS %s", VERSION);
			MsgText_Add2(temp);
			break;
    }

    /*
     * Set the right charset kludge
     */
    if (exitinfo.Charset != FTNC_NONE) {
	sprintf(temp, "\001CHRS: %s", getchrs(exitinfo.Charset));
    } else if (msgs.Charset != FTNC_NONE) {
	sprintf(temp, "\001CHRS: %s", getchrs(msgs.Charset));
    } else {
	sprintf(temp, "\001CHRS: %s", getchrs(FTNC_LATIN_1));
    }
    MsgText_Add2(temp);
    sprintf(temp, "\001MSGID: %s %08lx", aka2str(msgs.Aka), sequencer());
    MsgText_Add2(temp);
    Msg.MsgIdCRC = upd_crc32(temp, crc, strlen(temp));

    if (IsReply) {
	sprintf(temp, "\001REPLY: %s", Msg.Replyid);
	MsgText_Add2(temp);
	crc = -1;
	Msg.ReplyCRC = upd_crc32(temp, crc, strlen(temp));
    } else
	Msg.ReplyCRC = 0xffffffff;

    sprintf(temp, "\001PID: MBSE-BBS %s (%s-%s)", VERSION, OsName(), OsCPU());
    MsgText_Add2(temp);
    tt = time(NULL);
    sprintf(temp, "\001TZUTC: %s", gmtoffset(tt));
    MsgText_Add2(temp);
    free(temp);
}



/*
 *  Add bottom message kludges. The flag Quote is false if this is called
 *  from Offline Reader, the user then may or may have not added a quote.
 *  The QWK upload may set HasTear if the client already uploaded a tearline.
 */
void Add_Footkludges(int Quote, char *tear, int HasTear)
{
    char    *temp, *aka;
    FILE    *fp;

    temp = calloc(PATH_MAX, sizeof(char));
    aka  = calloc(32,  sizeof(char));

    /*
     * If Quote (message entered at the bbs) we append a signature.
     */
    if (Quote) {
	sprintf(temp, "%s/%s/.signature", CFG.bbs_usersdir, exitinfo.Name);
	if ((fp = fopen(temp, "r"))) {
	    MsgText_Add2((char *)"");
	    while (fgets(temp, 80, fp)) {
		Striplf(temp);
		MsgText_Add2(temp);
	    }
	    fclose(fp);
	    MsgText_Add2((char *)"");
	}
    }
	
    if (msgs.Quotes && Quote) {
	sprintf(temp, "... %s", Oneliner_Get());
	MsgText_Add2(temp);
	MsgText_Add2((char *)"");
    }

    /*
     * The offline reader may override the tearline
     */
    if (!HasTear) {
	if (tear == NULL) {
	    MsgText_Add2(TearLine());
	} else {
	    sprintf(temp, "--- %s", tear);
	    MsgText_Add2(temp);
	}
    }

    if ((msgs.Type == ECHOMAIL) || (msgs.Type == LIST)) {
	if (msgs.Aka.point)
	    sprintf(aka, "(%d:%d/%d.%d)", msgs.Aka.zone, msgs.Aka.net, msgs.Aka.node, msgs.Aka.point);
	else
	    sprintf(aka, "(%d:%d/%d)", msgs.Aka.zone, msgs.Aka.net, msgs.Aka.node);

	if (strlen(msgs.Origin))
	    sprintf(temp, " * Origin: %s %s", msgs.Origin, aka);
	else
	    sprintf(temp, " * Origin: %s %s", CFG.origin, aka);
	MsgText_Add2(temp);
    }

    free(aka);
    free(temp);
    NewMessages = TRUE;
}



