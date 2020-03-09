/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sv_user.c -- server code for moving users

#include "server.h"

edict_t	*sv_player;


#if KINGPIN
download_t *downloads = NULL;
#if 0
#ifdef _WIN32
static unsigned __stdcall CacheDownload(void *arg)
#else
static void *CacheDownload(void *arg)
#endif
{
	download_t *d = (download_t*)arg;
	int c;
	z_stream zs;

	c = d->size - d->offset;

	if (d->compbuf)
	{
		if (sv_compress_downloads->intvalue > Z_BEST_COMPRESSION)
			Cvar_Set ("sv_compress_downloads", va("%d", Z_BEST_COMPRESSION));
		memset(&zs, 0, sizeof(zs));
		deflateInit2(&zs, sv_compress_downloads->intvalue, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
		zs.next_out = d->compbuf;
		zs.avail_out = c - 1;
	}

	do
	{
		char buf[0x4000];
		int r = read(d->fd, buf, c < sizeof(buf) ? c : sizeof(buf));
		if (r <= 0)
			break;
		c -= r;
		if (d->compbuf)
		{
			if (!zs.next_in)
			{
				zs.next_in = buf;
				zs.avail_in = 1024;
				deflate(&zs, Z_PARTIAL_FLUSH);
				zs.avail_in += r - 1024;
			}
			else
			{
				zs.next_in = buf;
				zs.avail_in = r;
			}
			if ((r = deflate(&zs, 0)) || !zs.avail_out)
			{
				if (r)
					Com_Printf ("Download compression error (%d)\n", LOG_SERVER|LOG_DOWNLOAD|LOG_WARNING, r);
				break;
			}
		}
	}
	while (c > 0);
	if (d->compbuf)
	{
		if (!c && deflate(&zs, Z_FINISH) == Z_STREAM_END)
		{
			d->compsize = zs.total_out;
			d->compbuf = Z_Realloc(d->compbuf, d->compsize);
			Com_Printf ("Compressed %s from %d to %d (%d%%)\n", LOG_SERVER|LOG_DOWNLOAD|LOG_NOTICE, d->name, d->size - d->offset, d->compsize, 100 * d->compsize / (d->size - d->offset));
		}
		else
		{
			Z_Free(d->compbuf);
			d->compbuf = NULL;
		}
		deflateEnd(&zs);
	}
	close(d->fd);
	d->fd = -1;
	return 0;
}

download_t *NewCachedDownload(client_t *cl, qboolean compress)
{
	struct stat	s;
	download_t *d = downloads;
	if (compress)
		fstat(fileno(cl->download), &s);
	while (d)
	{
		if (!strcmp(d->name, cl->downloadFileName))
		{
			if (compress)
			{
				if (d->compbuf && d->offset == cl->downloadoffset)
				{
					if (d->mtime == s.st_mtime)
					{
						d->refc++;
						return d;
					}
					if (!d->refc)
					{
						d->refc = 1;
						d->size = cl->downloadsize;
						d->mtime = s.st_mtime;
						d->compsize = 0;
						d->compbuf = Z_Realloc(d->compbuf, d->size - d->offset);
						d->fd = dup(fileno(cl->download));
						d->thread = Sys_StartThread(CacheDownload, d, -1);
						return d;
					}
				}
			}
			else if (d->offset <= cl->downloadoffset)
			{
				if (d->fd == -1)
					return NULL;
				d->refc++;
				return d;
			}
		}
		d = d->next;
	}

	d = Z_TagMalloc(sizeof(*d), TAGMALLOC_DOWNLOAD_CACHE);
	memset(d, 0, sizeof(*d));
	d->refc = 1;
	d->size = cl->downloadsize;
	d->offset = cl->downloadoffset;
	if (compress)
	{
		d->mtime = s.st_mtime;
		d->compbuf = Z_TagMalloc(d->size - d->offset, TAGMALLOC_DOWNLOAD_CACHE);
		if (!d->compbuf)
		{
			Z_Free(d);
			return NULL;
		}
	}
	d->name = CopyString (cl->downloadFileName, TAGMALLOC_DOWNLOAD_CACHE);
	d->fd = dup(fileno(cl->download));
	d->thread = Sys_StartThread(CacheDownload, d, -1);
	d->next = downloads;
	downloads = d;
	return d;
}
#endif

void ReleaseCachedDownload(download_t *download)
{
	download_t *d = downloads, *p = NULL;
	while (d)
	{
		if (d == download)
		{
			d->refc--;
			if (!d->refc && d->compbuf && d->offset)
			{
				if (p)
					p->next = d->next;
				else
					downloads = d->next;
				Sys_WaitThread(d->thread);
				Z_Free(d->compbuf);
				Z_Free(d->name);
				Z_Free(d);
			}
			return;
		}
		p = d;
		d = d->next;
	}
}
#endif




/*
============================================================

USER STRINGCMD EXECUTION

sv_client and sv_player will be valid.
============================================================
*/

/*
==================
SV_BeginDemoServer
==================
*/
void SV_BeginDemoserver (void)
{
	char		name[MAX_OSPATH];

	Com_sprintf (name, sizeof(name), "demos/%s", sv.name);
	FS_FOpenFile (name, &sv.demofile, FS_READ);
	if (!sv.demofile)
		Com_Error (ERR_DROP, "Couldn't open %s\n", name);
}

#if KINGPIN
static void SV_BaselinesMessage (qboolean userCmd);

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
#if 0
static void SV_CreateBaseline (client_t *cl)
{
	edict_t			*svent;
	int				entnum;

	memset (cl->lastlines, 0, sizeof(entity_state_t) * MAX_EDICTS);

	for (entnum = 1; entnum < ge->num_edicts ; entnum++)
	{
		svent = EDICT_NUM(entnum);

		if (!svent->inuse)
			continue;

#if KINGPIN
		// MH: include props in baselines
		if (!svent->s.num_parts)
#endif
		if (!svent->s.modelindex && !svent->s.sound && !svent->s.effects)
			continue;

		svent->s.number = entnum;

		//
		// take current state as baseline
		//
		//VectorCopy (svent->s.origin, svent->s.old_origin);
		cl->lastlines[entnum] = svent->s;

		// MH: don't bother including player positions/angles as they'll soon change
		if (svent->client)
		{
			memset(&cl->lastlines[entnum].origin, 0, sizeof(cl->lastlines[entnum].origin));
			memset(&cl->lastlines[entnum].angles, 0, sizeof(cl->lastlines[entnum].angles));
#if KINGPIN
			// directional lighting too
			memset(&cl->lastlines[entnum].model_lighting, 0, sizeof(cl->lastlines[entnum].model_lighting));
#endif
		}
		// MH: don't include frames/events either for the same reason
		cl->lastlines[entnum].frame = cl->lastlines[entnum].event = 0;

	VectorCopy(cl->lastlines[entnum].origin, cl->lastlines[entnum].old_origin);
	}
}


static void SV_AddConfigstrings (void)
{
	int		start;
	int		wrote;
	int		len;

	if (sv_client->state != cs_spawning)
	{
		//r1: dprintf to avoid console spam from idiot client
		Com_Printf ("configstrings for %s not valid -- not spawning\n", sv_client->name);
		return;
	}

	start = 0;
	wrote = 0;

	// write a packet full of data

	{
		while (start < MAX_CONFIGSTRINGS)
		{
#if KINGPIN
			int cs = start;
			// MH: send downloadables, possibly in place of images
			if (cs >= CS_IMAGES && cs < CS_IMAGES + MAX_IMAGES && (sv.dlconfigstrings[cs - CS_IMAGES][0] || sv.dlconfigstrings[cs - CS_IMAGES - 1][0]))
				cs += MAX_CONFIGSTRINGS - CS_IMAGES;
			if (sv.configstrings[cs][0])
#else
			if (sv.configstrings[start][0])
#endif
			{
#if KINGPIN
				len = (int)strlen(sv.configstrings[cs]);
#else
				len = (int)strlen(sv.configstrings[start]);
#endif

				len = len > MAX_QPATH ? MAX_QPATH : len;
				SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
				//MSG_BeginWriting (svc_configstring);
				MSG_WriteByte (&sv_client->netchan.message, svc_configstring);
				MSG_WriteShort (&sv_client->netchan.message, start);
#if KINGPIN
				MSG_WriteString(&sv_client->netchan.message, sv.configstrings[cs]); //hypov8 add
				//MSG_Write (sv.configstrings[cs], len);
#else
				MSG_Write (sv.configstrings[start], len);
#endif
				//MSG_Write ("\0", 1);
				MSG_WriteString(&sv_client->netchan.message, "\0"); //hypov8 add

				// MH: count full message length
				wrote += MSG_GetLength();
				SV_AddMessage (sv_client, true);

				//we add in a stuffcmd every 500 bytes to ensure that old clients will transmit a
				//netchan ack asap. uuuuuuugly...
#if KINGPIN
				if (/*sv_client->patched < 4 &&*/ wrote >= 1300) // MH: patched client doesn't need this
#else
				if (wrote >= 500)
#endif
				{
					SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
					//MSG_BeginWriting (svc_stufftext);
					MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
					
					MSG_WriteString (&sv_client->netchan.message, "cmd \177n\n");
					SV_AddMessage (sv_client, true);
					//===todo:=====SZ_Write
					//===todo:=====wrote = 0;
				}
			}
			start++;
		}
	}

	// send next command

	SV_BaselinesMessage (false);
}
#endif

#endif

/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_New_f (void)
{
	char		*gamedir;
	int			playernum;
	edict_t		*ent;

	Com_DPrintf ("New() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
	#if  0 //KINGPIN
		if (sv_client->state == cs_spawning)
		{
			//client typed 'reconnect/new' while connecting.
			SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
			//MSG_BeginWriting (&sv_client->netchan.message,svc_stufftext);
			MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
			MSG_WriteString (&sv_client->netchan.message,"\ndisconnect\nreconnect\n");
			SV_AddMessage (sv_client, true);
			SV_DropClient (sv_client, true);
			//SV_WriteReliableMessages (sv_client, sv_client->netchan.message.buffsize);
		}
		else
	#endif	
		{
			//shouldn't be here!
			Com_DPrintf ("WARNING: Illegal 'new' from %s, client state %d. This shouldn't happen...\n", sv_client->name, sv_client->state);
		}
		return;
	}

	// demo servers just dump the file message
	if (sv.state == ss_demo)
	{
		SV_BeginDemoserver ();
		return;
	}

#if 0 //KINGPIN	
	//r1: new client state now to prevent multiple new from causing high cpu / overflows.
	sv_client->state = cs_spawning;

	//r1: fix for old clients that don't respond to stufftext due to pending cmd buffer
	SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
	//MSG_BeginWriting (&sv_client->netchan.message,svc_stufftext);
	MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
	MSG_WriteString (&sv_client->netchan.message,"\n");
	SV_AddMessage (sv_client, true);

	//if (SV_UserInfoBanned (sv_client)) //hypov8 todo:?
		//return;
#endif		
	//
	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	//
#if KINGPIN
	// MH: send client-side "gamedir" value
	gamedir = Cvar_VariableString ("clientdir");
#else
	gamedir = Cvar_VariableString ("gamedir");
#endif

	// send the serverdata
	SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
	//MSG_BeginWriting (svc_serverdata);
	MSG_WriteByte (&sv_client->netchan.message, svc_serverdata);
#if KINGPIN //hypov8 todo: server
	MSG_WriteLong (&sv_client->netchan.message, OLD_PROTOCOL_VERSION);
#else
	MSG_WriteLong (&sv_client->netchan.message, PROTOCOL_VERSION);
#endif
	MSG_WriteLong (&sv_client->netchan.message, svs.spawncount);
	MSG_WriteByte (&sv_client->netchan.message, sv.attractloop);
	MSG_WriteString (&sv_client->netchan.message, gamedir);

	if (sv.state == ss_cinematic || sv.state == ss_pic)
		playernum = -1;
	else
		playernum = sv_client - svs.clients;
	MSG_WriteShort (&sv_client->netchan.message, playernum);

	// send full levelname
	MSG_WriteString (&sv_client->netchan.message, sv.configstrings[CS_NAME]);
#if 0 //KINGPIN
	SV_AddMessage (sv_client, true);

	//r1: we have to send another \n in case serverdata caused game switch -> autoexec without \n
	//this will still cause failure if the last line of autoexec exec's another config for example.
	SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
	//MSG_BeginWriting (svc_stufftext);
	MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
	MSG_WriteString (&sv_client->netchan.message, "\n");
	SV_AddMessage (sv_client, true);
	//===todo:=====NET_SendPacket
	//SV_WriteClientdataToMessage(sv_client, &sv_client->netchan.message);
	//===todo:=====MSG_WriteDeltaEntity
#endif

	//
	// game server
	// 
	if (sv.state == ss_game)
	{
		// set up the entity for the client
		ent = EDICT_NUM(playernum+1);
		ent->s.number = playernum+1;
		sv_client->edict = ent;
		memset (&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));
#if 0 //KINGPIN
		//r1: per-client baselines
		SV_CreateBaseline (sv_client);
		SV_AddConfigstrings ();
#else
		// begin fetching configstrings
		SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i 0\n",svs.spawncount) );
#endif
	}
#if 0 //KINGPIN
	else if (sv.state == ss_pic || sv.state == ss_cinematic)
	{
		SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
		//MSG_BeginWriting (svc_stufftext);
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd begin %i\n", svs.spawncount));
		SV_AddMessage (sv_client, true);
	}
#endif
}

