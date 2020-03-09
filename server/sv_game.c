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
// sv_game.c -- interface to the game dll

#include "server.h"

game_export_t	*ge;


#if KINGPIN
#define	TAG_LEVEL	766		// clear when loading a new level
#define	MAX_OBJECT_BOUNDS	2048

typedef struct
{
	int magic;
	int version;
	int skinWidth;
	int skinHeight;
	int frameSize;
	int numSkins;
	int numVertices;
	int numTriangles;
	int numGlCommands;
	int numFrames;
	int numSfxDefines;
	int numSfxEntries;
	int numSubObjects;
	int offsetSkins;
	int offsetTriangles;
	int offsetFrames;
	int offsetGlCommands;
	int offsetVertexInfo;
	int offsetSfxDefines;
	int offsetSfxEntries;
	int offsetBBoxFrames;
	int offsetDummyEnd;
	int offsetEnd;
} mdx_head_t;

static int objectc = 0;
static char *objectbounds_filename[MAX_MODELS];
static int object_bounds[MAX_MODELS][MAX_MODELPART_OBJECTS];

void EXPORT MDX_ClearObjectBoundsCached(void)
{
	objectc = 0;
}

void EXPORT MDX_GetObjectBounds(const char *mdx_filename, model_part_t *model_part)
{
	int i, j;
	for (i=0; i<objectc; i++)
	{
		if (!Q_stricmp(objectbounds_filename[i], mdx_filename))
			goto ok;
	}
	memset(model_part->object_bounds, 0, sizeof(model_part->object_bounds));
	if (objectc == MAX_MODELS)
	{
		Com_Printf (S_COLOR_RED"ERROR: MDX_GetObjectBounds: Reached MAX_MODELS limit, unable to load object-bounds\n");
		return;
	}
	else
	{
		void *data;
		int size = FS_LoadFile(mdx_filename, &data);
		if (!data)
		{
			Com_Printf (S_COLOR_YELLOW"WARNING: MDX_GetObjectBounds: Unable to find MDX file \"%s\", using simple collision detection\n", mdx_filename);
			return;
		}
		else
		{
			int *NumObjectBounds = ge->GetNumObjectBounds();
			void **ObjectBoundsPointer = ge->GetObjectBoundsPointer();
			mdx_head_t head;
			for (j=0; j<sizeof(head)/4; j++)
				((uint32*)&head)[j] = LittleLong(((int32*)data)[j]);
			if (head.magic != 0x58504449 || head.numSubObjects > MAX_MODELPART_OBJECTS || head.offsetBBoxFrames + head.numSubObjects * head.numFrames * 24 > size)
			{
				FS_FreeFile(data);
				Com_Printf (S_COLOR_RED"ERROR: MDX_GetObjectBounds: Invalid MDX file \"%s\"\n", mdx_filename);
				return;
			}
			if (*NumObjectBounds + head.numSubObjects >= MAX_OBJECT_BOUNDS)
			{
				FS_FreeFile(data);
				Com_Printf (S_COLOR_RED"ERROR: MDX_GetObjectBounds: Out of ObjectBounds items\n");
				return;
			}
			objectbounds_filename[i] = strcpy(Z_TagMalloc(strlen(mdx_filename)+1, TAG_LEVEL), mdx_filename);
			memset(object_bounds[i], 0, sizeof(object_bounds[i]));
			{
				void *d = Z_TagMalloc(head.numSubObjects*head.numFrames*24, TAG_LEVEL);
				memcpy(d, ((char*)data)+head.offsetBBoxFrames, head.numSubObjects*head.numFrames*24);
				for (j=0; j<head.numSubObjects; j++)
				{
					ObjectBoundsPointer[*NumObjectBounds] = ((char*)d)+j*head.numFrames*24;
					object_bounds[i][j] = ++(*NumObjectBounds);
				}
			}
			FS_FreeFile(data);
		}
	}
	objectc++;

ok:
	memcpy(model_part->object_bounds, object_bounds[i], sizeof(model_part->object_bounds));
	model_part->objectbounds_filename = objectbounds_filename[i];
}

void EXPORT VID_StopRender(void)
{
}

void EXPORT SV_SaveNewLevel(void);
#endif



/*
===============
PF_Unicast

Sends the contents of the mutlicast buffer to a single client
===============
*/
void PF_Unicast (edict_t *ent, qboolean reliable)
{
	int		p;
	client_t	*client;

	if (!ent)
		return;

	p = NUM_FOR_EDICT(ent);
	if (p < 1 || p > (int)maxclients->value)
		return;

	client = svs.clients + (p-1);

	if (reliable)
		SZ_Write (&client->netchan.message, sv.multicast.data, sv.multicast.cursize);
	else
		SZ_Write (&client->datagram, sv.multicast.data, sv.multicast.cursize);

	SZ_Clear (&sv.multicast);
}


