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
// cl_parse.c  -- parse a message received from the server

#include "client.h"

char *svc_strings[256] =
{
	"svc_bad",
#if KINGPIN
	"svc_muzzleflash",
	"svc_muzzleflash2",
	"svc_muzzleflash3",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",
	"svc_hud",
	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame",
	"svc_configstring_pointer",
	"svc_pushdownload",
	"svc_cpacket", // MH: compressed packet for patched clients
	"svc_xdownload", // MH: download packet for patched clients
#else
	"svc_muzzleflash",
	"svc_muzzlflash2",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",	
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame"
#endif
};

//=============================================================================

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int		i;

	S_BeginRegistration ();
	CL_RegisterTEntSounds ();

	// Knightmare- 1/2/2002- ULTRA-CHEESY HACK for old demos or
	// connected to server using old protocol
	// Changed config strings require different offsets
	if ( LegacyProtocol() )
	{
		for (i=1; i < OLD_MAX_SOUNDS; i++)
		{
			if (!cl.configstrings[OLD_CS_SOUNDS+i][0])
				break;
			cl.sound_precache[i] = S_RegisterSound (cl.configstrings[OLD_CS_SOUNDS+i]);
			Sys_SendKeyEvents ();	// pump message loop
		}

	}
	else
	{
		for (i=1; i < MAX_SOUNDS; i++)
		{
			if (!cl.configstrings[CS_SOUNDS+i][0])
				break;
			cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
			Sys_SendKeyEvents ();	// pump message loop
		}
	}
	//end Knightmare
	S_EndRegistration ();
}

/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
LegacyProtocol
A utility function that determines
if parsing of old protocol should be used.
==================
*/
qboolean LegacyProtocol (void)
{
	//if (dedicated->value)	// Server always uses new protocol
	//	return false;
	if ( (Com_ServerState() && cls.serverProtocol <= OLD_PROTOCOL_VERSION)
		|| (cls.serverProtocol == OLD_PROTOCOL_VERSION) )
		return true;
	return false;
}


/*
==================
R1Q2Protocol
A utility function that determines
if parsing of R1Q2 protocol should be used.
==================
*/
qboolean R1Q2Protocol (void)
{
	//if (dedicated->value)	// Server always uses new protocol
	//	return false;
	if ( cls.serverProtocol == R1Q2_PROTOCOL_VERSION )
		return true;
	return false;
}


/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData (void)
{
	extern cvar_t	*fs_gamedirvar;
	char	*str;
	int		i;
	
	Com_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();
	cls.state = ca_connected;

// parse protocol version number
	i = MSG_ReadLong (&net_message);
	cls.serverProtocol = i;

	// BIG HACK to let demos from release work with the 3.0x patch!!!
	// Knightmare- also allow connectivity with servers using the old protocol
//	if (Com_ServerState() && (i < PROTOCOL_VERSION) /*== 35*/)
	if ( LegacyProtocol() ) {} // do nothing
/*	else if (i == OLD_PROTOCOL_VERSION)
		Cvar_ForceSet ("cl_servertrick", "1");
	else if (i == PROTOCOL_VERSION)
		Cvar_ForceSet ("cl_servertrick", "0");	// force this off for local games
	else if (i != PROTOCOL_VERSION)	*/
	else if ( (i != PROTOCOL_VERSION) && (i != OLD_PROTOCOL_VERSION) )
		Com_Error (ERR_DROP,"Server returned version %i, not %i", i, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong (&net_message);
	cl.attractloop = MSG_ReadByte (&net_message);

	// game directory
	str = MSG_ReadString (&net_message);
	strncpy (cl.gamedir, str, sizeof(cl.gamedir)-1);

	// set gamedir
	if ( ( (*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str)))
		|| (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)) )
		&& !cl.attractloop ) // Knightmare- don't allow demos to change this
		Cvar_Set("game", str);

	// parse player entity number
	cl.playernum = MSG_ReadShort (&net_message);

	// get the full level name
	str = MSG_ReadString (&net_message);

	if (cl.playernum == -1)
	{	// playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic (str);
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		con.ormask = 128;
		Com_Printf ("%c"S_COLOR_SHADOW S_COLOR_ALT"%s\n", 2, str);
		con.ormask = 0;

		// need to prep refresh at next oportunity
		cl.refresh_prepped = false;
	}
}