#if 0 //KINGPIN

/*
==================
SV_Baselines_f
==================
*/
static void SV_BaselinesMessage (qboolean userCmd)
{
	int				startPos;
	int				start;
	int				wrote;

	entity_state_t	*base;

	Com_DPrintf ("Baselines() from %s\n", sv_client->name);

	if (sv_client->state != cs_spawning)
	{
		Com_DPrintf ("%s: baselines not valid -- not spawning\n", sv_client->name);
		return;
	}
	
	// handle the case of a level changing while a client was connecting
	if (userCmd)
	{
		if ( atoi(Cmd_Argv(1)) != svs.spawncount)
		{
			Com_Printf ("SV_Baselines_f from %s from a different level\n", sv_client->name);
			SV_New_f ();
			return;
		}

		startPos = atoi(Cmd_Argv(2));
	}
	else
	{
		startPos = 0;
	}


	/*//r1: huge security fix !! remote DoS by negative here. //hypov8 todo:
	if (startPos < 0)
	{
		Com_Printf ("Illegal baseline offset from %s[%s], client dropped\n", LOG_SERVER|LOG_EXPLOIT, sv_client->name, NET_AdrToString (&sv_client->netchan.remote_address));
		Blackhole (&sv_client->netchan.remote_address, true, sv_blackhole_mask->intvalue, BLACKHOLE_SILENT, "attempted DoS (negative baselines)");
		SV_DropClient (sv_client, false);
		return;
	}*/

	start = startPos;
	wrote = 0;

	// write a packet full of data
	//r1: use new per-client baselines
#if !defined(NO_ZLIB) && !KINGPIN
	if (sv_client->protocol == PROTOCOL_ORIGINAL)
#endif
	{
#if KINGPIN && 0 // not needed with parental lock check
		if (sv_client->patched < 4)
		{
			// MH: stuffing one at the start and then every 1300 bytes (below)
			MSG_BeginWriting (svc_stufftext);
			MSG_WriteString ("cmd \177n\n");
			SV_AddMessage (sv_client, true);
		}
#endif

#if !defined(NO_ZLIB) && !KINGPIN
plainLines:
#endif
		start = startPos;
		while (start < MAX_EDICTS)
		{
			base = &sv_client->lastlines[start];
			if (base->number)
			{

				SZ_Clear (&sv_client->netchan.message); //hypov8 add: ??
				//MSG_BeginWriting (svc_spawnbaseline);
				MSG_WriteByte (&sv_client->netchan.message, svc_spawnbaseline);
				
#if KINGPIN
				MSG_WriteDeltaEntity (&null_entity_state, base, true, true, false, 100);
				//SV_WriteDeltaEntity (&null_entity_state, base, true, true, false, 100);
#else
				SV_WriteDeltaEntity (&null_entity_state, base, true, true, sv_client->protocol, sv_client->protocol_version);
#endif
				wrote += MSG_GetLength();
				SV_AddMessage (sv_client, true);

				//we add in a stuffcmd every 500 bytes to ensure that old clients will transmit a
				//netchan ack asap. uuuuuuugly...
#if KINGPIN
				if (/*sv_client->patched < 4 &&*/ wrote >= 1300) // MH: patched client doesn't need this
#else
				if (wrote >= 500)
#endif
				{
					MSG_BeginWriting (svc_stufftext);
					MSG_WriteString ("cmd \177n\n","");//===todo:=====
					SV_AddMessage (sv_client, true);
					wrote = 0;
				}

			}
			start++;
		}
	}


	// send next command
	MSG_BeginWriting (svc_stufftext);
	MSG_WriteString (va("precache %i\n", svs.spawncount),"");//===todo:=====
	SV_AddMessage (sv_client, true);
}

