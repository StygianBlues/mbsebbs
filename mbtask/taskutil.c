/*****************************************************************************
 *
 * $Id$
 * Purpose ...............: MBSE BBS Task Manager, utilities
 *
 *****************************************************************************
 * Copyright (C) 1997-2004
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
#include "../lib/mbselib.h"
#include "signame.h"
#include "scanout.h"
#include "taskutil.h"



pid_t           	    mypid;	/* Original parent pid if child     	*/
int			    oserr;	/* Last OS error number			*/
extern struct sysconfig     CFG;
extern struct _fidonethdr   fidonethdr;
extern struct _fidonet	    fidonet;
extern struct taskrec	    TCFG;
unsigned long		    lcrc = 0, tcrc = 1;
int			    lcnt = 0, lchr;
static char		    *pbuff = NULL;


static char *mon[] = {
        (char *)"Jan",(char *)"Feb",(char *)"Mar",
        (char *)"Apr",(char *)"May",(char *)"Jun",
        (char *)"Jul",(char *)"Aug",(char *)"Sep",
        (char *)"Oct",(char *)"Nov",(char *)"Dec"
};



/************************************************************************
 *
 *  Loging procedures.
 */


char *date(void);
char *date(void)
{
    struct tm   ptm;
    time_t      now;
    static char buf[20];

    now = time(NULL);
#if defined(__OpenBSD__)
    localtime_r(&now, &ptm);
#else
    ptm = *localtime(&now);
#endif
    sprintf(buf,"%02d-%s-%04d %02d:%02d:%02d", ptm.tm_mday, mon[ptm.tm_mon], ptm.tm_year+1900,
			ptm.tm_hour, ptm.tm_min, ptm.tm_sec);
    return(buf);
}



void WriteError(const char *format, ...)
{
    char    *outputstr;
    va_list va_ptr;

    outputstr = calloc(10240, sizeof(char));
    va_start(va_ptr, format);
    vsprintf(outputstr, format, va_ptr);
    va_end(va_ptr);
    Syslog('?', outputstr);
    free(outputstr);
}



/*
 * general log for this server
 */
void Syslog(int grade, const char *format, ...)
{
    va_list va_ptr;
    char    outstr[1024];
    int     oldmask, debug;
    FILE    *logfile = NULL, *debugfile;
    char    *logname = NULL, *debugname;

    debug = isalpha(grade);
    va_start(va_ptr, format);
    vsprintf(outstr, format, va_ptr);
    va_end(va_ptr);

    tcrc = StringCRC32(outstr);
    if (tcrc == lcrc) {
	lcnt++;
        return;
    }
    lcrc = tcrc;

    if (!debug) {
	logname = calloc(PATH_MAX, sizeof(char));
	oldmask=umask(066);
	sprintf(logname, "%s/log/mbtask.log", getenv("MBSE_ROOT"));
	logfile = fopen(logname, "a");
	umask(oldmask);
	if (logfile == NULL) {
	    printf("Cannot open logfile \"%s\"\n", logname);
	    free(logname);
	    return;
	}
    }

    debugname = calloc(PATH_MAX, sizeof(char));
    oldmask=umask(066);
    sprintf(debugname, "%s/log/%s", getenv("MBSE_ROOT"), CFG.debuglog);
    debugfile = fopen(debugname, "a");
    umask(oldmask);
    if (debugfile == NULL) {
	printf("Cannot open logfile \"%s\"\n", debugname);
	free(debugname);
	if (!debug) {
	    free(logname);
	    fclose(logfile);
	}
	return;
    }
	
    if (lcnt) {
	lcnt++;
        fprintf(debugfile, "%c %s mbtask[%d] last message repeated %d times\n", lchr, date(), mypid, lcnt);
	if (!debug)
	    fprintf(logfile, "%c %s mbtask[%d] last message repeated %d times\n", lchr, date(), mypid, lcnt);
    }
    lcnt = 0;

    if (!debug) {
	fprintf(logfile, "%c %s mbtask[%d] ", grade, date(), mypid);
	fprintf(logfile, *outstr == '$' ? outstr+1 : outstr);
	if (*outstr == '$')
	    fprintf(logfile, ": %s\n", strerror(errno));
	else
	    fprintf(logfile, "\n");

	fflush(logfile);
	    if (fclose(logfile) != 0)
		printf("Cannot close logfile \"%s\"\n", logname);

	free(logname);
    }

    fprintf(debugfile, "%c %s mbtask[%d] ", grade, date(), mypid);
    fprintf(debugfile, *outstr == '$' ? outstr+1 : outstr);
    if (*outstr == '$')
	fprintf(debugfile, ": %s\n", strerror(errno));
    else
	fprintf(debugfile, "\n");

    fflush(debugfile);
    if (fclose(debugfile) != 0)
    printf("Cannot close logfile \"%s\"\n", debugname);

    lchr = grade;
    free(debugname);

    return;
}



/*
 * user log process
 */