/*
==================
CL_ParseBaseline
==================
*/

void CL_ParseBaseline (void)
{
	entity_state_t	*es;
	int				bits;
	int				newnum;
	entity_state_t	nullstate;

	memset (&nullstate, 0, sizeof(nullstate));
	newnum = CL_ParseEntityBits (&bits);
	es = &cl_entities[newnum].baseline;
	CL_ParseDelta (&nullstate, es, newnum, bits);
}


#if KINGPIN //<folder>/skinfolder.txt
qboolean CL_ParseSkinFfolderTxt (char *skinFolder, char *outHead, char *outBody, char *outLegs)
{
	int				len, i;
	fileHandle_t	f;
	char			buf[MAX_QPATH*3]; //hypov8 enough??
	char			*s, *token, *dest;
	char filename[MAX_QPATH];
	qboolean h=false, b=false, l=false;

	Com_sprintf (filename, sizeof(filename), "%s/skinfolder.txt", skinFolder);

	len = FS_FOpenFile (filename, &f, FS_READ);
	if (!f) {
		Com_Printf (S_COLOR_RED "CL_ParseSkinFfolderTxt: missing skinfolder.txt in %s\n", skinFolder);
		return false;
	}


	FS_Read (buf, len, f);
	buf[len] = 0;
	FS_FCloseFile (f);

	s = buf;
	
	//check for 3 lines
	for (i = 0; i < 3; i++)
	{
		token = COM_Parse(&s);
		if (!token)	{
			Com_Printf("UI_ParseArenaFromFile: unexpected EOF\n");
			return false;
		}

		if (!token || !s) break; //EOF

		if (Q_strncasecmp(token, "head:", 5) == 0){
			Com_sprintf(outHead, MAX_QPATH, "%s", token + 5); h = true;
		}
		else if (Q_strncasecmp(token, "body:", 5) == 0){
			Com_sprintf(outBody, MAX_QPATH, "%s", token + 5); b = true;
		}
		if (Q_strncasecmp(token, "legs:", 5) == 0){
			Com_sprintf(outLegs, MAX_QPATH, "%s", token + 5); l = true;
		}
	}

	if (!h || !b||!l) {
		Com_Printf (S_COLOR_RED "CL_ParseSkinFfolderTxt: error in %s\n", filename);
		return false;
	}

	return true;
}

