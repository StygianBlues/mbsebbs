/*****************************************************************************
 *
 * File ..................: mbuseradd/shadowio.c
 * Purpose ...............: MBSE BBS Shadow Password Suite
 * Last modification date : 13-Aug-2000
 * Original Source .......: Shadow Password Suite
 * Original Copyrioght ...: Julianne Frances Haugh and others.
 *
 *****************************************************************************
 * Copyright (C) 1997-2000
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

#ifdef SHADOW_PASSWORD
#include <string.h>
#include <stdlib.h>
#include <shadow.h>
#include <stdio.h>


#include "commonio.h"
#include "shadowio.h"



struct spwd *__spw_dup(const struct spwd *spent)
{
	struct spwd *sp;

	if (!(sp = (struct spwd *) malloc(sizeof *sp)))
		return NULL;
	*sp = *spent;
	if (!(sp->sp_namp = strdup(spent->sp_namp)))
		return NULL;
	if (!(sp->sp_pwdp = strdup(spent->sp_pwdp)))
		return NULL;
	return sp;
}



static void * shadow_dup(const void *ent)
{
	const struct spwd *sp = ent;
	return __spw_dup(sp);
}



static void shadow_free(void *ent)
{
	struct spwd *sp = ent;

	free(sp->sp_namp);
	free(sp->sp_pwdp);
	free(sp);
}



static const char * shadow_getname(const void *ent)
{
	const struct spwd *sp = ent;
	return sp->sp_namp;
}



static void * shadow_parse(const char *line)
{
	return (void *) sgetspent(line);
}



static int shadow_put(const void *ent, FILE *file)
{
	const struct spwd *sp = ent;
	return (putspent(sp, file) == -1) ? -1 : 0;
}



static struct commonio_ops shadow_ops = {
	shadow_dup,
	shadow_free,
	shadow_getname,
	shadow_parse,
	shadow_put,
	fgets,
	fputs
};



static struct commonio_db shadow_db = {
	SHADOW_FILE,
	&shadow_ops,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	0,
	0
};



int spw_name(const char *filename)
{
	return commonio_setname(&shadow_db, filename);
}



int spw_file_present(void)
{
	return commonio_present(&shadow_db);
}



int spw_lock(void)
{
	return commonio_lock(&shadow_db);
}



int spw_lock_first(void)
{
	return commonio_lock_first(&shadow_db);
}



int spw_open(int mode)
{
	return commonio_open(&shadow_db, mode);
}



const struct spwd * spw_locate(const char *name)
{
	return commonio_locate(&shadow_db, name);
}



int spw_update(const struct spwd *sp)
{
	return commonio_update(&shadow_db, (const void *) sp);
}



int spw_remove(const char *name)
{
	return commonio_remove(&shadow_db, name);
}



int spw_rewind(void)
{
	return commonio_rewind(&shadow_db);
}



const struct spwd * spw_next(void)
{
	return commonio_next(&shadow_db);
}



int spw_close(void)
{
	return commonio_close(&shadow_db);
}



int spw_unlock(void)
{
	return commonio_unlock(&shadow_db);
}



struct commonio_entry * __spw_get_head(void)
{
	return shadow_db.head;
}



void __spw_del_entry(const struct commonio_entry *ent)
{
	commonio_del_entry(&shadow_db, ent);
}


#endif