#endif

// Knightmare added
/*
==================
SV_SetMaxBaselinesSize
==================
*/
int SV_SetMaxBaselinesSize (void)
{
	//	bounds check sv_baselines_maxlen
	if (sv_baselines_maxlen->value < 400)
	{
		Com_Printf (S_COLOR_YELLOW"WARNING: sv_baselines_maxlen is less than 400!  Setting to default value of 1200.\n");
		Cvar_Set ("sv_baselines_maxlen", "1200");
	}
	if (sv_baselines_maxlen->value > MAX_MSGLEN)
	{
		Com_Printf (S_COLOR_YELLOW"WARNING: sv_baselines_maxlen is larger than MAX_MSGLEN of %i!  Setting to default value of 1200.\n", MAX_MSGLEN);
		Cvar_Set ("sv_baselines_maxlen", "1200");
	}
	if (sv_baselines_maxlen->value > 1400)
	{
		Com_Printf (S_COLOR_YELLOW"WARNING: sv_baselines_maxlen is larger than 1400!  Setting to default value of 1200.\n");
		Cvar_Set ("sv_baselines_maxlen", "1200");
	}

	// use MAX_MSGLEN/2 for SP and local clients
	if ( (sv_client->netchan.remote_address.type == NA_LOOPBACK) || ((int)maxclients->value == 1) )
		return MAX_MSGLEN/2;
	else
		return sv_baselines_maxlen->integer;
}