#endif
/*
================
CL_LoadClientinfo

================
*/
#if !KINGPIN
void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int i;
	char		*t;
	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		skin_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];

	strncpy(ci->cinfo, s, sizeof(ci->cinfo));
	ci->cinfo[sizeof(ci->cinfo)-1] = 0;

	// isolate the player's name
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name)-1] = 0;
	t = strstr (s, "\\");
	if (t)
	{
		ci->name[t-s] = 0;
		s = t+1;
	}

	if (cl_noskins->value || *s == 0)
	{
		Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
		Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/weapon.md2");
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/male/grunt.pcx");
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/male/grunt_i.pcx");
		ci->model = R_RegisterModel (model_filename);
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		ci->weaponmodel[0] = R_RegisterModel (weapon_filename);
		ci->skin = R_RegisterSkin (skin_filename);
		ci->icon = R_DrawFindPic (ci->iconname);
	}
	else
	{
		// isolate the model name
	//	strncpy (model_name, s);
		Q_strncpyz (model_name, s, sizeof(model_name));
		t = strstr(model_name, "/");
		if (!t)
			t = strstr(model_name, "\\");
		if (!t)
			t = model_name;
		*t = 0;

		// isolate the skin name
//		strncpy (skin_name, s + strlen(model_name) + 1);
		Q_strncpyz (skin_name, s + strlen(model_name) + 1, sizeof(skin_name));

		// model file
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
		ci->model = R_RegisterModel (model_filename);
		if (!ci->model)
		{
		//	strncpy(model_name, "male");
			Q_strncpyz(model_name, "male", sizeof(model_name));
			Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = R_RegisterModel (model_filename);
		}

		// skin file
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
		ci->skin = R_RegisterSkin (skin_filename);

		// if we don't have the skin and the model wasn't male,
		// see if the male has it (this is for CTF's skins)
 		if (!ci->skin && Q_stricmp(model_name, "male"))
		{
			// change model to male
		//	strncpy(model_name, "male");
			Q_strncpyz(model_name, "male", sizeof(model_name));
			Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = R_RegisterModel (model_filename);

			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
			ci->skin = R_RegisterSkin (skin_filename);
		}

		// if we still don't have a skin, it means that the male model didn't have
		// it, so default to grunt
		if (!ci->skin) {
			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/grunt.pcx", model_name, skin_name);
			ci->skin = R_RegisterSkin (skin_filename);
		}

		// weapon file
		for (i = 0; i < num_cl_weaponmodels; i++) {
			Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
			ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			if (!ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0) {
				// try male
				Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/%s", cl_weaponmodels[i]);
				ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			}
			if (!cl_vwep->value)
				break; // only one when vwep is off
		}

		// icon file
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/%s/%s_i.pcx", model_name, skin_name);
		ci->icon = R_DrawFindPic (ci->iconname);
	}

	// must have loaded all data types to be valud
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}
#else
void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int i;
	char		*t;
	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		skin_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];
	char currentskinKP[4][MAX_QPATH] = {"001", "001", "001", "0"};
	char		head_path[MAX_QPATH];
	char		body_path[MAX_QPATH];
	char		legs_path[MAX_QPATH];

	strncpy(ci->cinfo, s, sizeof(ci->cinfo));
	ci->cinfo[sizeof(ci->cinfo)-1] = 0;

	// isolate the player's name
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name)-1] = 0;
	t = strstr (s, "\\");
	if (t)
	{
		ci->name[t-s] = 0;
		s = t+1;
	}

	if (cl_noskins->value || *s == 0)
	{	
		//model
		Com_sprintf (model_filename, sizeof(model_filename), "players/male_thug/head.mdx");
		ci->model[0] = R_RegisterModel (model_filename);
		Com_sprintf (model_filename, sizeof(model_filename), "players/male_thug/body.mdx");
		ci->model[1] = R_RegisterModel (model_filename);
		Com_sprintf (model_filename, sizeof(model_filename), "players/male_thug/legs.mdx");
		ci->model[2] = R_RegisterModel (model_filename);
		//skin
		Com_sprintf (skin_filename, sizeof(skin_filename), "models/actors/thug/head_001.tga");
		ci->skin[0] = R_RegisterSkin (skin_filename);
		Com_sprintf (skin_filename, sizeof(skin_filename), "models/actors/thug/body_001.tga");
		ci->skin[1] = R_RegisterSkin (skin_filename);
		Com_sprintf (skin_filename, sizeof(skin_filename), "models/actors/thug/legs_001.tga");
		ci->skin[2] = R_RegisterSkin (skin_filename);

		Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male_thug/w_pipe.mdx"); //hypov8 todo: tomgun?
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		ci->weaponmodel[0] = R_RegisterModel (weapon_filename);
	}
	else
	{
		// isolate the model name
		// strncpy (model_name, s);
		Q_strncpyz (model_name, s, sizeof(model_name));
		t = strstr(model_name, "/");
		if (!t)
			t = strstr(model_name, "\\");
		if (!t)
			t = model_name;
		*t = 0;

		// isolate the skin name
		// strncpy (skin_name, s + strlen(model_name) + 1);
		Q_strncpyz (skin_name, s + strlen(model_name) + 1, sizeof(skin_name));


		//setup skin names from config string
		if (strchr(skin_name, ' '))
		{
			char *tmp, *token;
			int x;

			tmp = skin_name;
			for (x = 0; x < 4; x++)
			{
				token = COM_Parse(&tmp);
				if (token)
					strcpy(currentskinKP[x], token);
			}
		}

		//////////////////////////
		//check for skinfolder.txt
		if (!CL_ParseSkinFfolderTxt(va("players/%s", model_name), head_path, body_path, legs_path))	{
			Com_sprintf(head_path, sizeof(head_path),"players/%s", model_name);
			Com_sprintf(body_path, sizeof(body_path),"players/%s", model_name);
			Com_sprintf(legs_path, sizeof(legs_path),"players/%s", model_name);
		}

		/////////////
		// model file
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/head.mdx", model_name);
		ci->model[0] = R_RegisterModel (model_filename);
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/body.mdx", model_name);
		ci->model[1] = R_RegisterModel (model_filename);
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/legs.mdx", model_name);
		ci->model[2] = R_RegisterModel (model_filename);

		// skin file
		Com_sprintf (skin_filename, sizeof(skin_filename), "%s/head_%s.tga", head_path, currentskinKP[0]);//skin_name
		ci->skin[0] = R_RegisterSkin (skin_filename);
		Com_sprintf (skin_filename, sizeof(skin_filename), "%s/body_%s.tga", body_path, currentskinKP[1]);
		ci->skin[1] = R_RegisterSkin (skin_filename);
		Com_sprintf (skin_filename, sizeof(skin_filename), "%s/legs_%s.tga", legs_path, currentskinKP[2]);
		ci->skin[2] = R_RegisterSkin (skin_filename);


		// if we don't have the skin, check default
		// ctf/baggy??
 		if (!ci->skin[0] || !ci->skin[1] ||!ci->skin[2])
		{
			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "%s/head_001.tga", head_path);
			ci->skin[0] = R_RegisterSkin (skin_filename);
			Com_sprintf (skin_filename, sizeof(skin_filename), "%s/body_001.tga", body_path);
			ci->skin[1] = R_RegisterSkin (skin_filename);
			Com_sprintf (skin_filename, sizeof(skin_filename), "%s/legs_001.tga", legs_path);
			ci->skin[2] = R_RegisterSkin (skin_filename);
		}

		// if we still don't have a skin, load default male_thug
 		if ((!ci->skin[0] || !ci->skin[1] ||!ci->skin[2]) && Q_stricmp(model_name, "male_thug"))
		{
			// Default. male_thug
			Q_strncpyz(model_name, "male_thug", sizeof(model_name));

			//////////////////////////
			//check for skinfolder.txt
			if (!CL_ParseSkinFfolderTxt(va("players/%s", model_name), head_path, body_path, legs_path))	{
				Com_sprintf(head_path, sizeof(head_path),"players/%s", model_name);
				Com_sprintf(body_path, sizeof(body_path),"players/%s", model_name);
				Com_sprintf(legs_path, sizeof(legs_path),"players/%s", model_name);
			}

			//model
			Com_sprintf (model_filename, sizeof(model_filename), "players/%s/head.mdx", model_name);
			ci->model[0] = R_RegisterModel (model_filename);
			Com_sprintf (model_filename, sizeof(model_filename), "players/%s/body.mdx", model_name);
			ci->model[1] = R_RegisterModel (model_filename);
			Com_sprintf (model_filename, sizeof(model_filename), "players/%s/legs.mdx", model_name);
			ci->model[2] = R_RegisterModel (model_filename);
			// skin
			Com_sprintf (skin_filename, sizeof(skin_filename), "%s/head_001.tga", head_path);
			ci->skin[0] = R_RegisterSkin (skin_filename);
			Com_sprintf (skin_filename, sizeof(skin_filename), "%s/body_001.tga", body_path);
			ci->skin[1] = R_RegisterSkin (skin_filename);
			Com_sprintf (skin_filename, sizeof(skin_filename), "%s/legs_001.tga", legs_path);
			ci->skin[2] = R_RegisterSkin (skin_filename);
		}


		// weapon file
		for (i = 0; i < num_cl_weaponmodels; i++) {
			Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
			ci->weaponmodel[i] = R_RegisterModel(weapon_filename);

			if (!ci->weaponmodel[i] && strcmp(model_name, "male_thug") == 0) {
				// try male_thug //hypov8 todo: this is proably a bad idea
				Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male_thug/%s", cl_weaponmodels[i]);
				ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			}
			if (!ci->weaponmodel[i] && strcmp(model_name, "male_thug") == 0) {
				// try male_thug //hypov8 todo: this is proably a bad idea
				Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male_thug/w_tommygun.mdx");
				ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			}

			if (!cl_vwep->value)
				break; // only one when vwep is off
		}
	}

	// must have loaded all data types to be valud
	if (!ci->skin[0] ||!ci->skin[1] ||!ci->skin[2] 
		|| !ci->model[0] ||!ci->model[1] ||!ci->model[2] 
		|| !ci->weaponmodel[0])
	{
		ci->skin[0] = NULL;
		ci->skin[1] = NULL;
		ci->skin[2] = NULL;
		ci->model[0] = NULL;
		ci->model[1] = NULL;
		ci->model[2] = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}