/*
===============
PF_dprintf

Debug print to server console
===============
*/
void PF_dprintf (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr, fmt);
//	vsprintf (msg, fmt, argptr);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Com_Printf ("%s", msg);
}


/*
===============
PF_cprintf

Print to a single client
===============
*/
void PF_cprintf (edict_t *ent, int level, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;

	if (ent)
	{
		n = NUM_FOR_EDICT(ent);
		if (n < 1 || n > (int)maxclients->value)
			Com_Error (ERR_DROP, "cprintf to a non-client");
	}

	va_start (argptr, fmt);
//	vsprintf (msg, fmt, argptr);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	if (ent)
		SV_ClientPrintf (svs.clients+(n-1), level, "%s", msg);
	else
		Com_Printf ("%s", msg);
}


/*
===============
PF_centerprintf

centerprint to a single client
===============
*/
void PF_centerprintf (edict_t *ent, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;
	
	n = NUM_FOR_EDICT(ent);
	if (n < 1 || n > (int)maxclients->value)
		return;	// Com_Error (ERR_DROP, "centerprintf to a non-client");

	va_start (argptr, fmt);
//	vsprintf (msg, fmt, argptr);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&sv.multicast,svc_centerprint);
	MSG_WriteString (&sv.multicast,msg);
	PF_Unicast (ent, true);
}


/*
===============
PF_error

Abort the server with a game error
===============
*/
void PF_error (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr, fmt);
//	vsprintf (msg, fmt, argptr);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Com_Error (ERR_DROP, "Game Error: %s", msg);
}


/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
void PF_setmodel (edict_t *ent, char *name)
{
	int		i;
	cmodel_t	*mod;

	if (!name)
		Com_Error (ERR_DROP, "PF_setmodel: NULL");

	i = SV_ModelIndex (name);
		
//	ent->model = name;
	ent->s.modelindex = i;

// if it is an inline model, get the size information for it
	if (name[0] == '*')
	{
		mod = CM_InlineModel (name);
		VectorCopy (mod->mins, ent->mins);
		VectorCopy (mod->maxs, ent->maxs);
		SV_LinkEdict (ent);
	}

}

/*
===============
PF_Configstring

===============
*/
void EXPORT PF_Configstring (int index, char *val)
{
	size_t	len, maxlen;
	char	*dest;

	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "PF_Configstring: bad index %i\n", index);

	if (!val)
		val = "";

	// catch overflow of indvidual configstrings
	len = strlen(val);
#if 0// KINGPIN //hypov8 add:
	maxlen = CS_SIZE(index);
	if (len >= maxlen) {
		Com_Printf(S_COLOR_YELLOW"PF_Configstring: index %d overflowed: %d > %d\n", index, len, maxlen);
		len = maxlen - 1;
	}
#endif

	// change the string in sv
	// Don't use a null-terminated strncpy here!!
//	strncpy (sv.configstrings[index], val);
	dest = sv.configstrings[index];
	memcpy(dest, val, len);
	dest[len] = 0;
	
	if (sv.state != ss_loading)
	{	// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_WriteChar (&sv.multicast, svc_configstring);
		MSG_WriteShort (&sv.multicast, index);
		MSG_WriteString (&sv.multicast, val);

		SV_Multicast (vec3_origin, MULTICAST_ALL_R);
	}
}



void PF_WriteChar (int c) {MSG_WriteChar (&sv.multicast, c);}
void PF_WriteByte (int c) {MSG_WriteByte (&sv.multicast, c);}
void PF_WriteShort (int c) {MSG_WriteShort (&sv.multicast, c);}
void PF_WriteLong (int c) {MSG_WriteLong (&sv.multicast, c);}
void PF_WriteFloat (float f) {MSG_WriteFloat (&sv.multicast, f);}
void PF_WriteString (char *s) {MSG_WriteString (&sv.multicast, s);}
void PF_WritePos (vec3_t pos) {MSG_WritePos (&sv.multicast, pos);}
void PF_WriteDir (vec3_t dir) {MSG_WriteDir (&sv.multicast, dir);}
void PF_WriteAngle (float f) {MSG_WriteAngle (&sv.multicast, f);}


/*
=================
PF_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean PF_inPVS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;
	if (!CM_AreasConnected (area1, area2))
		return false;		// a door blocks sight
	return true;
}


/*
=================
PF_inPHS

Also checks portalareas so that doors block sound
=================
*/
qboolean PF_inPHS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPHS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;		// more than one bounce away
	if (!CM_AreasConnected (area1, area2))
		return false;		// a door blocks hearing

	return true;
}

