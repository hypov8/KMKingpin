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

#include "server.h"

server_static_t	svs;				// persistant server info
server_t		sv;					// local server

/*
================
SV_FindIndex

================
*/
int SV_FindIndex (char *name, int start, int max, qboolean create)
{
	int		i;
	
	if (!name || !name[0])
		return 0;

	for (i = 1; i < max && sv.configstrings[start+i][0]; i++)
		if (!strcmp(sv.configstrings[start+i], name))
			return i;

	if (!create)
		return 0;

	// Knightmare 12/23/2001
	// Output a more useful error message to tell user what overflowed
	// And don't bomb out, either- instead, return last possible index
	if (i == max)
	//	Com_Error (ERR_DROP, "*Index: overflow");
	{
		if (start == CS_MODELS)
			Com_Printf (S_COLOR_YELLOW"Warning: Index overflow for models\n");
		else if (start == CS_SOUNDS)
			Com_Printf (S_COLOR_YELLOW"Warning: Index overflow for sounds\n");
		else if (start == CS_IMAGES)
			Com_Printf (S_COLOR_YELLOW"Warning: Index overflow for images\n");
		return (max-1);	// return the last possible index
	}
	// end Knightmare

	strncpy (sv.configstrings[start+i], name, sizeof(sv.configstrings[i]));

	if (sv.state != ss_loading)
	{	// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_WriteChar (&sv.multicast, svc_configstring);
		MSG_WriteShort (&sv.multicast, start+i);
		MSG_WriteString (&sv.multicast, name);
		SV_Multicast (vec3_origin, MULTICAST_ALL_R);
	}

	return i;
}


#if KINGPIN
/*
	MH: The Kingpin client can download models/skins/maps/textures but its support for
	sounds/images/skies is broken. The broken image download support can be taken
	advantage of though to allow those things to be downloaded, which is what the
	code below does. The "imageindex" function can be used by the game to add any
	files to the downloadable list (only files starting with "/pics/" will be added
	to the image list).
	
	When a client connects,	the server sends the downloadable files list in place
	of the image list. The image list is then sent when the client has finished
	connecting.
*/

static void DownloadIndex(const char *name)
{
	// some common files that the player should already have
	static const char *ignore_sound[] = {
		"actors/player/male/fry.wav",
		"actors/player/male/gasp1.wav",
		"actors/player/male/gasp2.wav",
		"misc/w_pkup.wav",
		"weapons/bullethit_tin.wav",
		"weapons/bullethit_tin2.wav",
		"weapons/bullethit_tin3.wav",
		"weapons/machinegun/machgf1b.wav",
		"weapons/melee/swing.wav",
		"weapons/pistol/silencer.wav",
		"weapons/pistol/silencerattatch.wav",
		"weapons/ric1.wav",
		"weapons/ric2.wav",
		"weapons/ric3.wav",
		"weapons/shotgun/shotgf1b.wav",
		"weapons/shotgun/shotgr1b.wav",
		"world/amb_wind.wav",
		"world/city.wav",
		"world/citybg.wav",
		"world/citybglow.wav",
		"world/citybglow2.wav",
		"world/crate1.wav",
		"world/crate2.wav",
		"world/crate3.wav",
		"world/doors/dr1_end.wav",
		"world/doors/dr1_mid.wav",
		"world/doors/dr1_strt.wav",
		"world/doors/dr2_endb.wav",
		"world/doors/dr2_mid.wav",
		"world/doors/dr2_strt.wav",
		"world/doors/dr3_endb.wav",
		"world/doors/dr3_mid.wav",
		"world/doors/dr3_strt.wav",
		"world/fire.wav",
		"world/fire_sm.wav",
		"world/lightf_broke.wav",
		"world/lightf_broker.wav",
		"world/lightf_hum.wav",
		"world/lightf_hum2.wav",
		"world/pickups/ammo.wav",
		"world/pickups/generic.wav",
		"world/pickups/health.wav",
		"world/plats/pt1_end.wav",
		"world/plats/pt1_mid.wav",
		"world/plats/pt1_strt.wav",
		"world/switches/butn2.wav",
		"world/trash1.wav",
		"world/trash2.wav",
		"world/trash3.wav",
		NULL
	};

	char lwrname[MAX_QPATH], *ext;
	int i;

	// only add to the downloadable files list during the map loading phase
	if (!(int)allow_download->value || sv.state != ss_loading)
		return;
	if (!name || !name[0] || strlen(name) >= sizeof(lwrname))
		return;
	strcpy(lwrname, name);
	Q_strlwr(lwrname);

	ext = strrchr(lwrname, '.');
	if (ext && (!strcmp(ext+1, "dll") || !strcmp(ext+1, "exe")))
	{
		Com_Printf (S_COLOR_YELLOW"WARNING: '%s' will not be downloadable because it is an executable.\n", lwrname);
		return;
	}

	if (!strchr(lwrname, '/') && !Cvar_VariableString("clientdir")[0])
	{
		Com_Printf (S_COLOR_YELLOW"WARNING: '%s' will not be downloadable because it is not in a subdirectory.\n", lwrname);
		return;
	}

	if (!strncmp(lwrname, "pics/", 5))
	{
		if (ext && !strcmp(ext, ".tga") && !strncmp(lwrname+5, "h_", 2))
			return;
	}
	else if (!strncmp(lwrname, "sound/", 6))
	{
		for (i=0; ignore_sound[i]; i++)
			if (!strcmp(lwrname+6, ignore_sound[i]))
				return;
	}

	// don't add files that are in a PAK
	if (FS_LoadFile(lwrname, NULL) > 0 /*&& lastpakfile*/)
		return;

	for (i=0; i<MAX_IMAGES && sv.dlconfigstrings[i][0]; i++)
		if (!strcmp(sv.dlconfigstrings[i], lwrname))
			return;
	if (i == MAX_IMAGES)
	{
		Com_Printf (S_COLOR_YELLOW"WARNING: Ran out of download configstrings while attempting to add '%s'\n", lwrname);
		return;
	}

	strcpy(sv.dlconfigstrings[i], lwrname);
}