#endif
/*
================
CL_ParseClientinfo

Load the skin, icon, and model for a client
================
*/
void CL_ParseClientinfo (int player)
{
	char			*s;
	clientinfo_t	*ci;

	// Knightmare- 1/2/2002- GROSS HACK for old demos or
	// connected to server using old protocol
	// Changed config strings require different offsets
	if ( LegacyProtocol() )
		s = cl.configstrings[player+OLD_CS_PLAYERSKINS];
	else
		s = cl.configstrings[player+CS_PLAYERSKINS];
	//end Knightmare

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo (ci, s);
}


/*
================
CL_MissionPackCDTrack
Returns correct OGG track number for mission packs.
This assumes that the standard Q2 CD was ripped
as track02-track11, and the Rogue CD as track12-track21.
================
*/
int CL_MissionPackCDTrack (int tracknum)
{
	if (modType("rogue") || cl_rogue_music->value)
	{
		if (tracknum >= 2 && tracknum <= 11)
			return tracknum + 10;
		else
			return tracknum;
	}
	// an out-of-order mix from Q2 and Rogue CDs
	else if (modType("xatrix") || cl_xatrix_music->value)
	{
		switch(tracknum)
		{
			case 2: return 9;	break;
			case 3: return 13;	break;
			case 4: return 14;	break;
			case 5: return 7;	break;
			case 6: return 16;	break;
			case 7: return 2;	break;
			case 8: return 15;	break;
			case 9: return 3;	break;
			case 10: return 4;	break;
			case 11: return 18; break;
			default: return tracknum; break;
		}
	}
	else
		return tracknum;
}