int ulog(char *fn, char *grade, char *prname, char *prpid, char *format)
{
    int     i, oldmask;
    FILE    *fp;
        
    oldmask = umask(066);
    fp = fopen(fn, "a");
    umask(oldmask);
    if (fp == NULL) {
	oserr = errno;
	Syslog('!', "$Cannot open user logfile %s", fn);
	return -1;
    }

    fprintf(fp, "%s %s %s[%s] ", grade, date(), prname, prpid);
    for (i = 0; i < strlen(format); i++) {
	if (iscntrl(format[i])) {
	    fputc('^', fp);
	    fputc(format[i] + 64, fp);
	} else {
	    fputc(format[i], fp);
	}
    }
    fputc('\n', fp);
    fflush(fp);

    if (fclose(fp) != 0) {
        oserr = errno;
        Syslog('!', "$Cannot close user logfile %s", fn);
	return -1;
    }
    return 0;
}



char *xstrcpy(char *src)
{
    char    *tmp;

    if (src == NULL) 
        return(NULL);
    tmp = malloc(strlen(src)+1);
    strcpy(tmp, src);
    return tmp;
}



char *xstrcat(char *src, char *add)
{
    char    *tmp;
    size_t  size = 0;

    if ((add == NULL) || (strlen(add) == 0))
        return src;
    if (src)
        size = strlen(src);
    size += strlen(add);
    tmp = malloc(size + 1);
    *tmp = '\0';
    if (src) {
        strcpy(tmp, src);
        free(src);
    }
    strcat(tmp, add);
    return tmp;
}



void CreateSema(char *sem)
{
    char    temp[PATH_MAX];
    FILE    *fp;
    int	    oldmask;

    sprintf(temp, "%s/var/sema/%s", getenv("MBSE_ROOT"), sem);
    if (access(temp, F_OK) == 0)
	return;
    oldmask = umask(002);
    if ((fp = fopen(temp, "w")))
	fclose(fp);
    else
        Syslog('?', "Can't create semafore %s", temp);
    umask(oldmask);
}



void TouchSema(char *sem)
{
    char    temp[PATH_MAX];
    FILE    *fp;
    int	    oldmask;

    sprintf(temp, "%s/var/sema/%s", getenv("MBSE_ROOT"), sem);
    oldmask = umask(002);
    if ((fp = fopen(temp, "w")))
	fclose(fp);
    else
        Syslog('?', "Can't touch semafore %s", temp);
    umask(oldmask);
}



void RemoveSema(char *sem)
{
    char    temp[PATH_MAX];

    sprintf(temp, "%s/var/sema/%s", getenv("MBSE_ROOT"), sem);
    if (access(temp, F_OK))
	return;
    if (unlink(temp) == -1)
        Syslog('?', "Can't remove semafore %s", temp);
}



int IsSema(char *sem)
{
    char    temp[PATH_MAX];

    sprintf(temp, "%s/var/sema/%s", getenv("MBSE_ROOT"), sem);
    return (access(temp, F_OK) == 0);
}



/*
 * Test if the given file exists. The second option is:
 * R_OK - test for Read rights 
 * W_OK - test for Write rights
 * X_OK - test for eXecute rights
 * F_OK - test file presence only
 */ 
int file_exist(char *path, int mode)
{
    if (access(path, mode) != 0)
	return errno;

    return 0;
}



/*
 *    Make directory tree, the name must end with a /
 */
