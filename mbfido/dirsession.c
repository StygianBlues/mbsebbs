/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: Directory Mail/Files sessions
 *
 *****************************************************************************
 * Copyright (C) 1997-2002
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
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include "../config.h"
#include "../lib/libs.h"
#include "../lib/memwatch.h"
#include "../lib/structs.h"
#include "../lib/users.h"
#include "../lib/records.h"
#include "../lib/common.h"
#include "../lib/clcomm.h"
#include "dirsession.h"


extern int  do_unprot;


/*
 * Check for lock, return TRUE if node is locked.
 */
int islocked(char *lockfile, int chklck, int waitclr, int loglvl)
{
    int	    i;
    time_t  now, ftime;

    if (chklck && strlen(lockfile)) {
	Syslog(loglvl, "Checking lockfile %s", lockfile);

	/*
	 * First check for stale lockfile.
	 */
	if (! file_exist(lockfile, R_OK)) {
	    ftime = file_time(lockfile);
	    now = time(NULL);
	    if ((now - ftime) > 21600) {
		Syslog('+', "Removing stale lockfile %s, older then 6 hours", lockfile);
		file_rm(lockfile);
	    }
	}

	/*
	 * Next check for lockfile with wait for clear
	 */
	if ((file_exist(lockfile, R_OK) == 0) && waitclr) {
	    Syslog('+', "Node is locked, waiting 10 minutes");
	    i = 120;
	    while (TRUE) {
		sleep(5);
		Nopper();
		if (file_exist(lockfile, R_OK))
		    break;
		i--;
		if (i == 0) {
		    Syslog('+', "Timeout waiting for lock %s", lockfile);
		    break;
		}
	    }
	}

	/*
	 * Check again if lockfile exists.
	 */
	if (file_exist(lockfile, R_OK)) {
	    return FALSE;
	} else {
	    Syslog('+', "Lockfile exists, skipping this node");
	    return TRUE;
	}
    }
    return FALSE;
}



/*
 * Create a 1 byte lockfile if create is TRUE.
 * Returns FALSE if failed.
 */
int setlock(char *lockfile, int create, int loglvl)
{
    FILE    *fp;
    char    temp[1];

    if (create && strlen(lockfile)) {
	Syslog(loglvl, "create lockfile %s", lockfile);
	if ((fp = fopen(lockfile, "w")) == NULL) {
	    WriteError("$Can't create lock %s", lockfile);
	    return FALSE;
	}
	fwrite(temp, 1, sizeof(char), fp);
	fsync(fileno(fp));
	fclose(fp);
	chmod(lockfile, 0664);
    }

    return TRUE;
}



/*
 * Removing lockfile
 */
void remlock(char *lockfile, int create, int loglvl)
{
    if (create) {
	if (file_rm(lockfile))
	    WriteError("$Can't remove lock %s", lockfile);
	else
	    Syslog(loglvl, "Removed lock %s", lockfile);
    }
}



void dirinbound(void)
{
    FILE	    *fp;
    char	    *temp, *from, *too;
    long	    fileptr;
    struct dirent   *de;
    DIR		    *dp;
    
    Syslog('m', "Starting directory inbound sessions");

    temp = calloc(PATH_MAX, sizeof(char));
    sprintf(temp, "%s/etc/nodes.data", getenv("MBSE_ROOT"));
    if ((fp = fopen(temp, "r")) != NULL) {
	fread(&nodeshdr, sizeof(nodeshdr), 1, fp);

	while (fread(&nodes, nodeshdr.recsize, 1, fp) == 1) {
	    if (nodes.Session_in == S_DIR && strlen(nodes.Dir_in_path)) {
		fileptr = ftell(fp) - nodeshdr.recsize;
		Syslog('+', "Directory inbound session for node %s", aka2str(nodes.Aka[0]));
		if (! islocked(nodes.Dir_in_clock, nodes.Dir_in_chklck, nodes.Dir_in_waitclr, 'm')) {
		    Syslog('m', "Node is free, start processing");
		    setlock(nodes.Dir_in_mlock, nodes.Dir_in_mklck, 'm');

		    if ((dp = opendir(nodes.Dir_in_path)) == NULL) {
			WriteError("$Can't open directory %s", nodes.Dir_in_path);
		    } else {
			from = calloc(PATH_MAX, sizeof(char));
			too  = calloc(PATH_MAX, sizeof(char));
			while ((de = readdir(dp))) {
			    if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
				sprintf(from, "%s/%s", nodes.Dir_in_path, de->d_name);
				if (access(from, R_OK | W_OK)) {
				    WriteError("$No rights to move %s", from);
				} else {
				    /*
				     * Inbound is set by previous toss or tic to
				     * protected or unprotected inbound.
				     */
				    if (do_unprot)
					sprintf(too,  "%s/%s", CFG.inbound, de->d_name);
				    else
					sprintf(too,  "%s/%s", CFG.pinbound, de->d_name);
				    if (access(too, F_OK) == 0) {
					WriteError("File %s already in inbound, skip", too);
				    } else {
					Syslog('m', "Move %s to %s", from, too);
					if (file_mv(from, too)) {
					    WriteError("$Move %s to %s failed", from, too);
					}
				    }
				}
			    }
			}
			free(too);
			free(from);
		    }
		    
		    remlock(nodes.Dir_in_mlock, nodes.Dir_in_mklck, 'm');
		}

		/*
		 * Reread noderecord
		 */
		fseek(fp, fileptr, SEEK_SET);
		fread(&nodes, nodeshdr.recsize, 1, fp);
	    }
	    fseek(fp, nodeshdr.filegrp + nodeshdr.mailgrp, SEEK_CUR);
	}
    }
    free(temp);
    Syslog('m', "Finished directory inbound sessions");
}