/*
==================
SV_Configstrings_f
==================
*/
void SV_Configstrings_f (void)
{
	int			startPos, start;
	int		maxLen;		// Knightmare added

	Com_DPrintf ("Configstrings() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf ("configstrings not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Com_Printf ("SV_Configstrings_f from different level\n");
		SV_New_f ();
		return;
	}

	//	Knightmare- use sv_baselines_maxlen for proper bounding in multiplayer
	maxLen = SV_SetMaxBaselinesSize();

//	start = atoi(Cmd_Argv(2));
	startPos = atoi(Cmd_Argv(2));
	if (startPos < 0) // r1ch's fix for negative index
	{
		Com_Printf ("Illegal configstrings request (negative index) from %s[%s], dropping client\n", sv_client->name, NET_AdrToString(sv_client->netchan.remote_address));
		SV_DropClient (sv_client);
		return;
	}
	start = startPos;

	// write a packet full of data
	//	Knightmare- use maxLen for proper bounding
//	while ( sv_client->netchan.message.cursize < MAX_MSGLEN/2 && start < MAX_CONFIGSTRINGS)
	while ( sv_client->netchan.message.cursize < maxLen && start < MAX_CONFIGSTRINGS)
	{
		if (sv.configstrings[start][0])
		{
			MSG_WriteByte (&sv_client->netchan.message, svc_configstring);
			MSG_WriteShort (&sv_client->netchan.message, start);
			MSG_WriteString (&sv_client->netchan.message, sv.configstrings[start]);
		}
		start++;
	}

	// send next command

	if (start == MAX_CONFIGSTRINGS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i 0\n",svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i %i\n",svs.spawncount, start) );
	}
}

/*
==================
SV_Baselines_f
==================
*/
void SV_Baselines_f (void)
{
	int				startPos, start;
	int				maxLen;	// Knightmare added
	entity_state_t	nullstate;
	entity_state_t	*base;

	Com_DPrintf ("Baselines() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf ("baselines not valid -- already spawned\n");
		return;
	}
	
	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Com_Printf ("SV_Baselines_f from different level\n");
		SV_New_f ();
		return;
	}

	//	Knightmare- use sv_baselines_maxlen for proper bounding in multiplayer
	maxLen = SV_SetMaxBaselinesSize();

//	start = atoi(Cmd_Argv(2));
	startPos = atoi(Cmd_Argv(2));
	if (startPos < 0) // r1ch's fix for negative index
	{
		Com_Printf ("Illegal baselines request (negative index) from %s[%s], dropping client\n", sv_client->name, NET_AdrToString(sv_client->netchan.remote_address));
		SV_DropClient (sv_client);
		return;
	}
	start = startPos;

	memset (&nullstate, 0, sizeof(nullstate));

	// write a packet full of data
	//	Knightmare- use maxLen for proper bounding
//	while ( sv_client->netchan.message.cursize <  MAX_MSGLEN/2 && start < MAX_EDICTS)
	while ( sv_client->netchan.message.cursize < maxLen && start < MAX_EDICTS)
	{
		base = &sv.baselines[start];
		if (base->modelindex || base->sound || base->effects)
		{
			MSG_WriteByte (&sv_client->netchan.message, svc_spawnbaseline);
			MSG_WriteDeltaEntity (&nullstate, base, &sv_client->netchan.message, true, true);
		}
		start++;
	}

	// send next command

	if (start == MAX_EDICTS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("precache %i\n", svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i %i\n",svs.spawncount, start) );
	}
}