/*
=================
CL_PlayBackgroundTrack
=================
*/
#ifdef OGG_SUPPORT

#include "snd_ogg.h"

void CL_PlayBackgroundTrack (void)
{
	char	name[MAX_QPATH];
	int		track;

	if (!cl.refresh_prepped)
		return;

	// using a named audio track intead of numbered
	if (strlen(cl.configstrings[CS_CDTRACK]) > 2)
	{
		Com_sprintf (name, sizeof(name), "music/%s.ogg", cl.configstrings[CS_CDTRACK]);
		if (FS_LoadFile(name, NULL) != -1)
		{
			CDAudio_Stop();
			S_StartBackgroundTrack(name, name);
			return;
		}
	}

	track = atoi(cl.configstrings[CS_CDTRACK]);

	if (track == 0)
	{	// Stop any playing track
		CDAudio_Stop();
		S_StopBackgroundTrack();
		return;
	}

	// If an OGG file exists play it, otherwise fall back to CD audio
	Com_sprintf (name, sizeof(name), "music/track%02i.ogg", CL_MissionPackCDTrack(track));
	if ( (FS_LoadFile(name, NULL) != -1) && cl_ogg_music->value )
		S_StartBackgroundTrack(name, name);
	else
		CDAudio_Play(track, true);
}