void PF_StartSound (edict_t *entity, int channel, int sound_num, float volume,
    float attenuation, float timeofs)
{
	if (!entity)
		return;
	SV_StartSound (NULL, entity, channel, sound_num, volume, attenuation, timeofs);
}

//==============================================

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_ShutdownGameProgs (void)
{
	if (!ge)
		return;
	ge->Shutdown ();
	Sys_UnloadGame ();
	ge = NULL;
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SCR_DebugGraph (float value, int color);

void SV_InitGameProgs (void)
{
	game_import_t	import;

	// unload anything we have now
	if (ge)
		SV_ShutdownGameProgs ();

	// load a new game dll
	import.multicast = SV_Multicast;
	import.unicast = PF_Unicast;
	import.bprintf = SV_BroadcastPrintf;
	import.dprintf = PF_dprintf;
	import.cprintf = PF_cprintf;
	import.centerprintf = PF_centerprintf;
	import.error = PF_error;

	import.linkentity = SV_LinkEdict;
	import.unlinkentity = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.trace = SV_Trace;
	import.pointcontents = SV_PointContents;
	import.setmodel = PF_setmodel;
	import.inPVS = PF_inPVS;
	import.inPHS = PF_inPHS;
	import.Pmove = Pmove;

	import.modelindex = SV_ModelIndex;
	import.soundindex = SV_SoundIndex;
	import.imageindex = SV_ImageIndex;

	import.configstring = PF_Configstring;
	import.sound = PF_StartSound;
	import.positioned_sound = SV_StartSound;

	import.WriteChar = PF_WriteChar;
	import.WriteByte = PF_WriteByte;
	import.WriteShort = PF_WriteShort;
	import.WriteLong = PF_WriteLong;
	import.WriteFloat = PF_WriteFloat;
	import.WriteString = PF_WriteString;
	import.WritePosition = PF_WritePos;
	import.WriteDir = PF_WriteDir;
	import.WriteAngle = PF_WriteAngle;

	import.TagMalloc = Z_TagMalloc;
	import.TagFree = Z_Free;
	import.FreeTags = Z_FreeTags;

	import.cvar = Cvar_Get;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;

	import.argc = Cmd_Argc;
	import.argv = Cmd_Argv;
	import.args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

	import.DebugGraph = SCR_DebugGraph;

	// Knightmare- support game DLL loading from pak files thru engine
	// This can be used to load script files, etc
	import.ListPak = FS_ListPak;
	import.LoadFile = FS_LoadFile;
	import.FreeFile = FS_FreeFile;
	import.FreeFileList = FS_FreeFileList;

	import.SetAreaPortalState = CM_SetAreaPortalState;
	import.AreasConnected = CM_AreasConnected;

#if KINGPIN
	import.skinindex = SV_SkinIndex;
	import.ClearObjectBoundsCached = MDX_ClearObjectBoundsCached;
	//import.StopRender = VID_StopRender;
	import.GetObjectBounds = MDX_GetObjectBounds;
	//import.SaveCurrentGame = SV_SaveNewLevel;
#endif

	ge = (game_export_t *)Sys_GetGameAPI (&import);

	if (!ge)
		Com_Error (ERR_DROP, "failed to load game DLL");
	if (ge->apiversion != GAME_API_VERSION)
		Com_Error (ERR_DROP, "game is version %i, not %i", ge->apiversion,
		GAME_API_VERSION);
#if KINGPIN
		//r1: verify we got essential exports
	if (!ge->ClientCommand || !ge->ClientBegin || !ge->ClientConnect || !ge->ClientDisconnect || !ge->ClientUserinfoChanged ||
		!ge->ReadGame || !ge->ReadLevel || !ge->RunFrame || !ge->ServerCommand || !ge->Shutdown || !ge->SpawnEntities ||
		!ge->WriteGame || !ge->WriteLevel || !ge->Init)
		Com_Error (ERR_DROP, "Game is missing required exports");
#endif
	ge->Init ();

#if !KINGPIN
		if (!ge->edicts)
		Com_Error (ERR_DROP, "Game failed to initialize globals.edicts");


	//r1: moved from SV_InitGame to here.
	for (i=0 ; i<maxclients->intvalue ; i++)
	{
		ent = EDICT_NUM(i+1);
		ent->s.number = i+1;
		svs.clients[i].edict = ent;
		memset (&svs.clients[i].lastcmd, 0, sizeof(svs.clients[i].lastcmd));
	}
#endif
}