/*
==================
SV_Begin_f
==================
*/
void SV_Begin_f (void)
{
	Com_DPrintf ("Begin() from %s\n", sv_client->name);

	// r1ch: could be abused to respawn or cause spam/other mod specific problems
	if (sv_client->state != cs_connected)
	{
		Com_Printf ("EXPLOIT: Illegal 'begin' from %s[%s] (already spawned), client dropped.\n", sv_client->name, NET_AdrToString (sv_client->netchan.remote_address));
		SV_DropClient (sv_client);
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Com_Printf ("SV_Begin_f from different level\n");
		SV_New_f ();
		return;
	}

	sv_client->state = cs_spawned;
	
	// call the game begin function
	ge->ClientBegin (sv_player);

	// Knightmare- set default player speeds here, if
	// the game DLL hasn't already set them
#ifdef NEW_PLAYER_STATE_MEMBERS
#if KINGPIN //hypov8 todo: SV_Begin_f
	if (sv_player->client)
#endif
	{
	if (!sv_player->client->ps.maxspeed)
		sv_player->client->ps.maxspeed = DEFAULT_MAXSPEED;
	if (!sv_player->client->ps.duckspeed)
		sv_player->client->ps.duckspeed = DEFAULT_DUCKSPEED;
	if (!sv_player->client->ps.waterspeed)
		sv_player->client->ps.waterspeed = DEFAULT_WATERSPEED;
	if (!sv_player->client->ps.accel)
		sv_player->client->ps.accel = DEFAULT_ACCELERATE;
	if (!sv_player->client->ps.stopspeed)
		sv_player->client->ps.stopspeed = DEFAULT_STOPSPEED;
	}
#endif
	// end Knightmare

	Cbuf_InsertFromDefer ();
}

//=============================================================================

/*
==================
SV_NextDownload_f
==================
*/
#if !KINGPIN
void SV_NextDownload_f (void)
{
	int		r;
	int		percent;
	int		size;

	if (!sv_client->download)
		return;

	r = sv_client->downloadsize - sv_client->downloadcount;
	if (r > 1024)
		r = 1024;

	MSG_WriteByte (&sv_client->netchan.message, svc_download);
	MSG_WriteShort (&sv_client->netchan.message, r);

	sv_client->downloadcount += r;
	size = sv_client->downloadsize;
	if (!size)
		size = 1;
	percent = sv_client->downloadcount*100/size;
	MSG_WriteByte (&sv_client->netchan.message, percent);
	SZ_Write (&sv_client->netchan.message,
		sv_client->download + sv_client->downloadcount - r, r);

	if (sv_client->downloadcount != sv_client->downloadsize)
		return;

	FS_FreeFile (sv_client->download);
	sv_client->download = NULL;
}
#endif

#if KINGPIN
// MH: send a download message to a client
void PushDownload (client_t *cl, qboolean start)
{
	int		r;

	if (start && cl->downloadcache)
	{
		if (cl->downloadcache->compbuf)
		{
			// starting a compressed download
			cl->downloadsize = cl->downloadcache->compsize;
			cl->downloadpos = cl->downloadcount = 0;
		}
		else
		{
			// starting a cached download
			ReleaseCachedDownload(cl->downloadcache);
			cl->downloadcache = NULL;
			fseek(cl->download, cl->downloadstart + cl->downloadoffset, SEEK_SET);
		}
	}

	r = cl->downloadsize - cl->downloadpos;
	if (r < 0)
		return;
#if 0
	if (cl->patched >= 4)
	{
		// svc_xdownload message for patched client
		if (r > 1366)
			r = 1366;

		MSG_BeginWriting (svc_xdownload);
		MSG_WriteShort (r);
		MSG_WriteLong (cl->downloadid);
		if (start)
		{
			MSG_WriteLong (-cl->downloadoffset >> 10);
			MSG_WriteLong (cl->downloadsize);
			MSG_WriteLong (cl->downloadcache ? cl->downloadcache->size : 0);
		}
		else if (cl->downloadcache)
			MSG_WriteLong (cl->downloadpos / 1366);
		else
			MSG_WriteLong ((cl->downloadpos - cl->downloadoffset) / 1366);
		if (cl->downloadcache)
			MSG_Write (cl->downloadcache->compbuf + cl->downloadpos, r);
		else 
			FS_Read(SZ_GetSpace(MSG_GetRawMsg(), r), r, cl->download);
	}
	else
#endif
	{
		// svc_pushdownload message
		if (r > 1024)
			r = 1024;

		//MSG_BeginWriting (svc_pushdownload);
		SZ_Clear (&sv_client->netchan.message);
		MSG_WriteByte (&sv_client->netchan.message, svc_pushdownload/*svc_download*/); //add hypov8
		MSG_WriteShort (&sv_client->netchan.message, r);
		MSG_WriteLong (&sv_client->netchan.message, cl->downloadsize);
		MSG_WriteLong (&sv_client->netchan.message, cl->downloadid);
		MSG_WriteLong (&sv_client->netchan.message, cl->downloadpos >> 10);
		FS_Read(SZ_GetSpace(&sv_client->netchan.message/*MSG_GetRawMsg()*/, r), r, cl->download);
	}

	cl->downloadpos += r;
	if (cl->downloadcount < cl->downloadpos)
		cl->downloadcount = cl->downloadpos;

	// first and last blocks are sent in reliable messages
	if (!start && cl->downloadcount != cl->downloadsize)
	{
		int msglen =&sv_client->netchan.message.cursize;
		int packetdup = cl->netchan.packetdup;
		// send reliable separately first if needed to avoid overflow
		int send_reliable =
			(
				(	cl->netchan.incoming_acknowledged > cl->netchan.last_reliable_sequence &&
					cl->netchan.incoming_reliable_acknowledged != cl->netchan.reliable_sequence &&
					cl->netchan.reliable_length + msglen > MAX_MSGLEN - 8
				)
				||
				(
					!cl->netchan.reliable_length && cl->netchan.message.cursize + msglen > MAX_MSGLEN - 8
				)
			);
		if (send_reliable)
			Netchan_Transmit (&cl->netchan, 0, NULL);
		cl->netchan.packetdup = 0; // disable duplicate packets (client will re-request lost blocks)
		Netchan_Transmit (&cl->netchan, msglen, &sv_client->netchan.message/* MSG_GetData()*/);
		cl->netchan.packetdup = packetdup;
		//MSG_FreeData();
		SZ_Clear (&cl->netchan.message); //hypov8 add: Netchan_Transmit
	}
	else
	{
		//SV_AddMessage(cl, true); //hypov8 todo: 
	}

	if (cl->downloadcount != cl->downloadsize)
		return;

	// free download stuff
	SV_CloseDownload(cl);
}