#else

void CL_PlayBackgroundTrack (void)
{
	CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
}

#endif // OGG_SUPPORT

/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString (void)
{
	int		i;
	int		max_models, max_sounds, max_images, cs_lights, cs_sounds, cs_images, cs_playerskins;
	char	*s;
	char	olds[MAX_QPATH];
#if KINGPIN
	int max_modelSkins, cs_modelskins;
#endif

	// Knightmare- hack for connected to server using old protocol
	// Changed config strings require different parsing
	if ( LegacyProtocol() )
	{
		max_models = OLD_MAX_MODELS;
		max_sounds = OLD_MAX_SOUNDS;
		max_images = OLD_MAX_IMAGES;

		cs_lights = OLD_CS_LIGHTS;
		cs_sounds = OLD_CS_SOUNDS;
		cs_images = OLD_CS_IMAGES;
		cs_playerskins = OLD_CS_PLAYERSKINS;
#if KINGPIN
		max_modelSkins = OLD_MAX_MODELS;
		cs_modelskins = OLD_CS_MODELSKINS;
#endif
	}
	else
	{
		max_models = MAX_MODELS;
		max_sounds = MAX_SOUNDS;
		max_images = MAX_IMAGES;

		cs_lights = CS_LIGHTS;
		cs_sounds = CS_SOUNDS;
		cs_images = CS_IMAGES;
		cs_playerskins = CS_PLAYERSKINS;
#if KINGPIN
		max_modelSkins = MAX_MODELS;
		cs_modelskins = CS_MODELSKINS;
#endif
	}

	i = MSG_ReadShort (&net_message);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	s = MSG_ReadString(&net_message);

	strncpy (olds, cl.configstrings[i], sizeof(olds));
	olds[sizeof(olds) - 1] = 0;

	// Don't use a null-terminated strncpy here!!
//	strcpy (cl.configstrings[i], s);
	strncpy (cl.configstrings[i], s, sizeof(cl.configstrings[i]));

	if (i == CS_STATUSBAR+1)
	{
		int j;
		int len = strlen(s);
		len = ceil(len/64);

		if ((i+len >= CS_SERVER_VERSION))
			Com_Error (ERR_DROP, "HUD configstring to long");

		for (j = 0; j < len; j++)
		{
			s += 64;
			strncpy (cl.configstrings[i+j+1], s, sizeof(cl.configstrings[i+j+1]));
		}
	}

	// do something apropriate 
	else if (i >= cs_lights && i < cs_lights+MAX_LIGHTSTYLES)
		CL_SetLightstyle (i - cs_lights);
	else if (i == CS_CDTRACK)
	{
		if (cl.refresh_prepped)
			CL_PlayBackgroundTrack ();
	}
	else if (i == CS_MAXCLIENTS)	// from R1Q2
	{
		if (!cl.attractloop)
			cl.maxclients = atoi(cl.configstrings[CS_MAXCLIENTS]);
	}
	else if (i >= CS_MODELS && i < CS_MODELS+max_models)
	{
		if (cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = R_RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	}
	else if (i >= cs_sounds && i < cs_sounds+max_sounds) //Knightmare- was MAX_MODELS
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-cs_sounds] = S_RegisterSound (cl.configstrings[i]);
	}
	else if (i >= cs_images && i < cs_images+max_images) //Knightmare- was MAX_MODELS
	{
		if (cl.refresh_prepped)
			cl.image_precache[i-cs_images] = R_DrawFindPic (cl.configstrings[i]);
	}
	else if (i >= cs_playerskins && i < cs_playerskins+MAX_CLIENTS)
	{
		// from R1Q2- a hack to avoid parsing non-skins from mods that overload CS_PLAYERSKINS
		if ( (i-cs_playerskins) < cl.maxclients ) {
			if (cl.refresh_prepped && strcmp(olds, s))
				CL_ParseClientinfo (i-cs_playerskins);
		}
		else {
			Com_DPrintf ("CL_ParseConfigString: Ignoring out-of-range playerskin %d (%s)\n", i, MakePrintable(s, 0));
		}
	}