int EXPORT SV_ModelIndex (const char *name)
{
	char lwrname[MAX_QPATH];
	int i;

	if (!name || !name[0] || strlen(name) >= sizeof(sv.configstrings[0]))
		return 0;
	strcpy(lwrname, name);
	Q_strlwr(lwrname);

	i = SV_FindIndex (lwrname, CS_MODELS, MAX_MODELS, true);
	if (i && !strncmp(lwrname, "models/objects/", 15))
	{
		// Kingpin doesn't auto-download model skins, so add them to downloadable files list
		void *data;
		FS_LoadFile(lwrname, &data);
		if (data)
		{
			header_mdx_t *pheader = (header_mdx_t*)data;
			if (LittleLong(pheader->ident) == ALIAS_MDX_HEADER && LittleLong(pheader->version) == ALIAS_MDX_VERSION)
			{
				int a, ns = LittleLong(pheader->num_skins);
				for (a=0; a<ns; a++)
					DownloadIndex((char*)data + LittleLong(pheader->ofs_skins) + a * MAX_MDX_SKINNAME);
			}
			FS_FreeFile(data);
		}
	}
	return i;
}

int EXPORT SV_SoundIndex (const char *name)
{
	char lwrname[MAX_QPATH];
	int i;

	if (!name || !name[0] || strlen(name) >= sizeof(lwrname))
		return 0;
	strcpy(lwrname, name);
	Q_strlwr(lwrname);

	i = SV_FindIndex (lwrname, CS_SOUNDS, MAX_SOUNDS, true);
	if (i && lwrname[0] != '*')
	{
		char buf[MAX_QPATH + 1];
		Com_sprintf(buf, sizeof(buf), "sound/%s", lwrname);
		DownloadIndex(buf);
	}
	return i;
}

int EXPORT SV_ImageIndex (const char *name)
{
	char lwrname[MAX_QPATH];
	int i;

	if (!name || !name[0] || !strchr(name+1,'/') || strlen(name) >= sizeof(lwrname))
		return 0;
	strcpy(lwrname, name);
	Q_strlwr(lwrname);

	// Kingpin always begins image files with "/pics/"
	if (!strncmp(lwrname, "/pics/", 6))
		i = SV_FindIndex (lwrname, CS_IMAGES, MAX_IMAGES, true);
	else
		i = 0;
	DownloadIndex(lwrname[0] == '/' ? lwrname+1 : lwrname);
	return i;
}