// MH: handle a download chunk request
void SV_NextPushDownload_f (void)
{
	int offset;
	int n;

	if (!sv_client->download || (sv_client->downloadcache && sv_client->downloadsize != sv_client->downloadcache->compsize))
		return;

	for (n=1;; n++)
	{
		if (!Cmd_Argv(n)[0])
			break;

		// check that the client has tokens available if bandwidth is limited
		///if (sv_bandwidth->intvalue > 0) //hypov8 todo:
		{
			if (sv_client->downloadtokens < 1)
				return;
			sv_client->downloadtokens -= (/*sv_client->patched >= 4 ? 1.333f :*/ 1);
		}
#if 0
		if (sv_client->patched >= 4)
		{
			offset = atoi(Cmd_Argv(n)) * 1366;
			if (!sv_client->downloadcache)
				offset += sv_client->downloadoffset;
		}
		else
#endif
			offset = atoi(Cmd_Argv(n)) << 10;
		if (offset < 0 || offset >= sv_client->downloadsize)
			return;

		if (sv_client->downloadpos != offset)
		{
			// seek to requested position
			sv_client->downloadpos = offset;
			if (!sv_client->downloadcache)
				fseek(sv_client->download, sv_client->downloadstart + sv_client->downloadpos, SEEK_SET);
		}

		PushDownload(sv_client, false);

		// patched clients can request multiple chunks at a time
		if (/*sv_client->patched < 4 ||*/ !sv_client->download)
			break;
	}
}

// MH: calculate per-client download speed limit
int GetDownloadRate()
{
	int dlrate;
#if 0 //hypov8 todo: sv_bandwidth
	if (sv_bandwidth->intvalue)
	{
		int j, c = 0, cp = 0;
		int bw = sv_bandwidth->intvalue;
		for (j=0 ; j<maxclients->intvalue ; j++)
		{
			if (svs.clients[j].state <= cs_zombie)
				continue;
			if (svs.clients[j].download)
			{
				c++;
				if (svs.clients[j].patched)
					cp++;
			}
			else
				bw -= (svs.clients[j].rate > 14000 ? 14000 : svs.clients[j].rate) / 1000;
		}
		dlrate = bw / (c ? c : 1);
		if (dlrate > 200 && cp)
		{
			dlrate = (bw - (c - cp) * 200) / cp;
			if (dlrate > 1000) // no higher than 1MB/s
				dlrate = 1000;
		}
		else if (dlrate < 30) // no lower than 30KB/s
			dlrate = 30;
	}
	else
#endif
		dlrate = 1000; // default to 1MB/s
	return dlrate;
}