#if KINGPIN	
	else 	if (i >= cs_modelskins && i < cs_modelskins +max_modelSkins)
	{
		if (cl.refresh_prepped)
		{
			int len = strlen(s);
			int len2 = MSG_ReadByte(&net_message);

			if (len2+len < MAX_QPATH)
				strncpy(cl.configstrings[i], s, sizeof(cl.configstrings[i]));
			//addSkins();
		}
	}
#endif
}


/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos_v;
	float	*pos;
    int 	channel, ent;
    int 	sound_num;
    float 	volume;
    float 	attenuation;  
	int		flags;
	float	ofs;

	flags = MSG_ReadByte (&net_message);

	// Knightmare- 12/23/2001
	// read sound indices as bytes only if playing old demos or
	// connected to server using old protocol; otherwise, read as shorts
	if ( LegacyProtocol() )
		sound_num = MSG_ReadByte (&net_message);
	else
		sound_num = MSG_ReadShort (&net_message);
	//end Knightmare

    if (flags & SND_VOLUME)
		volume = MSG_ReadByte (&net_message) / 255.0;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte (&net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;	

    if (flags & SND_OFFSET)
		ofs = MSG_ReadByte (&net_message) / 1000.0;
	else
		ofs = 0;

	if (flags & SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(&net_message); 
		ent = channel>>3;
		if (ent > MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{	// positioned in space
		MSG_ReadPos (&net_message, pos_v);
 
		pos = pos_v;
	}
	else	// use entity number
		pos = NULL;

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound (pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}       


void SHOWNET(char *s)
{
	if (cl_shownet->value>=2)
		Com_Printf ("%3i:%s\n", net_message.readcount-1, s);
}

/*
=====================
CL_ParseStuffText
Catches stuffed quit or error commands from the server.
Shutting down suddenly in this way can hang some SMP systems.
This simply disconnects, same effect as kicking player.
=====================
*/
/*qboolean CL_ParseStuffText (char *stufftext)
{
	char	*parsetext = stufftext;

	// skip leading spaces
	while (*parsetext == ' ') parsetext++;
	if (strncmp(parsetext, "quit", 4))
	{
		Com_Printf("server stuffed quit command, disconnecting...\n");
		CL_Disconnect ();
		return false;
	}
	if (strncmp(parsetext, "error", 5))    	
	{
		Com_Printf("server stuffed error command, disconnecting...\n");
		CL_Disconnect ();
		return false;
	}
	return true;
}*/


// Knightmare- server-controlled fog
/*
=====================
CL_ParseFog
=====================
*/
// Fog is sent like this:
// gi.WriteByte (svc_fog); // svc_fog = 21
// gi.WriteByte (fog_enable); // 1 = on, 0 = off
// gi.WriteByte (fog_model); // 0, 1, or 2
// gi.WriteByte (fog_density); // 1-100
// gi.WriteShort (fog_near); // >0, <fog_far
// gi.WriteShort (fog_far); // >fog_near-64, <5000
// gi.WriteByte (fog_red); // 0-255
// gi.WriteByte (fog_green); // 0-255
// gi.WriteByte (fog_blue); // 0-255
// gi.unicast (player_ent, true); 

void CL_ParseFog (void)
{
	qboolean fogenable;
	int model, density, start, end,
			red, green, blue, temp;

	temp = MSG_ReadByte (&net_message);
	fogenable = (temp > 0) ? true:false;
	model = MSG_ReadByte (&net_message);
	density = MSG_ReadByte (&net_message);
	start = MSG_ReadShort (&net_message);
	end = MSG_ReadShort (&net_message);
	red = MSG_ReadByte (&net_message);
	green = MSG_ReadByte (&net_message);
	blue = MSG_ReadByte (&net_message);

	R_SetFogVars (fogenable, model, density, start, end, red, green, blue);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int			cmd;
	char		*s;
	int			i;
	static int lastCmd = 0; //hypov8 debug prev cmd

//
// if recording demos, copy the message out
//
	if (cl_shownet->value == 1)
		Com_Printf ("%i ",net_message.cursize);
	else if (cl_shownet->value >= 2)
		Com_Printf ("------------------\n");


//
// parse the message
//
	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value>=2)
		{
			if (!svc_strings[cmd])
				Com_Printf ("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}

	// other commands
		switch (cmd)
		{
		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Illegible server message\n");
			break;
			
		case svc_nop:
//			Com_Printf ("svc_nop\n");
			break;
			
		case svc_disconnect:
			Com_Error (ERR_DISCONNECT,"Server disconnected\n");
			break;

		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n");
			if (cls.download) {
				//ZOID, close download
				fclose (cls.download);
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
			break;

		case svc_print:
			i = MSG_ReadByte (&net_message);
			if (i == PRINT_CHAT)
			{
#if !KINGPIN 
				S_StartLocalSound ("misc/talk.wav");
				con.ormask = 128;
				Com_Printf (S_COLOR_ALT"%s", MSG_ReadString (&net_message)); // Knightmare- add green flag
#else
				char st[1024];
				S_StartLocalSound ("misc/talk.wav");
				con.ormask = 128;
				Com_sprintf(st, sizeof(st), "%s", MSG_ReadString(&net_message));
				Com_Printf(S_COLOR_ALT"%s", st); // Knightmare- add green flag
				SCR_ChatPrint(st);
#endif
			}
			else
				Com_Printf ("%s", MSG_ReadString (&net_message));
			con.ormask = 0;
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString (&net_message));
			break;
			
		case svc_stufftext:
			s = MSG_ReadString (&net_message);
			Com_DPrintf ("stufftext: %s\n", s);
			Cbuf_AddText (s);
			break;
			
		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CL_ParseServerData ();
			break;
			
		case svc_configstring:
			CL_ParseConfigString ();
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket();
			break;
			
		case svc_spawnbaseline:
			CL_ParseBaseline ();
			break;

		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_muzzleflash:
			CL_ParseMuzzleFlash ();
			break;

		case svc_muzzleflash2:
			CL_ParseMuzzleFlash2 ();
			break;
#if KINGPIN //hypov8 todo:?
		case svc_muzzleflash3:
			CL_ParseMuzzleFlash3 ();
			break;
		case svc_pushdownload:
			CL_ParsePushDownload ();
			break;
#endif

		case svc_download:
			CL_ParseDownload ();
			break;

		case svc_frame:
			CL_ParseFrame ();
			break;

		case svc_inventory:
			CL_ParseInventory ();
			break;

		case svc_fog:	// Knightmare added
			CL_ParseFog ();
			break;

		case svc_layout:
			s = MSG_ReadString (&net_message);
			strncpy (cl.layout, s, sizeof(cl.layout)-1);
			break;

		case svc_playerinfo:
		case svc_packetentities:
		case svc_deltapacketentities:
			Com_Error (ERR_DROP, "Out of place frame data");
			break;
		}
		lastCmd = cmd;//hypov8 todo: debug
	}


	CL_AddNetgraph ();

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if (cls.demorecording && !cls.demowaiting)
		CL_WriteDemoMessage ();

}