int	EXPORT SV_SkinIndex(int modelindex, const char *name)
{
	char lwrname[MAX_QPATH];
	int i, len, len2;

	len = strlen(name);
	if ((uint32)modelindex > MAX_MODELS || len >= sizeof(lwrname))
		return 0;
	strcpy(lwrname, name);
	Q_strlwr(lwrname);

	len2 = strlen(sv.configstrings[CS_MODELSKINS + modelindex]);
	for (i=0; i<len2; i++)
		if (!strncmp(sv.configstrings[CS_MODELSKINS + modelindex] + i, lwrname, len))
			return i;

	strcpy(sv.configstrings[CS_MODELSKINS + modelindex] + i, lwrname);

	if (sv.state != ss_loading)
	{
		SZ_Clear (&sv.multicast); //hypov8 todo: ok?
		//MSG_BeginWriting (svc_configstring);
		MSG_WriteChar (&sv.multicast, svc_configstring); //hypov8 todo: ok?
		MSG_WriteShort (&sv.multicast, CS_MODELSKINS + modelindex);
		MSG_WriteString (&sv.multicast, sv.configstrings[CS_MODELSKINS + modelindex]);
		MSG_WriteByte (&sv.multicast, len);
		SV_Multicast (NULL, MULTICAST_ALL_R);
	}
	return i;
}
#else

int SV_ModelIndex (char *name)
{
	return SV_FindIndex (name, CS_MODELS, MAX_MODELS, true);
}

int SV_SoundIndex (char *name)
{
	return SV_FindIndex (name, CS_SOUNDS, MAX_SOUNDS, true);
}

int SV_ImageIndex (char *name)
{
	return SV_FindIndex (name, CS_IMAGES, MAX_IMAGES, true);
}
#endif

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline (void)
{
	edict_t			*svent;
	int				entnum;	

	for (entnum = 1; entnum < ge->num_edicts ; entnum++)
	{
		svent = EDICT_NUM(entnum);
		if (!svent->inuse)
			continue;
		if (!svent->s.modelindex && !svent->s.sound && !svent->s.effects)
			continue;
		svent->s.number = entnum;

		//
		// take current state as baseline
		//
		VectorCopy (svent->s.origin, svent->s.old_origin);
		sv.baselines[entnum] = svent->s;
	}
}