int mkdirs(char *name, mode_t mode)
{
    char    buf[PATH_MAX], *p, *q;
    int     rc, last = 0, oldmask;

    memset(&buf, 0, sizeof(buf));
    strncpy(buf, name, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    p = buf+1;

    oldmask = umask(000);
    while ((q = strchr(p, '/'))) {
        *q = '\0';
        rc = mkdir(buf, mode);
        last = errno;
        *q = '/';
        p = q+1;
    }

    umask(oldmask);

    if ((last == 0) || (last == EEXIST)) {
        return TRUE;
    } else {
        Syslog('?', "$mkdirs(%s)", name);
        return FALSE;
    }
}



/*
 * Return size of file, or -1 if file doesn't exist
 */
long file_size(char *path)
{
    static struct   stat sb;

    if (stat(path, &sb) == -1)
        return -1;

    return sb.st_size;
}



/*
 * Return time of file, or -1 if file doen't exist, which is
 * the same as 1 second before 1 jan 1970. You may test the 
 * result on -1 since time_t is actualy a long integer.
 */
time_t file_time(char *path)
{
    static struct stat sb;

    if (stat(path, &sb) == -1)
        return -1;

    return sb.st_mtime;
}



/*
 * Return ASCII string for node, the bits in 'fl' set the output format.
 */
char *ascfnode(faddr *a, int fl)
{
    static char buf[128];

    buf[0] = '\0';
    if ((fl & 0x08) && (a->zone))
        sprintf(buf+strlen(buf),"%u:",a->zone);
    if (fl & 0x04)
        sprintf(buf+strlen(buf),"%u/",a->net);
    if (fl & 0x02)
        sprintf(buf+strlen(buf),"%u",a->node);
    if ((fl & 0x01) && (a->point))
        sprintf(buf+strlen(buf),".%u",a->point);
    if ((fl & 0x10) && (strlen(a->domain)))
        sprintf(buf+strlen(buf),"@%s",a->domain);
    return buf;
}



/*
 * Return ASCII string for node, the bits in 'fl' set the output format.
 */
char *fido2str(fidoaddr a, int fl)
{
    static char buf[128];

    buf[0] = '\0';
    if ((fl & 0x08) && (a.zone))
	sprintf(buf+strlen(buf),"%u:",a.zone);
    if (fl & 0x04)
	sprintf(buf+strlen(buf),"%u/",a.net);
    if (fl & 0x02)
	sprintf(buf+strlen(buf),"%u",a.node);
    if ((fl & 0x01) && (a.point))
	sprintf(buf+strlen(buf),".%u",a.point);
    if ((fl & 0x10) && (strlen(a.domain)))
	sprintf(buf+strlen(buf),"@%s",a.domain);
    return buf;
}



char *Dos2Unix(char *dosname)
{
    char            buf[PATH_MAX];
    static char     buf2[PATH_MAX];
    char            *p, *q;

    memset(&buf, 0, sizeof(buf));
    memset(&buf2, 0, sizeof(buf2));
    sprintf(buf, "%s", dosname);
    p = buf;

    if (strlen(CFG.dospath)) {
        if (strncasecmp(p, CFG.dospath, strlen(CFG.dospath)) == 0) {
            strcpy((char *)buf2, CFG.uxpath);
            for (p+=strlen(CFG.dospath), q = buf2 + strlen(buf2); *p; p++, q++)
                *q = ((*p) == '\\')?'/':tolower(*p);
	    *q = '\0';
            p = buf2;
        } else {
            if (strncasecmp(p, CFG.uxpath, strlen(CFG.uxpath)) == 0) {
                for (p+=strlen(CFG.uxpath), q = buf2 + strlen(buf2); *p; p++, q++)
                    *q = ((*p) == '\\')?'/':tolower(*p);
                *q = '\0';
                p = buf2;
            }
        }
    }
    return buf2;
}



static char *dow[] = {(char *)"su", (char *)"mo", (char *)"tu", (char *)"we", 
                      (char *)"th", (char *)"fr", (char *)"sa"};

char *dayname(void)
{
    time_t  	tt;
    struct tm	ptm;
    static char	buf[3];

    tt  = time(NULL);
#if defined(__OpenBSD__)
    localtime_r(&tt, &ptm);
#else
    ptm = *localtime(&tt);
#endif
    sprintf(buf, "%s", dow[ptm.tm_wday]);

    return buf;     
}



void InitFidonet(void)
{
    memset(&fidonet, 0, sizeof(fidonet));
}



int SearchFidonet(unsigned short zone)
{
    FILE    *fil;
    char    fidonet_fil[PATH_MAX];
    int	    i;

    sprintf(fidonet_fil, "%s/etc/fidonet.data", getenv("MBSE_ROOT"));
    if ((fil = fopen(fidonet_fil, "r")) == NULL) {
        return FALSE;
    }
    fread(&fidonethdr, sizeof(fidonethdr), 1, fil);

    while (fread(&fidonet, fidonethdr.recsize, 1, fil) == 1) {
        for (i = 0; i < 6; i++) {
            if (zone == fidonet.zone[i]) {
	        fclose(fil);
        	return TRUE;
            }
	}
    }
    fclose(fil);
    return FALSE;
}



char *xmalloc(size_t);
char *xmalloc(size_t size)
{
    char *tmp;

    tmp = malloc(size);
    if (!tmp) 
	abort();
			            
    return tmp;
}



char *printable(char *s, int l)
{
    int     len;
    char    *p;

    if (pbuff) 
	free(pbuff);
    pbuff=NULL;

    if (s == NULL) 
	return (char *)"(null)";

    if (l > 0) 
	len=l;
    else if (l == 0) 
	len=strlen(s);
    else {
	len=strlen(s);
	if (len > -l) 
	    len=-l;
    }
    pbuff=(char*)xmalloc(len*3+1);
    p=pbuff;
    while (len--) {
	if (isprint(*(unsigned char*)s))
	    *p++=*s;
	else switch (*s) {
	    case '\\': *p++='\\'; *p++='\\'; break;
	    case '\r': *p++='\\'; *p++='r'; break;
	    case '\n': *p++='\\'; *p++='n'; break;
	    case '\t': *p++='\\'; *p++='t'; break;
	    case '\b': *p++='\\'; *p++='b'; break;
	    default:   sprintf(p,"\\%02x", (*s & 0xff)); p+=3; break;
	}
	s++;
    }
    *p='\0';
    return pbuff;
}

