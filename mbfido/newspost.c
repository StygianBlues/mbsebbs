/*****************************************************************************
 *
 * File ..................: mbfido/newspost.c
 * Purpose ...............: Post newsarticles in temp newsfile.
 * Last modification date : 21-May-2001
 *
 *****************************************************************************
 * Copyright (C) 1997-2001
 *   
 * Michiel Broek		FIDO:		2:280/2801
 * Beekmansbos 10		Internet:	mbroek@users.sourceforge.net
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
#include "../lib/records.h"
#include "../lib/common.h"
#include "../lib/clcomm.h"
#include "../lib/mbinet.h"
#include "newspost.h"


extern	FILE	*nfp;
extern	int	newsopen;
extern	int	news_out;
extern	int	news_bad;



int newspost(void)
{
	int	start = TRUE;
	char	*buf, *p;
	long	curpos, count, seqnr;
	FILE	*ofp = NULL, *nb;
	struct  utsname utsbuf;

	if (newsopen)
		fclose(nfp);
	buf = calloc(10240, sizeof(char));

	/*
	 *  Now reopen the file for reading. If it fails and
	 *  the file was original closed we leave quiet.
	 *  If the file wasn't open previously but there is
	 *  a file, try to post the articles. They may be
	 *  still here if the newsserver wasn't available.
	 */
	sprintf(buf, "%s/tmp/newsout", getenv("MBSE_ROOT"));
	if ((nfp = fopen(buf, "r")) == NULL) {
		if (newsopen)
			WriteError("$Can't reopen %s", buf);
		free(buf);
		return newsopen;
	}
	IsDoing("Post news");

	if (CFG.newsfeed == FEEDINN) {
		Syslog('+', "Posting news articles to the NNTP server");
		if (nntp_connect() == -1) {
			free(buf);
			return TRUE;
		}

		while (fgets(buf, 10240, nfp)) {
			if (start) {
				if (nntp_cmd((char *)"POST\r\n", 340) != 0) {
					WriteError("NNTP POST refused");
					free(buf);
					return TRUE;
				}
			}
			start = FALSE;
			if (!strcmp(buf, ".\n")) {
				if (nntp_cmd((char *)".\r\n", 240) == 0) {
					news_out++;
				} else {
					WriteError("NNTP: refused article %d", news_out+1);
					news_bad++;
				}
				start = TRUE;
			} else {
				/*
				 *  Most NNTP servers like cr/lf after each line.
				 */
				Striplf(buf);
				p = buf+strlen(buf);
				*p++ = '\r';
				*p++ = '\n';
				*p = '\0';
				nntp_send(buf);
			}
			Nopper();
		}
		nntp_close();
	}

	/*
	 *  Create newsbatch file.
	 */
	if ((CFG.newsfeed == FEEDUUCP) || (CFG.newsfeed == FEEDRNEWS)) {
		Syslog('n', "Building uncompressed batchfile");
		sprintf(buf, "%s/tmp/newsbatch", getenv("MBSE_ROOT"));
		if ((ofp = fopen(buf, "w+")) == NULL) {
			WriteError("$Can't create %s", buf);
			free(buf);
			fclose(nfp);
			return TRUE;
		}
		buf = calloc(10240, sizeof(char));

		count = curpos = 0;
		while (feof(ofp) == 0) {
			/*
			 *  Count the total length of the message
			 */
			while (fgets(buf, 10240, nfp)) {
				if (strcmp(buf, ".\n")) {
					count += strlen(buf);
				} else {
					break;
				}
			}
			if (!count)
				break;
			fseek(nfp, curpos, SEEK_SET);
			fprintf(ofp, "#! rnews %ld\n", count);
			while (fgets(buf, 10240, nfp)) {
				if (strcmp(buf, ".\n")) {
					fprintf(ofp, buf);
				} else {
					break;
				}
			}
			news_out++;
			curpos = ftell(nfp);
			count = 0;
		}
		/*
		 *  Rewind the newsbatch and leave it open.
		 */
		rewind(ofp);
	}

	fclose(nfp);
	newsopen = FALSE;

	/*
	 *  Mode rnews, pipe just created newsbatch to rnews.
	 */
	if (CFG.newsfeed == FEEDRNEWS) {
		if ((nb = (expipe(CFG.rnewspath, NULL, NULL))) == NULL) {
			WriteError("Could not open (pip) output for %s", CFG.rnewspath);
			newsopen = FALSE;
			return TRUE;
		}
		while (fgets(buf, 10240, ofp)) {
			fputs(buf, nb);
		}
		if (exclose(nb)) {
			WriteError("Error closing pipe");
			newsopen = FALSE;
			return TRUE;
		} else
			Syslog('+', "Articles send through %s", CFG.rnewspath);
		fclose(ofp);
		sprintf(buf, "%s/tmp/newsbatch", getenv("MBSE_ROOT"));
		unlink(buf);
	}

	/*
	 *  Mode UUCP, create UUCP files.
	 */
	if (CFG.newsfeed == FEEDUUCP) {
		seqnr = sequencer();
		memset(&utsbuf, 0, sizeof(utsbuf));
		if (uname(&utsbuf)) {
			WriteError("Can't get system nodename");
			newsopen = FALSE;
			return TRUE;
		}

		sprintf(buf, "%s/C.%s%lx", CFG.rnewspath, CFG.nntpnode, seqnr);
		if ((nb = fopen(buf, "a")) == NULL) {
			WriteError("Can't create %s", buf);
			newsopen = FALSE;
			return TRUE;
		}
		seqnr = sequencer();
		fprintf(nb, "E D.%s%lx D.%s%lx news -C D.%s%lx 0666 \"\" 0 rnews\n", 
			utsbuf.nodename, seqnr, utsbuf.nodename, seqnr, utsbuf.nodename, seqnr);
		fclose(nb);
		sprintf(buf, "%s/D.%s%lx", CFG.rnewspath, utsbuf.nodename, seqnr);
		if ((nb = fopen(buf, "a")) == NULL) {
			WriteError("Can't create %s", buf);
			newsopen = FALSE;
			return TRUE;
		}
                while (fgets(buf, 10240, ofp)) {
                        fputs(buf, nb);
                }
                Syslog('+', "Articles placed in %s", CFG.rnewspath);
                fclose(ofp);
                sprintf(buf, "%s/tmp/newsbatch", getenv("MBSE_ROOT"));
                unlink(buf);
	}

	sprintf(buf, "%s/tmp/newsout", getenv("MBSE_ROOT"));
	unlink(buf);
	free(buf);
	return FALSE;
}