// MH: check if a file is in the downloadables list
static qboolean IsDownloadable(const char *file)
{
	int i;

	for (i=0; i<MAX_IMAGES; i++)
	{
		if (!sv.dlconfigstrings[i][0])
			break;
		if (!Q_stricmp(file, sv.dlconfigstrings[i]))
			return true;
	}
	return false;
}
#endif
/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f(void)
{
	char		*name, *p;
	size_t		length;
	qboolean	valid;
//	extern int	file_from_pak; // ZOID did file come from pak?

	int offset = 0;

	name = Cmd_Argv(1);

	if (Cmd_Argc() > 2)
		offset = atoi(Cmd_Argv(2)); // downloaded offset

	// r1ch fix: name is always filtered for security reasons
	StripHighBits (name, 1);

	// hacked by zoid to allow more conrol over download
	// first off, no .. or global allow check

	// r1ch fix: for  some ./ references in maps, eg ./textures/map/file
	length = strlen(name);
	p = name;
	while ((p = strstr (p, "./")))
	{
		memmove (p, p+2, length - (p - name) - 1);
		length -= 2;
	}

	// r1ch fix: block the really nasty ones - \server.cfg will download from mod root on win32, .. is obvious
	if (name[0] == '\\' || strstr (name, ".."))
	{
		Com_Printf ("Refusing illegal download path %s to %s\n", name, sv_client->name);
		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		Com_Printf ("Client %s[%s] tried to download illegal path: %s\n", sv_client->name, NET_AdrToString (sv_client->netchan.remote_address), name);
		SV_DropClient (sv_client);
		return;
	}
	else if (offset < 0) // r1ch fix: negative offset will crash on read
	{
		Com_Printf ("Refusing illegal download offset %d to %s\n", offset, sv_client->name);
		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		Com_Printf ("Client %s[%s] supplied illegal download offset for %s: %d\n", sv_client->name, NET_AdrToString (sv_client->netchan.remote_address), name, offset);
		SV_DropClient (sv_client);
		return;
	}
	else if ( !length || name[0] == 0 // empty name, maybe as result of ./ normalize
			|| !IsValidChar(name[0])
			// r1ch: \ is bad in general, client won't even write properly if we do sent it
			|| strchr (name, '\\')
			// MUST be in a subdirectory, unless a pk3	
			|| (!strstr (name, "/") && strcmp(name+strlen(name)-4, ".pk3"))
			// r1ch: another bug, maps/. will fopen(".") -> crash
			|| !IsValidChar(name[length-1]) )
/*	if (strstr (name, "..") || !allow_download->value
		// leading dot is no good
		|| *name == '.' 
		// leading slash bad as well, must be in subdir
		|| *name == '/'
		// next up, skin check
		|| (strncmp(name, "players/", 8) == 0 && !allow_download_players->value)
		// now models
		|| (strncmp(name, "models/", 7) == 0 && !allow_download_models->value)
		// now sounds
		|| (strncmp(name, "sound/", 6) == 0 && !allow_download_sounds->value)
		// now maps (note special case for maps, must not be in pak)
		|| (strncmp(name, "maps/", 5) == 0 && !allow_download_maps->value)
		// MUST be in a subdirectory, unless a pk3	
		|| (!strstr (name, "/") && strcmp(name+strlen(name)-4, ".pk3")) )	*/
	{	// don't allow anything with .. path
		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

	valid = true;

	if ( !allow_download->value
		|| (strncmp(name, "players/", 8) == 0 && !allow_download_players->value)
		|| (strncmp(name, "models/", 7) == 0 && !allow_download_models->value)
		|| (strncmp(name, "sound/", 6) == 0 && !allow_download_sounds->value)
		|| (strncmp(name, "maps/", 5) == 0 && !allow_download_maps->value)
		|| (strncmp(name, "pics/", 5) == 0 && !allow_download_pics->value)
		|| ( ((strncmp(name, "env/", 4) == 0 || strncmp(name, "textures/", 9) == 0)) && !allow_download_textures->value ) )
		valid = false;

	if (!valid)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

	if (sv_client->download)
		FS_FreeFile (sv_client->download);

	sv_client->downloadsize = FS_LoadFile (name, (void **)&sv_client->download);
	sv_client->downloadcount = offset;

	if (offset > sv_client->downloadsize)
		sv_client->downloadcount = sv_client->downloadsize;

	// ZOID- special check for maps, if it came from a pak file, don't allow download  
	if (!sv_client->download || (strncmp(name, "maps/", 5) == 0 && file_from_pak))
	{
		Com_DPrintf ("Couldn't download %s to %s\n", name, sv_client->name);
		if (sv_client->download)
		{
			FS_FreeFile (sv_client->download);
			sv_client->download = NULL;
		}

		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}
#if KINGPIN //hypov8 todo: SV_NextDownload_f

#else
	SV_NextDownload_f ();
#endif
	Com_DPrintf ("Downloading %s to %s\n", name, sv_client->name);
}



// MH: free download stuff
void SV_CloseDownload(client_t *cl)
{
	if (!cl->download)
		return;

	fclose (cl->download);
	cl->download = NULL;
	cl->downloadsize = 0;

	Z_Free (cl->downloadFileName);
	cl->downloadFileName = NULL;

#if KINGPIN
	if (cl->downloadcache)
	{
		ReleaseCachedDownload(cl->downloadcache);
		cl->downloadcache = NULL;
	}
#endif
}



//============================================================================


/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
void SV_Disconnect_f (void)
{
//	SV_EndRedirect ();
	SV_DropClient (sv_client);	
}


/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
void SV_ShowServerinfo_f (void)
{
//	Info_Print (Cvar_Serverinfo());

	// r1ch: this is a client issued command !
	char *s;
	char *p;
	int flip;

	s = Cvar_Serverinfo();

	// skip beginning \\ char
	s++;

	flip = 0;
	p = s;

	// make it more readable
	while (p[0])
	{
		if (p[0] == '\\')
		{
			if (flip)
				p[0] = '\n';
			else
				p[0] = '=';
			flip ^= 1;
		}
		p++;
	}

	SV_ClientPrintf (sv_client, PRINT_HIGH, "%s\n", s);
	// end r1ch fix
}


void SV_Nextserver (void)
{
	char	*v;

	// ZOID, ss_pic can be nextserver'd in coop mode
	if (sv.state == ss_game || (sv.state == ss_pic && !Cvar_VariableValue("coop")))
		return;		// can't nextserver while playing a normal game

	svs.spawncount++;	// make sure another doesn't sneak in
	v = Cvar_VariableString ("nextserver");
	if (!v[0])
	{
	//	Com_DPrintf ("SV_Nextserver: Null nextserver!\n");
		Cbuf_AddText ("killserver\n");
	}
	else
	{
	//	Com_DPrintf ("SV_Nextserver: Changing to nextserver %s.\n", v);
		Cbuf_AddText (v);
		Cbuf_AddText ("\n");
	}
	Cvar_Set ("nextserver","");
}

/*
==================
SV_Nextserver_f

A cinematic has completed or been aborted by a client, so move
to the next server,
==================
*/
void SV_Nextserver_f (void)
{
	if ( atoi(Cmd_Argv(1)) != svs.spawncount ) {
		Com_DPrintf ("Nextserver() from wrong level, from %s\n", sv_client->name);
		return;		// leftover from last server
	}

	Com_DPrintf ("Nextserver() from %s\n", sv_client->name);

	SV_Nextserver ();
}

typedef struct
{
	char	*name;
	void	(*func) (void);
} ucmd_t;

ucmd_t ucmds[] =
{
	// auto issued
	{"new", SV_New_f},
	{"configstrings", SV_Configstrings_f},
	{"baselines", SV_Baselines_f},
	{"begin", SV_Begin_f},

	{"nextserver", SV_Nextserver_f},

	{"disconnect", SV_Disconnect_f},

	// issued by hand at client consoles	
	{"info", SV_ShowServerinfo_f},

	{"download", SV_BeginDownload_f},
#if KINGPIN
	{"download5", SV_BeginDownload_f},
	{"nextdl2", SV_NextPushDownload_f},
#else
	{"nextdl", SV_NextDownload_f},
#endif

#if !KINGPIN //hypov8 todo: SV_PutAway_f
	{"putaway", SV_PutAway_f},
#endif

	{NULL, NULL}
};

/*
==================
SV_ExecuteUserCommand
==================
*/
void SV_ExecuteUserCommand (char *s)
{
	ucmd_t	*u;
	
	Cmd_TokenizeString (s, false); //Knightmare- password security fix, was true
									// prevents players from reading rcon_password
	sv_player = sv_client->edict;

//	SV_BeginRedirect (RD_CLIENT);

	for (u=ucmds ; u->name ; u++)
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			u->func ();
			break;
		}

	// r1ch: do we really want to be passing commands from unconnected players
	// to the game dll at this point? doesn't sound like a good idea to me
	// especially if the game dll does its own banning functions after connect
	// as banned players could spam game commands (eg say) whilst connecting
	if (sv_client->state < cs_spawned)
		return;

	if (!u->name && sv.state == ss_game)
		ge->ClientCommand (sv_player);

//	SV_EndRedirect ();
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/



void SV_ClientThink (client_t *cl, usercmd_t *cmd)

{
	cl->commandMsec -= cmd->msec;

	if (cl->commandMsec < 0 && sv_enforcetime->value )
	{
		Com_DPrintf ("commandMsec underflow from %s\n", cl->name);
		return;
	}

	ge->ClientThink (cl->edict, cmd);
}



#define	MAX_STRINGCMDS	8
/*
===================
SV_ExecuteClientMessage

The current net_message is parsed for the given client
===================
*/
void SV_ExecuteClientMessage (client_t *cl)
{
	int		c;
	char	*s;

	usercmd_t	nullcmd;
	usercmd_t	oldest, oldcmd, newcmd;
	int		net_drop;
	int		stringCmdCount;
	int		checksum, calculatedChecksum;
	int		checksumIndex;
	qboolean	move_issued;
	int		lastframe;

	sv_client = cl;
	sv_player = sv_client->edict;

	// only allow one move command
	move_issued = false;
	stringCmdCount = 0;

	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Printf ("SV_ReadClientMessage: badread\n");
			SV_DropClient (cl);
			return;
		}	

		c = MSG_ReadByte (&net_message);
		if (c == -1)
			break;
				
		switch (c)
		{
		default:
			Com_Printf ("SV_ReadClientMessage: unknown command char\n");
			SV_DropClient (cl);
			return;
						
		case clc_nop:
			break;

		case clc_userinfo:
			strncpy (cl->userinfo, MSG_ReadString (&net_message), sizeof(cl->userinfo)-1);
			SV_UserinfoChanged (cl);
			break;

		case clc_move:
			if (move_issued)
				return;		// someone is trying to cheat...

			move_issued = true;
			checksumIndex = net_message.readcount;
			checksum = MSG_ReadByte (&net_message);
			lastframe = MSG_ReadLong (&net_message);
			if (lastframe != cl->lastframe) 
			{
				cl->lastframe = lastframe;
				if (cl->lastframe > 0) 
				{
					cl->frame_latency[cl->lastframe&(LATENCY_COUNTS-1)] = 
						svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].senttime;
				}
			}

			memset (&nullcmd, 0, sizeof(nullcmd));
			MSG_ReadDeltaUsercmd (&net_message, &nullcmd, &oldest);
			MSG_ReadDeltaUsercmd (&net_message, &oldest, &oldcmd);
			MSG_ReadDeltaUsercmd (&net_message, &oldcmd, &newcmd);

			if ( cl->state != cs_spawned )
			{
				cl->lastframe = -1;
				break;
			}

			// if the checksum fails, ignore the rest of the packet
#if KINGPIN
			calculatedChecksum = COM_BlockSequenceCheckByte (
				net_message_buffer + checksumIndex + 1,
				net_message.readcount - checksumIndex - 1,
				cl->netchan.incoming_sequence,
				cl->challenge);

			if (calculatedChecksum != checksum)
			{
				Com_DPrintf ("Failed command checksum for %s (%d != %d)/%d\n", 
					cl->name, calculatedChecksum, checksum, 
					cl->netchan.incoming_sequence);
				return;
			}
#else
			calculatedChecksum = COM_BlockSequenceCRCByte (
				net_message.data + checksumIndex + 1,
				net_message.readcount - checksumIndex - 1,
				cl->netchan.incoming_sequence);

			if (calculatedChecksum != checksum)
			{
				Com_DPrintf ("Failed command checksum for %s (%d != %d)/%d\n", 
					cl->name, calculatedChecksum, checksum, 
					cl->netchan.incoming_sequence);
				return;
			}
#endif

			if (!sv_paused->value)
			{
				net_drop = cl->netchan.dropped;
				if (net_drop < 20)
				{

//if (net_drop > 2)

//	Com_Printf ("drop %i\n", net_drop);
					while (net_drop > 2)
					{
						SV_ClientThink (cl, &cl->lastcmd);

						net_drop--;
					}
					if (net_drop > 1)
						SV_ClientThink (cl, &oldest);

					if (net_drop > 0)
						SV_ClientThink (cl, &oldcmd);

				}
				SV_ClientThink (cl, &newcmd);
			}

			cl->lastcmd = newcmd;
			break;

		case clc_stringcmd:	
			s = MSG_ReadString (&net_message);

			// malicious users may try using too many string commands
			if (++stringCmdCount < MAX_STRINGCMDS)
				SV_ExecuteUserCommand (s);

			if (cl->state == cs_zombie)
				return;	// disconnect command
			break;
		}
	}
}