/*
=================
SV_CheckForSavegame
=================
*/
void SV_CheckForSavegame (void)
{
	char		name[MAX_OSPATH];
	FILE		*f;
	int			i;

	if (sv_noreload->value)
		return;

	if (Cvar_VariableValue ("deathmatch"))
		return;

	Com_sprintf (name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	f = fopen (name, "rb");
	if (!f)
		return;		// no savegame

	fclose (f);

	SV_ClearWorld ();

	// get configstrings and areaportals
	SV_ReadLevelFile ();

	if (!sv.loadgame)
	{	// coming back to a level after being in a different
		// level, so run it for ten seconds

		// rlava2 was sending too many lightstyles, and overflowing the
		// reliable data. temporarily changing the server state to loading
		// prevents these from being passed down.
		server_state_t		previousState;		// PGM

		previousState = sv.state;				// PGM
		sv.state = ss_loading;					// PGM
		for (i=0 ; i<100 ; i++)
			ge->RunFrame ();

		sv.state = previousState;				// PGM
	}
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

================
*/
void SV_SpawnServer (char *server, char *spawnpoint, server_state_t serverstate, qboolean attractloop, qboolean loadgame)
{
	int			i;
	unsigned	checksum;
	fileHandle_t	f;

	if (attractloop)
		Cvar_Set ("paused", "0");

	Com_Printf ("------- Server Initialization -------\n");

	Com_DPrintf ("SpawnServer: %s\n",server);
	if (sv.demofile)
		FS_FCloseFile (sv.demofile);

	svs.spawncount++;		// any partially connected client will be
							// restarted
	sv.state = ss_dead;
	Com_SetServerState (sv.state);

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof(sv));
	svs.realtime = 0;
	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	// save name for levels that don't set message
//	strncpy (sv.configstrings[CS_NAME], server);
	Q_strncpyz (sv.configstrings[CS_NAME], server, sizeof(sv.configstrings[CS_NAME]));
#if KINGPIN
	strcpy (sv.configstrings[CS_SERVER_VERSION], "200"); // 2.00
#else
	if (Cvar_VariableValue ("deathmatch"))
	{
		Com_sprintf(sv.configstrings[CS_AIRACCEL], sizeof(sv.configstrings[CS_AIRACCEL]), "%g", sv_airaccelerate->value);
		pm_airaccelerate = sv_airaccelerate->value;
	}
	else
	{
	//	strncpy(sv.configstrings[CS_AIRACCEL], "0");
		Q_strncpyz(sv.configstrings[CS_AIRACCEL], "0", sizeof(sv.configstrings[CS_AIRACCEL]));
		pm_airaccelerate = 0;
	}
#endif

	SZ_Init (&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));

//	strncpy (sv.name, server);
	Q_strncpyz (sv.name, server, sizeof(sv.name));

	// leave slots at start for clients only
	for (i=0 ; i<(int)maxclients->value ; i++)
	{
		// needs to reconnect
		if (svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;
		svs.clients[i].lastframe = -1;
	}

	sv.time = 1000;
	
//	strncpy (sv.name, server);
//	strncpy (sv.configstrings[CS_NAME], server);
	Q_strncpyz (sv.name, server, sizeof(sv.name));
	Q_strncpyz (sv.configstrings[CS_NAME], server, sizeof(sv.configstrings[CS_NAME]));

	if (serverstate != ss_game)
	{
		sv.models[1] = CM_LoadMap ("", false, &checksum);	// no real map
	}
	else
	{
		Com_sprintf (sv.configstrings[CS_MODELS+1],sizeof(sv.configstrings[CS_MODELS+1]),
			"maps/%s.bsp", server);
	
		// resolve CS_PAKFILE, hack by Jay Dolan
		FS_FOpenFile(sv.configstrings[CS_MODELS + 1], &f, FS_READ);
	//	strncpy(sv.configstrings[CS_PAKFILE], (last_pk3_name ? last_pk3_name : ""));
		Q_strncpyz(sv.configstrings[CS_PAKFILE], (last_pk3_name ? last_pk3_name : ""), sizeof(sv.configstrings[CS_PAKFILE]));
		FS_FCloseFile(f);
	
		sv.models[1] = CM_LoadMap (sv.configstrings[CS_MODELS+1], false, &checksum);
	}
	Com_sprintf (sv.configstrings[CS_MAPCHECKSUM],sizeof(sv.configstrings[CS_MAPCHECKSUM]),
		"%i", checksum);

	//
	// clear physics interaction links
	//
	SV_ClearWorld ();
	
	for (i=1 ; i< CM_NumInlineModels() ; i++)
	{
		Com_sprintf (sv.configstrings[CS_MODELS+1+i], sizeof(sv.configstrings[CS_MODELS+1+i]),
			"*%i", i);
		sv.models[i+1] = CM_InlineModel (sv.configstrings[CS_MODELS+1+i]);
	}

	//
	// spawn the rest of the entities on the map
	//	

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	Com_SetServerState (sv.state);

	// load and spawn all other entities
	ge->SpawnEntities ( sv.name, CM_EntityString(), spawnpoint );

	// run two frames to allow everything to settle
	ge->RunFrame ();
	ge->RunFrame ();

	// all precaches are complete
	sv.state = serverstate;
	Com_SetServerState (sv.state);
	
	// create a baseline for more efficient communications
	SV_CreateBaseline ();

	// check for a savegame
	SV_CheckForSavegame ();

	// set serverinfo variable
	Cvar_FullSet ("mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	Com_Printf ("-------------------------------------\n");
}

/*
==============
SV_InitGame

A brand new game has been started
==============
*/
void PF_Configstring (int index, char *val);

void SV_InitGame (void)
{
	int		i;
	edict_t	*ent;
	char	idmaster[32];

	if (svs.initialized)
	{
		// cause any connected clients to reconnect
		SV_Shutdown ("Server restarted\n", true);
	}
	else
	{
		// make sure the client is down
		CL_Drop ();
		SCR_BeginLoadingPlaque ();
	}

	// get any latched variable changes (maxclients, etc)
	Cvar_GetLatchedVars ();

	svs.initialized = true;

	if (Cvar_VariableValue ("coop") && Cvar_VariableValue ("deathmatch"))
	{
		Com_Printf("Deathmatch and Coop both set, disabling Coop\n");
		Cvar_FullSet ("coop", "0",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// dedicated servers can't be single player and are usually DM
	// so unless they explicity set coop, force it to deathmatch
	if (dedicated->value)
	{
		if (!Cvar_VariableValue ("coop"))
			Cvar_FullSet ("deathmatch", "1",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// init clients
	if (Cvar_VariableValue ("deathmatch"))
	{
		if ((int)maxclients->value <= 1)
			Cvar_FullSet ("maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
		else if ((int)maxclients->value > MAX_CLIENTS)
			Cvar_FullSet ("maxclients", va("%i", MAX_CLIENTS), CVAR_SERVERINFO | CVAR_LATCH);
	}
	else if (Cvar_VariableValue ("coop"))
	{
		if ((int)maxclients->value <= 1 || (int)maxclients->value > 4)
			Cvar_FullSet ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	}
	else	// non-deathmatch, non-coop is one player
	{
		Cvar_FullSet ("maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH);
	}

	svs.spawncount = rand();
	svs.clients = Z_Malloc (sizeof(client_t)*(int)maxclients->value);
	svs.num_client_entities = (int)maxclients->value*UPDATE_BACKUP*64;
	svs.client_entities = Z_Malloc (sizeof(entity_state_t)*svs.num_client_entities);

	// init network stuff
	NET_Config ( ((int)maxclients->value > 1) );

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = -99999;		// send immediately
	Com_sprintf(idmaster, sizeof(idmaster), "192.246.40.37:%i", PORT_MASTER);
	NET_StringToAdr (idmaster, &master_adr[0]);

	// init game
	SV_InitGameProgs ();
	for (i = 0; i < (int)maxclients->value; i++)
	{

		ent = EDICT_NUM(i+1);
		ent->s.number = i+1;
		svs.clients[i].edict = ent;
		memset (&svs.clients[i].lastcmd, 0, sizeof(svs.clients[i].lastcmd));
	}
}


/*
======================
SV_Map

  the full syntax is:

  map [*]<map>$<startspot>+<nextserver>

command from the console or progs.
Map can also be a.cin, .pcx, or .dm2 file
Nextserver is used to allow a cinematic to play, then proceed to
another level:

	map tram.cin+jail_e3
======================
*/
void SV_Map (qboolean attractloop, char *levelstring, qboolean loadgame)
{
	char	level[MAX_QPATH];
	char	*ch;
	int		l;
	char	spawnpoint[MAX_QPATH];

	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	if (sv.state == ss_dead && !sv.loadgame)
		SV_InitGame ();	// the game is just starting

	// r1ch fix: buffer overflow
//	strncpy (level, levelstring);
	if (levelstring[0] == '*')
		Q_strncpyz (level, levelstring+1, sizeof(level));
	else
		Q_strncpyz (level, levelstring, sizeof(level));

	// if there is a + in the map, set nextserver to the remainder
	ch = strstr(level, "+");
	if (ch)
	{
		*ch = 0;
			Cvar_Set ("nextserver", va("gamemap \"%s\"", ch+1));
	}
	else
		Cvar_Set ("nextserver", "");

	//ZOID special hack for end game screen in coop mode
	if (Cvar_VariableValue ("coop") && !Q_stricmp(level, "victory.pcx"))
		Cvar_Set ("nextserver", "gamemap \"*base1\"");

	// if there is a $, use the remainder as a spawnpoint
	ch = strstr(level, "$");
	if (ch)
	{
		*ch = 0;
	//	strncpy (spawnpoint, ch+1);
		Q_strncpyz (spawnpoint, ch+1, sizeof(spawnpoint));
	}
	else
		spawnpoint[0] = 0;

	// skip the end-of-unit flag if necessary
//	if (level[0] == '*')
//		strncpy (level, level+1);

	l = strlen(level);
#ifdef	ROQ_SUPPORT
	if (l > 4 && (!strcmp (level+l-4, ".cin") || !strcmp (level+l-4, ".roq")) )
#else
	if (l > 4 && !strcmp (level+l-4, ".cin") )
#endif // ROQ_SUPPORT
	{
		if (!dedicated->value)
			SCR_BeginLoadingPlaque ();			// for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, spawnpoint, ss_cinematic, attractloop, loadgame);
	}
	else if (l > 4 && !strcmp (level+l-4, ".dm2") )
	{
		if (!dedicated->value)
			SCR_BeginLoadingPlaque ();			// for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, spawnpoint, ss_demo, attractloop, loadgame);
	}
	else if (l > 4 && !strcmp (level+l-4, ".pcx"))
	{
		if (!dedicated->value)
			SCR_BeginLoadingPlaque ();			// for local system
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, spawnpoint, ss_pic, attractloop, loadgame);
	}
	else
	{
		if (!dedicated->value)
			SCR_BeginLoadingPlaque ();			// for local system
		SV_BroadcastCommand ("changing\n");
		SV_SendClientMessages ();
		SV_SpawnServer (level, spawnpoint, ss_game, attractloop, loadgame);
		Cbuf_CopyToDefer ();
	}

	SV_BroadcastCommand ("reconnect\n");
}
