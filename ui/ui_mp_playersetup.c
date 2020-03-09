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

// ui_playersetup.c -- the player setup menu 

#include <ctype.h>
#ifdef _WIN32
#include <io.h>
#endif
#include "../client/client.h"
#include "ui_local.h"


/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/
extern menuframework_s	s_multiplayer_menu;

static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
#if KINGPIN
static menulist_s		s_player_skin_boxKP[3]; //KINGPIN 
static menuseparator_s	s_player_skin_titleKP[3]; //KINGPIN 
#else
static menulist_s		s_player_skin_box;
static menuseparator_s	s_player_skin_title;
#endif
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;
//static menuaction_s		s_player_download_action;
static menuaction_s		s_player_back_action;

// save skins and models here so as to not have to re-register every frame
#if KINGPIN
struct model_s *playermodelKP[3];
struct image_s *playerskinKP[3];
#else
struct model_s *playermodel;
struct image_s *playerskin;
#endif

struct model_s *weaponmodel;
char *currentweaponmodel;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024
#define	NUM_SKINBOX_ITEMS 7

typedef struct
{
#if KINGPIN
	int		nskinsKP[3];
	char	**skindisplaynamesKP[3];
#else
	int		nskins;
	char	**skindisplaynames;
#endif
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

#if KINGPIN
static char s_pmskinPathHead[MAX_QPATH];
static char s_pmskinPathBody[MAX_QPATH];
static char s_pmskinPathLegs[MAX_QPATH];
#endif


static int rate_tbl[] = { 2500, 3200, 5000, 10000, 15000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "56K/Single ISDN",
	"Dual ISDN", "Cable/DSL", "T1/LAN", "User defined", 0 };


static void HandednessCallback (void *unused)
{
	Cvar_SetValue ("hand", s_player_handedness_box.curvalue);
}


static void RateCallback (void *unused)
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		Cvar_SetValue ("rate", rate_tbl[s_player_rate_box.curvalue]);
}


static void ModelCallback (void *unused)
{
	char scratch[MAX_QPATH];
#if KINGPIN
	char		model_name[MAX_QPATH];
	char		head_path[MAX_QPATH];
	char		body_path[MAX_QPATH];
	char		legs_path[MAX_QPATH];

	//hypov8 todo: set current selection
	s_player_skin_boxKP[0].itemnames = s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[0];
	s_player_skin_boxKP[0].curvalue = 0;
	s_player_skin_boxKP[1].itemnames = s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[1];
	s_player_skin_boxKP[1].curvalue = 0;
	s_player_skin_boxKP[2].itemnames = s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[2];
	s_player_skin_boxKP[2].curvalue = 0;


	//check for skinfolder.txt
	Com_sprintf( model_name, sizeof(model_name), "players/%s", s_pmi[s_player_model_box.curvalue].directory );
	if (!CL_ParseSkinFfolderTxt(va("%s", model_name), head_path, body_path, legs_path))	{
		Com_sprintf(head_path, sizeof(head_path),"players/%s", model_name);
		Com_sprintf(body_path, sizeof(body_path),"players/%s", model_name);
		Com_sprintf(legs_path, sizeof(legs_path),"players/%s", model_name);
	}

	Q_strncpyz(s_pmskinPathHead, head_path, MAX_QPATH);
	Q_strncpyz(s_pmskinPathBody, body_path, MAX_QPATH);	
	Q_strncpyz(s_pmskinPathLegs, legs_path, MAX_QPATH);	

	// only register model and skin on starup or when changed
	Com_sprintf( scratch, sizeof(scratch), "players/%s/head.mdx", s_pmi[s_player_model_box.curvalue].directory );
	playermodelKP[0] = R_RegisterModel (scratch);
	Com_sprintf( scratch, sizeof(scratch), "players/%s/body.mdx", s_pmi[s_player_model_box.curvalue].directory );
	playermodelKP[1] = R_RegisterModel (scratch);
	Com_sprintf( scratch, sizeof(scratch), "players/%s/legs.mdx", s_pmi[s_player_model_box.curvalue].directory );
	playermodelKP[2] = R_RegisterModel (scratch);


	Com_sprintf( scratch, sizeof(scratch), "%s/%s.tga", head_path, s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[0][s_player_skin_boxKP[0].curvalue] );
	playerskinKP[0] = R_RegisterSkin (scratch);
	Com_sprintf( scratch, sizeof(scratch), "%s/%s.tga", body_path, s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[1][s_player_skin_boxKP[1].curvalue] );
	playerskinKP[1] = R_RegisterSkin (scratch);
	Com_sprintf( scratch, sizeof(scratch), "%s/%s.tga", legs_path, s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[2][s_player_skin_boxKP[2].curvalue] );
	playerskinKP[2] = R_RegisterSkin (scratch);
#else

	s_player_skin_box.itemnames = s_pmi[s_player_model_box.curvalue].skindisplaynames;
	s_player_skin_box.curvalue = 0;

	// only register model and skin on starup or when changed
	Com_sprintf( scratch, sizeof(scratch), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory );
	playermodel = R_RegisterModel (scratch);

	Com_sprintf( scratch, sizeof(scratch), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
	playerskin = R_RegisterSkin (scratch);
#endif
	// show current weapon model (if any)
	if (currentweaponmodel && strlen(currentweaponmodel)) {
		Com_sprintf (scratch, sizeof(scratch), "players/%s/%s", s_pmi[s_player_model_box.curvalue].directory, currentweaponmodel);
		weaponmodel = R_RegisterModel(scratch);
		if (!weaponmodel) {
			Com_sprintf (scratch, sizeof(scratch), "players/%s/w_pipe.mdx", s_pmi[s_player_model_box.curvalue].directory);
			weaponmodel = R_RegisterModel (scratch);
		}
	}
	else {
		Com_sprintf (scratch, sizeof(scratch), "players/%s/w_pipe.mdx", s_pmi[s_player_model_box.curvalue].directory);
		weaponmodel = R_RegisterModel (scratch);
	}
}

#if !KINGPIN
// only register skin on starup and when changed
static void SkinCallback (void *unused)
{
	char scratch[MAX_QPATH];

	Com_sprintf(scratch, sizeof(scratch), "players/%s/%s.tga", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
	playerskin = R_RegisterSkin(scratch);

}
#else
static void SkinCallbackKP_Head (void *unused)
{
	char scratch[MAX_QPATH];

	Com_sprintf(scratch, sizeof(scratch), "%s/%s.tga", s_pmskinPathHead, s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[0][s_player_skin_boxKP[0].curvalue]);
	playerskinKP[0] = R_RegisterSkin(scratch);
}
static void SkinCallbackKP_Body (void *unused)
{
	char scratch[MAX_QPATH];
	Com_sprintf(scratch, sizeof(scratch), "%s/%s.tga", s_pmskinPathBody, s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[1][s_player_skin_boxKP[1].curvalue]);
	playerskinKP[1] = R_RegisterSkin(scratch);
}
static void SkinCallbackKP_Legs (void *unused)
{
	char scratch[MAX_QPATH];
	Com_sprintf(scratch, sizeof(scratch), "%s/%s.tga", s_pmskinPathLegs, s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[2][s_player_skin_boxKP[2].curvalue]);
	playerskinKP[2] = R_RegisterSkin(scratch);
}

#endif


static qboolean IconOfSkinExists (char *skin, char **files, int nfiles, char *suffix)
{
	int i;
	char scratch[1024];

//	strncpy(scratch, skin);
	Q_strncpyz(scratch, skin, sizeof(scratch));
	*strrchr(scratch, '.') = 0;
//	strncat(scratch, suffix);
	Q_strncatz(scratch, suffix, sizeof(scratch));
	//strncat(scratch, "_i.pcx");

	for (i = 0; i < nfiles; i++)
	{
		if ( strcmp(files[i], scratch) == 0 )
			return true;
	}

	return false;
}


// adds menu support for TGA and JPG skins
static qboolean IsValidSkin (char **filelist, int numFiles, int index)
{
	int		len = strlen(filelist[index]);

	if (	!strcmp (filelist[index]+max(len-4,0), ".pcx")
		||  !strcmp (filelist[index]+max(len-4,0), ".tga")
		||  !strcmp (filelist[index]+max(len-4,0), ".png")
		||	!strcmp (filelist[index]+max(len-4,0), ".jpg") )
	{
#if !KINGPIN
		if (	strcmp (filelist[index]+max(len-6,0), "_i.pcx")
			&&  strcmp (filelist[index]+max(len-6,0), "_i.tga")
			&&  strcmp (filelist[index]+max(len-6,0), "_i.png")
			&&  strcmp (filelist[index]+max(len-6,0), "_i.jpg") )

			if (	IconOfSkinExists (filelist[index], filelist, numFiles-1 , "_i.pcx")
				||	IconOfSkinExists (filelist[index], filelist, numFiles-1 , "_i.tga")
				||	IconOfSkinExists (filelist[index], filelist, numFiles-1 , "_i.png")
				||	IconOfSkinExists (filelist[index], filelist, numFiles-1 , "_i.jpg"))
				return true;
#else
		if (	IconOfSkinExists (filelist[index], filelist, numFiles-1 , ".pcx")
			||	IconOfSkinExists (filelist[index], filelist, numFiles-1 , ".tga")
			||	IconOfSkinExists (filelist[index], filelist, numFiles-1 , ".png")
			||	IconOfSkinExists (filelist[index], filelist, numFiles-1 , ".jpg"))
			return true;

#endif
	}
	return false;
}


static qboolean PlayerConfig_ScanDirectories (void)
{
	char findname[1024];
	char scratch[1024];
	int ndirs = 0;	//num directories
	int npms = 0;	//num player models
	char **dirnames;
	char *path = NULL;
	int i;
	#if KINGPIN
	char model_name[MAX_QPATH];
	char head_path[MAX_QPATH];
	char body_path[MAX_QPATH];
	char legs_path[MAX_QPATH];
	#endif

	//extern char **FS_ListFiles (char *, int *, unsigned, unsigned);

	s_numplayermodels = 0;

	// loop back to here if there were no valid player models found in the selected path
	do
	{
		//
		// get a list of directories
		//
		do 
		{
			path = FS_NextPath(path);
			Com_sprintf( findname, sizeof(findname), "%s/players/*.*", path );

			if ( (dirnames = FS_ListFiles(findname, &ndirs, SFF_SUBDIR, 0)) != 0 )
				break;
		} while (path);

		if (!dirnames)
			return false;

		//
		// go through the subdirectories
		//
		npms = ndirs;
		if (npms > MAX_PLAYERMODELS)
			npms = MAX_PLAYERMODELS;
		if ( (s_numplayermodels + npms) > MAX_PLAYERMODELS )
			npms = MAX_PLAYERMODELS - s_numplayermodels;

		//hypov8 look for head, body, legs. model+skin
		for (i = 0; i < npms; i++)
		{
			int			k, s;
			char		*a, *b, *c;
#if !KINGPIN
			char		**skinnames;
			char		**imagenames;
			int			nimagefiles;
			int			nskins = 0;
#else
			char		**skinnamesKP[3];
			char		**imagenamesKP[3];
			int			numImagefilesKP[3]; //number of head/body/legs textures
			int			nskinsKP[3] = { 0, 0, 0 };
#endif
			qboolean	already_added = false;
			if (dirnames[i] == 0)
				continue;

			// check if dirnames[i] is already added to the s_pmi[i].directory list
			a = strrchr(dirnames[i], '/');
			b = strrchr(dirnames[i], '\\');
			c = (a > b) ? a : b;
#if KINGPIN
			//hypov8 get only male/female skin
			if(strncmp(c+1, "male_", 5)	&& strncmp(c+1, "female_", 7))
				continue;
#endif
			for (k=0; k < s_numplayermodels; k++)
				if (!strcmp(s_pmi[k].directory, c+1))
				{	
					already_added = true;	
					break;	
				}

			if (already_added)
			{	// todo: add any skins for this model not already listed to skindisplaynames
				continue;
			}

			// verify the existence of head.mdx, body.mdx, legs.mdx

#if KINGPIN
			Q_strncpyz(scratch, dirnames[i], sizeof(scratch));
			Q_strncatz(scratch, "/head.mdx", sizeof(scratch));
			if ( !Sys_FindFirst(scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM) )
			{	free(dirnames[i]);	dirnames[i] = 0; Sys_FindClose();
				continue;}
			Sys_FindClose();
			Q_strncpyz(scratch, dirnames[i], sizeof(scratch));
			Q_strncatz(scratch, "/body.mdx", sizeof(scratch));
			if ( !Sys_FindFirst(scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM) )
			{	free(dirnames[i]);	dirnames[i] = 0; Sys_FindClose();
				continue;}
			Sys_FindClose();
			Q_strncpyz(scratch, dirnames[i], sizeof(scratch));
			Q_strncatz(scratch, "/legs.mdx", sizeof(scratch));
			if ( !Sys_FindFirst(scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM) )
			{	free(dirnames[i]);	dirnames[i] = 0; Sys_FindClose();
				continue;}
			Sys_FindClose();

			//check for skinfolder.txt
			Com_sprintf( model_name, sizeof(model_name), "players%s", c );
			if (!CL_ParseSkinFfolderTxt(va("%s", model_name), head_path, body_path, legs_path))	{
				Com_sprintf(head_path, sizeof(head_path),"players/%s", c+1);
				Com_sprintf(body_path, sizeof(body_path),"players/%s", c+1);
				Com_sprintf(legs_path, sizeof(legs_path),"players/%s", c+1);
			}


			// verify the existence of at least one skin. head_001
			Q_strncpyz(scratch, va("%s/%s%s", path, head_path, "/head_???.*"), sizeof(scratch)); // was "/*.pcx"
			imagenamesKP[0] = FS_ListFiles (scratch, &numImagefilesKP[0], 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
			if (!imagenamesKP[0])
			{	free(dirnames[i]);	dirnames[i] = 0; continue;	}
			// count valid skins
			for (k = 0; k < numImagefilesKP[0]-1; k++)
				if ( IsValidSkin(imagenamesKP[0], numImagefilesKP[0], k) )
					nskinsKP[0]++;
			if (!nskinsKP[0])	continue;

			//body_001
			Q_strncpyz(scratch, va("%s/%s%s", path, body_path, "/body_???.*"), sizeof(scratch)); // was "/*.pcx"
			imagenamesKP[1] = FS_ListFiles (scratch, &numImagefilesKP[1], 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
			if (!imagenamesKP[1])
			{	free(dirnames[i]);	dirnames[i] = 0; continue;	}
			// count valid skins
			for (k = 0; k < numImagefilesKP[1]-1; k++)
				if ( IsValidSkin(imagenamesKP[1], numImagefilesKP[1], k) )
					nskinsKP[1]++;
			if (!nskinsKP[1])	continue;
			//legs_001
			Q_strncpyz(scratch, va("%s/%s%s", path, legs_path, "/legs_???.*"), sizeof(scratch)); // was "/*.pcx"
			imagenamesKP[2] = FS_ListFiles (scratch, &numImagefilesKP[2], 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
			if (!imagenamesKP[2])
			{	free(dirnames[i]);	dirnames[i] = 0; continue;	}
			// count valid skins
			for (k = 0; k < numImagefilesKP[2]-1; k++)
				if ( IsValidSkin(imagenamesKP[2], numImagefilesKP[2], k) )
					nskinsKP[2]++;
			if (!nskinsKP[2])	continue;

			skinnamesKP[0] = malloc(sizeof(char *) * (nskinsKP[0]+1));
			memset(skinnamesKP[0], 0, sizeof(char *) * (nskinsKP[0]+1));
			skinnamesKP[1] = malloc(sizeof(char *) * (nskinsKP[1]+1));
			memset(skinnamesKP[1], 0, sizeof(char *) * (nskinsKP[1]+1));
			skinnamesKP[2] = malloc(sizeof(char *) * (nskinsKP[2]+1));
			memset(skinnamesKP[2], 0, sizeof(char *) * (nskinsKP[2]+1));

		// copy the valid skins
			if (numImagefilesKP[0])
				for (s = 0, k = 0; k < numImagefilesKP[0]-1; k++) {
					char *a, *b, *c;
					if ( IsValidSkin(imagenamesKP[0], numImagefilesKP[0], k) )
					{
						a = strrchr(imagenamesKP[0][k], '/');
						b = strrchr(imagenamesKP[0][k], '\\');
						c = (a > b) ? a : b;
						Q_strncpyz(scratch, c+1, sizeof(scratch));

						if ( strrchr(scratch, '.') )
							*strrchr(scratch, '.') = 0;

						skinnamesKP[0][s] = strdup(scratch);
						s++;
					}
				}
			if (numImagefilesKP[1])
				for (s = 0, k = 0; k < numImagefilesKP[1]-1; k++) {
					char *a, *b, *c;
					if ( IsValidSkin(imagenamesKP[1], numImagefilesKP[1], k) )
					{
						a = strrchr(imagenamesKP[1][k], '/');
						b = strrchr(imagenamesKP[1][k], '\\');
						c = (a > b) ? a : b;
						Q_strncpyz(scratch, c+1, sizeof(scratch));

						if ( strrchr(scratch, '.') )
							*strrchr(scratch, '.') = 0;

						skinnamesKP[1][s] = strdup(scratch);
						s++;
					}
				}
			if (numImagefilesKP[2])
				for (s = 0, k = 0; k < numImagefilesKP[2]-1; k++) {
					char *a, *b, *c;
					if ( IsValidSkin(imagenamesKP[2], numImagefilesKP[2], k) )
					{
						a = strrchr(imagenamesKP[2][k], '/');
						b = strrchr(imagenamesKP[2][k], '\\');
						c = (a > b) ? a : b;
						Q_strncpyz(scratch, c+1, sizeof(scratch));

						if ( strrchr(scratch, '.') )
							*strrchr(scratch, '.') = 0;

						skinnamesKP[2][s] = strdup(scratch);
						s++;
					}
				}
			// at this point we have a valid player model
			s_pmi[s_numplayermodels].nskinsKP[0] = nskinsKP[0];
			s_pmi[s_numplayermodels].skindisplaynamesKP[0] = skinnamesKP[0];
			s_pmi[s_numplayermodels].nskinsKP[1] = nskinsKP[1];
			s_pmi[s_numplayermodels].skindisplaynamesKP[1] = skinnamesKP[1];
			s_pmi[s_numplayermodels].nskinsKP[2] = nskinsKP[2];
			s_pmi[s_numplayermodels].skindisplaynamesKP[2] = skinnamesKP[2];

			// make short name for the model
			a = strrchr(dirnames[i], '/');
			b = strrchr(dirnames[i], '\\');

			c = (a > b) ? a : b;

			strncpy(s_pmi[s_numplayermodels].displayname, c+1, MAX_DISPLAYNAME-1);
		//	strncpy(s_pmi[s_numplayermodels].directory, c+1);
			Q_strncpyz(s_pmi[s_numplayermodels].directory, c+1, sizeof(s_pmi[s_numplayermodels].directory));

			FS_FreeFileList (imagenamesKP[0], numImagefilesKP[0]);
			FS_FreeFileList (imagenamesKP[1], numImagefilesKP[1]);
			FS_FreeFileList (imagenamesKP[2], numImagefilesKP[2]);

#else
			Q_strncpyz(scratch, dirnames[i], sizeof(scratch));
			Q_strncatz(scratch, "/tris.md2", sizeof(scratch));
			if ( !Sys_FindFirst(scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM) )
			{
				free(dirnames[i]);
				dirnames[i] = 0;
				Sys_FindClose();
				continue;
			}
			Sys_FindClose();

			// verify the existence of at least one skin
		//	strncpy(scratch, va("%s%s", dirnames[i], "/*.*")); // was "/*.pcx"
			Q_strncpyz(scratch, va("%s%s", dirnames[i], "/*.*"), sizeof(scratch)); // was "/*.pcx"
			imagenames = FS_ListFiles (scratch, &nimagefiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);

			if (!imagenames)
			{
				free(dirnames[i]);
				dirnames[i] = 0;
				continue;
			}

			// count valid skins, which consist of a skin with a matching "_i" icon
			for (k = 0; k < nimagefiles-1; k++)
				if ( IsValidSkin(imagenames, nimagefiles, k) )
					nskins++;

			if (!nskins)
				continue;
			skinnames = malloc(sizeof(char *) * (nskins+1));
			memset(skinnames, 0, sizeof(char *) * (nskins+1));



			// copy the valid skins
			if (nimagefiles)
				for (s = 0, k = 0; k < nimagefiles-1; k++)
				{
					char *a, *b, *c;
					if ( IsValidSkin(imagenames, nimagefiles, k) )
					{
						a = strrchr(imagenames[k], '/');
						b = strrchr(imagenames[k], '\\');

						c = (a > b) ? a : b;

					//	strncpy(scratch, c+1);
						Q_strncpyz(scratch, c+1, sizeof(scratch));

						if ( strrchr(scratch, '.') )
							*strrchr(scratch, '.') = 0;

						skinnames[s] = strdup(scratch);
						s++;
					}
				}

			// at this point we have a valid player model
			s_pmi[s_numplayermodels].nskins = nskins;
			s_pmi[s_numplayermodels].skindisplaynames = skinnames;

			// make short name for the model
			a = strrchr(dirnames[i], '/');
			b = strrchr(dirnames[i], '\\');

			c = (a > b) ? a : b;

			strncpy(s_pmi[s_numplayermodels].displayname, c+1, MAX_DISPLAYNAME-1);
		//	strncpy(s_pmi[s_numplayermodels].directory, c+1);
			Q_strncpyz(s_pmi[s_numplayermodels].directory, c+1, sizeof(s_pmi[s_numplayermodels].directory));

			FS_FreeFileList (imagenames, nimagefiles);
#endif
			s_numplayermodels++;
		}
		
		if (dirnames)
			FS_FreeFileList (dirnames, ndirs);

	// if no valid player models found in path,
	// try next path, if there is one
	} while (path);	// (s_numplayermodels == 0 && path);

	return true;	//** DMP warning fix
}


static int pmicmpfnc (const void *_a, const void *_b)
{
	const playermodelinfo_s *a = (const playermodelinfo_s *) _a;
	const playermodelinfo_s *b = (const playermodelinfo_s *) _b;

	//
	// sort by male, female, then alphabetical
	//
	if ( strcmp(a->directory, "male") == 0 )
		return -1;
	else if (strcmp( b->directory, "male") == 0 )
		return 1;

	if ( strcmp(a->directory, "female") == 0 )
		return -1;
	else if (strcmp( b->directory, "female") == 0 )
		return 1;

	return strcmp(a->directory, b->directory);
}


qboolean PlayerConfig_MenuInit (void)
{
	extern cvar_t *name;
	extern cvar_t *team;
	extern cvar_t *skin;
	char currentdirectory[1024];
	char currentskin[1024];
	#if KINGPIN
	char currentskinKP[3][16] = {"","",""};
	char model_name[MAX_QPATH];
	char head_path[MAX_QPATH];
	char body_path[MAX_QPATH];
	char legs_path[MAX_QPATH];
	#endif
	char scratch[MAX_QPATH];
	int i = 0;
	int y = 0;

	int currentdirectoryindex = 0;
	#if !KINGPIN
	int currentskinindex = 0;
	#else
	int currentskinindexKP[3] = {0,0,0};

	#endif
	cvar_t *hand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );

	static const char *handedness[] = { "right", "left", "center", 0 };

	PlayerConfig_ScanDirectories ();

	if (s_numplayermodels == 0)
		return false;

	if ( hand->value < 0 || hand->value > 2 )
		Cvar_SetValue( "hand", 0 );

//	strncpy( currentdirectory, skin->string );
	Q_strncpyz( currentdirectory, skin->string, sizeof(currentdirectory) );

	if ( strchr( currentdirectory, '/' ) )
	{
	//	strncpy( currentskin, strchr( currentdirectory, '/' ) + 1 );
		Q_strncpyz( currentskin, strchr( currentdirectory, '/' ) + 1, sizeof(currentskin) );
		*strchr( currentdirectory, '/' ) = 0;
	}
	else if ( strchr( currentdirectory, '\\' ) )
	{
	//	strncpy( currentskin, strchr( currentdirectory, '\\' ) + 1 );
		Q_strncpyz( currentskin, strchr( currentdirectory, '\\' ) + 1, sizeof(currentskin) );
		*strchr( currentdirectory, '\\' ) = 0;
	}
	else
	{
	//	strncpy( currentdirectory, "male" );
	//	strncpy( currentskin, "grunt" );
		Q_strncpyz( currentdirectory, "male", sizeof(currentdirectory) );
		Q_strncpyz( currentskin, "grunt", sizeof(currentskin) );
	}

#if KINGPIN
	if (strchr(currentskin, ' '))
	{
		char *tmp, *token;
		int x;
		//strcpy(tmp, currentskin);
		tmp = currentskin;
		for (x = 0; x < 3; x++)
		{
			token = COM_Parse(&tmp);
			if (token)
			{
				strcpy(currentskinKP[x], token);
			}
		}
	}
#endif


	qsort( s_pmi, s_numplayermodels, sizeof( s_pmi[0] ), pmicmpfnc );

	memset( s_pmnames, 0, sizeof( s_pmnames ) );
	for ( i = 0; i < s_numplayermodels; i++ )
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if ( Q_stricmp( s_pmi[i].directory, currentdirectory ) == 0 )
		{
			int j;

			currentdirectoryindex = i;

			for ( j = 0; j < s_pmi[i].nskinsKP[0]; j++ )			{
				if ( Q_stricmp( s_pmi[i].skindisplaynamesKP[0][j], va("head_%s", currentskinKP[0]) ) == 0 )	{
					currentskinindexKP[0] = j;
					break;	}
			}

			for ( j = 0; j < s_pmi[i].nskinsKP[1]; j++ )			{
				if ( Q_stricmp( s_pmi[i].skindisplaynamesKP[1][j], va("body_%s", currentskinKP[1]) ) == 0 )	{
					currentskinindexKP[1] = j;
					break;	}
			}

			for ( j = 0; j < s_pmi[i].nskinsKP[2]; j++ )			{
				if ( Q_stricmp( s_pmi[i].skindisplaynamesKP[2][j], va("legs_%s", currentskinKP[2]) ) == 0 )	{
					currentskinindexKP[2] = j;
					break;	}
			}
		}
	}
	
	s_player_config_menu.x = SCREEN_WIDTH*0.5 - 120;
	s_player_config_menu.y = SCREEN_HEIGHT*0.5 - 140/*- 70*/; //hypov8
	s_player_config_menu.nitems = 0;
	
	s_player_name_field.generic.type = MTYPE_FIELD;
	//s_player_name_field.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_name_field.generic.name = "NAME   ";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x		= -MENU_FONT_SIZE;
	s_player_name_field.generic.y		= y;
	s_player_name_field.length	= 20;
	s_player_name_field.visible_length = 20;
	Q_strncpyz( s_player_name_field.buffer, name->string, sizeof(s_player_name_field.buffer) );
	s_player_name_field.cursor = strlen( name->string );
	
	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	//s_player_model_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_model_title.generic.name = "MODEL    ";
	s_player_model_title.generic.x    = -MENU_FONT_SIZE;
	s_player_model_title.generic.y	 = y += 3*MENU_LINE_SIZE;
	
	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x	= -12*MENU_FONT_SIZE;
	s_player_model_box.generic.y	= y += MENU_LINE_SIZE;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -11*MENU_FONT_SIZE;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = s_pmnames;
#if !KINGPIN	
	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_skin_title.generic.name = "HEAD SKIN"; //hypov8
	s_player_skin_title.generic.x    = -2*MENU_FONT_SIZE;
	s_player_skin_title.generic.y	 = y += 2*MENU_LINE_SIZE;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x	= -7*MENU_FONT_SIZE;
	s_player_skin_box.generic.y	= y += MENU_LINE_SIZE;
	s_player_skin_box.generic.name	= 0;
	s_player_skin_box.generic.callback = SkinCallback; // Knightmare added, was 0
	s_player_skin_box.generic.cursor_offset = -6*MENU_FONT_SIZE;
	s_player_skin_box.curvalue = currentskinindex;
	s_player_skin_box.itemnames = s_pmi[currentdirectoryindex].skindisplaynames;
	s_player_skin_box.generic.flags |= QMF_SKINLIST;
#else
	s_player_skin_titleKP[0].generic.type = MTYPE_SEPARATOR;
	//s_player_skin_titleKP[0].generic.flags = QMF_LEFT_JUSTIFY;
	s_player_skin_titleKP[0].generic.name = "HEAD SKIN"; //hypov8
	s_player_skin_titleKP[0].generic.x = -MENU_FONT_SIZE;
	s_player_skin_titleKP[0].generic.y	 = y += 2*MENU_LINE_SIZE;

	s_player_skin_boxKP[0].generic.type = MTYPE_SPINCONTROL;
	s_player_skin_boxKP[0].generic.x	= -12*MENU_FONT_SIZE;
	s_player_skin_boxKP[0].generic.y	= y += MENU_LINE_SIZE;
	s_player_skin_boxKP[0].generic.name	= 0;
	s_player_skin_boxKP[0].generic.callback = SkinCallbackKP_Head; // Knightmare added, was 0
	s_player_skin_boxKP[0].generic.cursor_offset = -11*MENU_FONT_SIZE;
	s_player_skin_boxKP[0].curvalue = currentskinindexKP[0];
	s_player_skin_boxKP[0].itemnames = s_pmi[currentdirectoryindex].skindisplaynamesKP[0];
	s_player_skin_boxKP[0].generic.flags |= QMF_SKINLIST;

	s_player_skin_titleKP[1].generic.type = MTYPE_SEPARATOR;
	//s_player_skin_titleKP[1].generic.flags = QMF_LEFT_JUSTIFY;
	s_player_skin_titleKP[1].generic.name = "BODY SKIN"; //hypov8
	s_player_skin_titleKP[1].generic.x    = -MENU_FONT_SIZE;
	s_player_skin_titleKP[1].generic.y	 = y += 1.5*MENU_LINE_SIZE;

	s_player_skin_boxKP[1].generic.type = MTYPE_SPINCONTROL;
	s_player_skin_boxKP[1].generic.x	= -12*MENU_FONT_SIZE;
	s_player_skin_boxKP[1].generic.y	= y += MENU_LINE_SIZE;
	s_player_skin_boxKP[1].generic.name	= 0;
	s_player_skin_boxKP[1].generic.callback = SkinCallbackKP_Body; // Knightmare added, was 0
	s_player_skin_boxKP[1].generic.cursor_offset = -11*MENU_FONT_SIZE;
	s_player_skin_boxKP[1].curvalue = currentskinindexKP[1];
	s_player_skin_boxKP[1].itemnames = s_pmi[currentdirectoryindex].skindisplaynamesKP[1];
	s_player_skin_boxKP[1].generic.flags |= QMF_SKINLIST;

	s_player_skin_titleKP[2].generic.type = MTYPE_SEPARATOR;
	//s_player_skin_titleKP[2].generic.flags = QMF_LEFT_JUSTIFY;
	s_player_skin_titleKP[2].generic.name = "LEGS SKIN"; //hypov8
	s_player_skin_titleKP[2].generic.x    = -MENU_FONT_SIZE;
	s_player_skin_titleKP[2].generic.y	 = y += 1.5*MENU_LINE_SIZE;

	s_player_skin_boxKP[2].generic.type = MTYPE_SPINCONTROL;
	s_player_skin_boxKP[2].generic.x	= -12*MENU_FONT_SIZE;
	s_player_skin_boxKP[2].generic.y	= y += MENU_LINE_SIZE;
	s_player_skin_boxKP[2].generic.name	= 0;
	s_player_skin_boxKP[2].generic.callback = SkinCallbackKP_Legs ; // Knightmare added, was 0
	s_player_skin_boxKP[2].generic.cursor_offset = -11*MENU_FONT_SIZE;
	s_player_skin_boxKP[2].curvalue = currentskinindexKP[2];
	s_player_skin_boxKP[2].itemnames = s_pmi[currentdirectoryindex].skindisplaynamesKP[2];
	s_player_skin_boxKP[2].generic.flags |= QMF_SKINLIST;

#endif

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	//s_player_hand_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_hand_title.generic.name = "HAND     ";
	s_player_hand_title.generic.x    = -MENU_FONT_SIZE;
	s_player_hand_title.generic.y	 = y += 2*MENU_LINE_SIZE;
	
	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x	= -12*MENU_FONT_SIZE;
	s_player_handedness_box.generic.y	= y += MENU_LINE_SIZE;
	s_player_handedness_box.generic.name = 0;
	s_player_handedness_box.generic.cursor_offset = -11*MENU_FONT_SIZE;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue = Cvar_VariableValue( "hand" );
	s_player_handedness_box.itemnames = handedness;
	
	for (i = 0; i < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; i++)
		if (Cvar_VariableValue("rate") == rate_tbl[i])
			break;
		
	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	//s_player_rate_title.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_rate_title.generic.name = "NET SPEED";
	s_player_rate_title.generic.x    = -MENU_FONT_SIZE;
	s_player_rate_title.generic.y	 = y += 2*MENU_LINE_SIZE;
		
	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x	= -12*MENU_FONT_SIZE;
	s_player_rate_box.generic.y	= y += MENU_LINE_SIZE;
	s_player_rate_box.generic.name	= 0;
	s_player_rate_box.generic.cursor_offset = -11*MENU_FONT_SIZE;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;
	
	s_player_back_action.generic.type = MTYPE_ACTION;
	s_player_back_action.generic.name	= "back to multiplayer";
	s_player_back_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_player_back_action.generic.x	= -12*MENU_FONT_SIZE;
	s_player_back_action.generic.y	= y += 2*MENU_LINE_SIZE;
	s_player_back_action.generic.statusbar = NULL;
	s_player_back_action.generic.callback = UI_BackMenu;

	// only register model and skin on starup or when changed
#if !KINGPIN
	Com_sprintf( scratch, sizeof( scratch ), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory );
	playermodel = R_RegisterModel( scratch );

	Com_sprintf( scratch, sizeof( scratch ), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
	playerskin = R_RegisterSkin( scratch );
#else

	//check for skinfolder.txt
	Com_sprintf( model_name, sizeof(model_name), "players/%s", s_pmi[s_player_model_box.curvalue].directory );
	if (!CL_ParseSkinFfolderTxt(va("%s", model_name), head_path, body_path, legs_path))	{
		Com_sprintf(head_path, sizeof(head_path),"%s", model_name);
		Com_sprintf(body_path, sizeof(body_path),"%s", model_name);
		Com_sprintf(legs_path, sizeof(legs_path),"%s", model_name);
	}

	Q_strncpyz(s_pmskinPathHead, head_path, MAX_QPATH);
	Q_strncpyz(s_pmskinPathBody, body_path, MAX_QPATH);	
	Q_strncpyz(s_pmskinPathLegs, legs_path, MAX_QPATH);	

	Com_sprintf( scratch, sizeof( scratch ), "%s/head.mdx", model_name );
	playermodelKP[0] = R_RegisterModel( scratch );
	Com_sprintf( scratch, sizeof( scratch ), "%s/body.mdx", model_name );
	playermodelKP[1] = R_RegisterModel( scratch );
	Com_sprintf( scratch, sizeof( scratch ), "%s/legs.mdx", model_name );
	playermodelKP[2] = R_RegisterModel( scratch );

	//hypov8 todo: failsafe?
	Com_sprintf( scratch, sizeof( scratch ), "%s/head_%s.tga", head_path, currentskinKP[0] );
	playerskinKP[0] = R_RegisterSkin( scratch );
	Com_sprintf( scratch, sizeof( scratch ), "%s/body_%s.tga", body_path, currentskinKP[1] );
	playerskinKP[1] = R_RegisterSkin( scratch );
	Com_sprintf( scratch, sizeof( scratch ), "%s/legs_%s.tga", legs_path, currentskinKP[2] );
	playerskinKP[2] = R_RegisterSkin( scratch );

#endif

	// show current weapon model (if any)
	if (currentweaponmodel && strlen(currentweaponmodel)) {
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/%s", s_pmi[s_player_model_box.curvalue].directory, currentweaponmodel );
		weaponmodel = R_RegisterModel( scratch );
		if (!weaponmodel) 
		{
			Com_sprintf( scratch, sizeof( scratch ), "players/%s/w_pipe.mdx", s_pmi[s_player_model_box.curvalue].directory );
			weaponmodel = R_RegisterModel( scratch );
		}
	}
	else
	{
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/w_pipe.mdx", s_pmi[s_player_model_box.curvalue].directory );
		weaponmodel = R_RegisterModel( scratch );
	}

	Menu_AddItem( &s_player_config_menu, &s_player_name_field );
	Menu_AddItem( &s_player_config_menu, &s_player_model_title );
	Menu_AddItem( &s_player_config_menu, &s_player_model_box );

#if KINGPIN
	if (s_player_skin_boxKP[0].itemnames){
		Menu_AddItem(&s_player_config_menu, &s_player_skin_titleKP[0]);
		Menu_AddItem(&s_player_config_menu, &s_player_skin_boxKP[0]);
	}
	if (s_player_skin_boxKP[1].itemnames){
		Menu_AddItem(&s_player_config_menu, &s_player_skin_titleKP[1]);
		Menu_AddItem( &s_player_config_menu, &s_player_skin_boxKP[1] );
	}
	if (s_player_skin_boxKP[2].itemnames){
		Menu_AddItem(&s_player_config_menu, &s_player_skin_titleKP[2]);
		Menu_AddItem( &s_player_config_menu, &s_player_skin_boxKP[2] );
	}

#else
	if ( s_player_skin_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_skin_title );
		Menu_AddItem( &s_player_config_menu, &s_player_skin_box );
	}
#endif

	Menu_AddItem( &s_player_config_menu, &s_player_hand_title );
	Menu_AddItem( &s_player_config_menu, &s_player_handedness_box );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );
	Menu_AddItem( &s_player_config_menu, &s_player_back_action );

	return true;
}


qboolean PlayerConfig_CheckIncerement (int dir, float x, float y, float w, float h)
{
	float min[2], max[2], x1, y1, w1, h1;
	char *sound = NULL;

	x1 = x;	y1 = y;	w1 = w;	h1 = h;
	SCR_AdjustFrom640 (&x1, &y1, &w1, &h1, ALIGN_CENTER);
	min[0] = x1;	max[0] = x1 + w1;
	min[1] = y1;	max[1] = y1 + h1;

	if (cursor.x>=min[0] && cursor.x<=max[0] &&
		cursor.y>=min[1] && cursor.y<=max[1] &&
		!cursor.buttonused[MOUSEBUTTON1] &&
		cursor.buttonclicks[MOUSEBUTTON1]==1)
	{

#if !KINGPIN
		if (dir) // dir==1 is left
		{
			if (s_player_skin_box.curvalue>0)
				s_player_skin_box.curvalue--;
		}
		else
		{
			if (s_player_skin_box.curvalue<s_pmi[s_player_model_box.curvalue].nskins)
				s_player_skin_box.curvalue++;
		}
		sound = menu_move_sound;
		cursor.buttonused[MOUSEBUTTON1] = true;
		cursor.buttonclicks[MOUSEBUTTON1] = 0;

		if ( sound )
			S_StartLocalSound( sound );
		SkinCallback (NULL);

#else
		if (dir) // dir==1 is left
		{
			if (s_player_skin_boxKP[0].curvalue>0)
				s_player_skin_boxKP[0].curvalue--;
		}
		else
		{
			if (s_player_skin_boxKP[0].curvalue<s_pmi[s_player_model_box.curvalue].nskinsKP[0])
				s_player_skin_boxKP[0].curvalue++;
		}

		sound = menu_move_sound;
		cursor.buttonused[MOUSEBUTTON1] = true;
		cursor.buttonclicks[MOUSEBUTTON1] = 0;

		if ( sound )
			S_StartLocalSound( sound );
		SkinCallbackKP_Head (NULL);
#endif

		return true;
	}
	return false;
}

#if !KINGPIN
void PlayerConfig_MouseClick (void)
{
	float	icon_x = SCREEN_WIDTH*0.5 - 5, //width - 325
			icon_y = SCREEN_HEIGHT - 108,
			icon_offset = 0;
	int		i, count;
	char	*sound = NULL;
	buttonmenuobject_t buttons[NUM_SKINBOX_ITEMS];

	for (i=0; i<NUM_SKINBOX_ITEMS; i++)
		buttons[i].index =- 1;

	if (s_pmi[s_player_model_box.curvalue].nskins < NUM_SKINBOX_ITEMS || s_player_skin_box.curvalue < 4)
		i=0;
	else if (s_player_skin_box.curvalue > s_pmi[s_player_model_box.curvalue].nskins-4)
		i=s_pmi[s_player_model_box.curvalue].nskins-NUM_SKINBOX_ITEMS;
	else
		i=s_player_skin_box.curvalue-3;

	if (i > 0)
		if (PlayerConfig_CheckIncerement (1, icon_x-39, icon_y, 32, 32))
			return;

	for (count=0; count<NUM_SKINBOX_ITEMS; i++,count++)
	{
		if (i<0 || i>=s_pmi[s_player_model_box.curvalue].nskins)
			continue;

		UI_AddButton (&buttons[count], i, icon_x+icon_offset, icon_y, 32, 32);
		icon_offset += 34;
	}

	icon_offset = NUM_SKINBOX_ITEMS*34;
	if (s_pmi[s_player_model_box.curvalue].nskins-i > 0)
		if (PlayerConfig_CheckIncerement (0, icon_x+icon_offset+5, icon_y, 32, 32))
			return;

	for (i=0; i<NUM_SKINBOX_ITEMS; i++)
	{
		if (buttons[i].index == -1)
			continue;

		if (cursor.x>=buttons[i].min[0] && cursor.x<=buttons[i].max[0] &&
			cursor.y>=buttons[i].min[1] && cursor.y<=buttons[i].max[1])
		{
			if (!cursor.buttonused[MOUSEBUTTON1] && cursor.buttonclicks[MOUSEBUTTON1]==1)
			{
				s_player_skin_box.curvalue = buttons[i].index;

				sound = menu_move_sound;
				cursor.buttonused[MOUSEBUTTON1] = true;
				cursor.buttonclicks[MOUSEBUTTON1] = 0;

				if (sound)
					S_StartLocalSound (sound);
				SkinCallback (NULL);

				return;
			}
			break;
		}
	}
}
#else
void PlayerConfig_MouseClick (void)
{
	float	icon_x = SCREEN_WIDTH*0.5 - 5, //width - 325
			icon_y = SCREEN_HEIGHT - 108,
			icon_offset = 0;
	int		i, count;
	char	*sound = NULL;
	buttonmenuobject_t buttons[NUM_SKINBOX_ITEMS];

	for (i=0; i<NUM_SKINBOX_ITEMS; i++)
		buttons[i].index =- 1;

	if (s_pmi[s_player_model_box.curvalue].nskinsKP[0] < NUM_SKINBOX_ITEMS || s_player_skin_boxKP[0].curvalue < 4)
		i=0;
	else if (s_player_skin_boxKP[0].curvalue > s_pmi[s_player_model_box.curvalue].nskinsKP[0]-4)
		i=s_pmi[s_player_model_box.curvalue].nskinsKP[0]-NUM_SKINBOX_ITEMS;
	else
		i=s_player_skin_boxKP[0].curvalue-3;

	if (i > 0)
		if (PlayerConfig_CheckIncerement (1, icon_x-39, icon_y, 32, 32))
			return;

	for (count=0; count<NUM_SKINBOX_ITEMS; i++,count++)
	{
		if (i<0 || i>=s_pmi[s_player_model_box.curvalue].nskinsKP[0])
			continue;

		UI_AddButton (&buttons[count], i, icon_x+icon_offset, icon_y, 32, 32);
		icon_offset += 34;
	}

	icon_offset = NUM_SKINBOX_ITEMS*34;
	if (s_pmi[s_player_model_box.curvalue].nskinsKP[0]-i > 0)
		if (PlayerConfig_CheckIncerement (0, icon_x+icon_offset+5, icon_y, 32, 32))
			return;

	for (i=0; i<NUM_SKINBOX_ITEMS; i++)
	{
		if (buttons[i].index == -1)
			continue;

		if (cursor.x>=buttons[i].min[0] && cursor.x<=buttons[i].max[0] &&
			cursor.y>=buttons[i].min[1] && cursor.y<=buttons[i].max[1])
		{
			if (!cursor.buttonused[MOUSEBUTTON1] && cursor.buttonclicks[MOUSEBUTTON1]==1)
			{
				s_player_skin_boxKP[0].curvalue = buttons[i].index;

				sound = menu_move_sound;
				cursor.buttonused[MOUSEBUTTON1] = true;
				cursor.buttonclicks[MOUSEBUTTON1] = 0;

				if (sound)
					S_StartLocalSound (sound);
				SkinCallbackKP_Head (NULL);

				return;
			}
			break;
		}
	}
}
#endif

void PlayerConfig_DrawSkinSelection (void)
{
	char	scratch[MAX_QPATH];
	float	icon_x = SCREEN_WIDTH*0.5 - 5; //width - 325
	float	icon_y = SCREEN_HEIGHT - 108;
	float	icon_offset = 0;
	float	x, y, w, h;
	int		i, count, color[3];

#if KINGPIN //hypov8 not needed
	//return;
#endif

	TextColor((int)Cvar_VariableValue("alt_text_color"), &color[0], &color[1], &color[2]);
#if !KINGPIN
	if (s_pmi[s_player_model_box.curvalue].nskins<NUM_SKINBOX_ITEMS || s_player_skin_box.curvalue<4)
		i=0;
	else if (s_player_skin_box.curvalue > s_pmi[s_player_model_box.curvalue].nskins-4)
		i=s_pmi[s_player_model_box.curvalue].nskins-NUM_SKINBOX_ITEMS;
	else
		i=s_player_skin_box.curvalue-3;
#else
	if (s_pmi[s_player_model_box.curvalue].nskinsKP[0]<NUM_SKINBOX_ITEMS || s_player_skin_boxKP[0].curvalue<4)
		i=0;
	else if (s_player_skin_boxKP[0].curvalue > s_pmi[s_player_model_box.curvalue].nskinsKP[0]-4)
		i=s_pmi[s_player_model_box.curvalue].nskinsKP[0]-NUM_SKINBOX_ITEMS;
	else
		i=s_player_skin_boxKP[0].curvalue-3;
#endif


	// left arrow
	if (i>0)
		Com_sprintf (scratch, sizeof(scratch), "/gfx/ui/arrows/arrow_left.pcx");
	else
		Com_sprintf (scratch, sizeof(scratch), "/gfx/ui/arrows/arrow_left_d.pcx");
	SCR_DrawPic (icon_x-39, icon_y+2, 32, 32,  ALIGN_CENTER, scratch, 1.0);

	// background
	SCR_DrawFill (icon_x-3, icon_y-3, NUM_SKINBOX_ITEMS*34+4, 38, ALIGN_CENTER, 0,0,0,255);
	if (R_DrawFindPic("/gfx/ui/listbox_background.pcx")) {
		x = icon_x-2;	y = icon_y-2;	w = NUM_SKINBOX_ITEMS*34+2;	h = 36;
		SCR_AdjustFrom640 (&x, &y, &w, &h, ALIGN_CENTER);
		R_DrawTileClear ((int)x, (int)y, (int)w, (int)h, "/gfx/ui/listbox_background.pcx");
	}
	else
		SCR_DrawFill (icon_x-2, icon_y-2, NUM_SKINBOX_ITEMS*34+2, 36, ALIGN_CENTER, 60,60,60,255);
		
	for (count=0; count<NUM_SKINBOX_ITEMS; i++,count++)
	{
#if ! KINGPIN
		if (i<0 || i>=s_pmi[s_player_model_box.curvalue].nskins)
			continue;

		Com_sprintf (scratch, sizeof(scratch), "/players/%s/%s_i.pcx", 
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[i] );

		if (i==s_player_skin_box.curvalue)
#else
		if (i<0 || i>=s_pmi[s_player_model_box.curvalue].nskinsKP[0])
			continue;

		Com_sprintf (scratch, sizeof(scratch), "/players/%s/%s_i.pcx", 
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[0][i] );

		if (i==s_player_skin_boxKP[0].curvalue)
#endif
			SCR_DrawFill (icon_x + icon_offset-1, icon_y-1, 34, 34, ALIGN_CENTER, color[0],color[1],color[2],255);
		SCR_DrawPic (icon_x + icon_offset, icon_y, 32, 32,  ALIGN_CENTER, scratch, 1.0);
		icon_offset += 34;
	}

	// right arrow
	icon_offset = NUM_SKINBOX_ITEMS*34;
#if !KINGPIN
	if (s_pmi[s_player_model_box.curvalue].nskins-i>0)
#else
	if (s_pmi[s_player_model_box.curvalue].nskinsKP[0]-i>0)
#endif
		Com_sprintf (scratch, sizeof(scratch), "/gfx/ui/arrows/arrow_right.pcx");
	else
		Com_sprintf (scratch, sizeof(scratch), "/gfx/ui/arrows/arrow_right_d.pcx"); 
	SCR_DrawPic (icon_x+icon_offset+5, icon_y+2, 32, 32,  ALIGN_CENTER, scratch, 1.0);
}


void PlayerConfig_MenuDraw (void)
{
	refdef_t	refdef;
	float		rx, ry, rw, rh;
#if KINGPIN
	//M_Main_Draw(); //remove kp menu
#else
	Menu_DrawBanner ("m_banner_plauer_setup"); // typo for image name is id's fault
#endif


	memset(&refdef, 0, sizeof(refdef));

	rx = 0;							ry = 0;
	rw = SCREEN_WIDTH;				rh = SCREEN_HEIGHT;
	SCR_AdjustFrom640 (&rx, &ry, &rw, &rh, ALIGN_CENTER);
	refdef.x = rx;		refdef.y = ry;
	refdef.width = rw;	refdef.height = rh;
	refdef.fov_x = 50;
	refdef.fov_y = CalcFov (refdef.fov_x, refdef.width, refdef.height);
	refdef.time = cls.realtime*0.001;
 
#if ! KINGPIN
	if ( s_pmi[s_player_model_box.curvalue].skindisplaynames )
#else
	if ( s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[0] 
		&& s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[1]
		&& s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[2])
#endif
	{
		int			yaw;
		int			maxframe = 29;
		vec3_t		modelOrg;
		// Psychopspaz's support for showing weapon model
#if ! KINGPIN
		entity_t	entity[2], *ent;
#else
		entity_t	entity[4], *ent;
#endif

		refdef.num_entities = 0;
		refdef.entities = entity;
#if !KINGPIN
		yaw = anglemod(cl.time/10);
		VectorSet(modelOrg, 150, (hand->value == 1) ? 25 : -25, 0); // was 80, 0, 0
#else
		yaw = 160; //hypov8 disable rotate?
		VectorSet(modelOrg, 150, -35 , -10); // was 80, 0, 0 //hypov8 hand
#endif


		// Setup player model
		ent = &entity[0];
		memset (&entity[0], 0, sizeof(entity[0]));

		// moved registration code to init and change only
#if ! KINGPIN
		ent->model = playermodel;
		ent->skin = playerskin;
#else
		ent->model = playermodelKP[0];
		ent->skin = playerskinKP[0];
#endif
		ent->flags = RF_FULLBRIGHT|RF_NOSHADOW|RF_DEPTHHACK;
#if !KINGPIN
		if (hand->value == 1)
			ent->flags |= RF_MIRRORMODEL;
#endif

		ent->origin[0] = modelOrg[0];
		ent->origin[1] = modelOrg[1];
		ent->origin[2] = modelOrg[2];

		VectorCopy( ent->origin, ent->oldorigin );
		ent->frame = 0;
		ent->oldframe = 0;
		ent->backlerp = 0.0;
		ent->angles[1] = yaw;
		//if ( ++yaw > 360 )
		//	yaw -= 360;
#if !KINGPIN		
		if (hand->value == 1)
			ent->angles[1] = 360 - ent->angles[1];
#endif
		refdef.num_entities++;
#if KINGPIN
		// Setup player model (body)
		ent = &entity[1];
		memcpy(&entity[1], &entity[0], sizeof(entity[0]));
		ent->model = playermodelKP[1];
		ent->skin = playerskinKP[1];
		refdef.num_entities++;

		// Setup player model (body)
		ent = &entity[2];
		memcpy(&entity[2], &entity[0], sizeof(entity[0]));
		ent->model = playermodelKP[2];
		ent->skin = playerskinKP[2];
		refdef.num_entities++;

		// Setup weapon model
		ent = &entity[3];
		memcpy(&entity[3], &entity[0], sizeof(entity[0]));
		ent->model = weaponmodel;
		if (ent->model)
		{
			ent->skinnum = 0;
			ent->skin = 0;
			refdef.num_entities++;
		}

#else
		ent = &entity[1];
		memset (&entity[1], 0, sizeof(entity[1]));

		// moved registration code to init and change only
		ent->model = weaponmodel;
		if (ent->model)
		{
			ent->skinnum = 0;

			ent->flags = RF_FULLBRIGHT|RF_NOSHADOW|RF_DEPTHHACK;
			if (hand->value == 1)
				ent->flags |= RF_MIRRORMODEL;

			ent->origin[0] = modelOrg[0];
			ent->origin[1] = modelOrg[1];
			ent->origin[2] = modelOrg[2];

			VectorCopy( ent->origin, ent->oldorigin );
			ent->frame = 0;
			ent->oldframe = 0;
			ent->backlerp = 0.0;
			ent->angles[1] = yaw;
			
			if (hand->value == 1)
				ent->angles[1] = 360 - ent->angles[1];

			refdef.num_entities++;
		}
#endif

		refdef.areabits = 0;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		Menu_Draw( &s_player_config_menu );

		// skin selection preview
#if !KINGPIN
		PlayerConfig_DrawSkinSelection ();
#endif
		R_RenderFrame( &refdef );
	}
}

#if !KINGPIN
void PConfigAccept (void)
{
	int i;
	char scratch[1024];

	Cvar_Set( "name", s_player_name_field.buffer );

	Com_sprintf( scratch, sizeof( scratch ), "%s/%s", 
		s_pmi[s_player_model_box.curvalue].directory, 
		s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );

	Cvar_Set( "skin", scratch );

	for ( i = 0; i < s_numplayermodels; i++ )
	{
		int j;

		for ( j = 0; j < s_pmi[i].nskins; j++ )
		{
			if ( s_pmi[i].skindisplaynames[j] )
				free( s_pmi[i].skindisplaynames[j] );
			s_pmi[i].skindisplaynames[j] = 0;
		}
		free( s_pmi[i].skindisplaynames );
		s_pmi[i].skindisplaynames = 0;
		s_pmi[i].nskins = 0;
	}
}
#else
void PConfigAccept (void)
{
	int i;
	char scratch[1024];

	Cvar_Set( "name", s_player_name_field.buffer );
#if !KINGPIN
	Com_sprintf( scratch, sizeof( scratch ), "%s/%s", 
		s_pmi[s_player_model_box.curvalue].directory, 
		s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[0][s_player_skin_boxKP[0].curvalue] );
#else
	Com_sprintf( scratch, sizeof( scratch ), "%s/%s %s %s", 
		s_pmi[s_player_model_box.curvalue].directory, 
		s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[0][s_player_skin_boxKP[0].curvalue]+5, //head_ xxx
		s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[1][s_player_skin_boxKP[1].curvalue]+5,	//body_ xxx	
		s_pmi[s_player_model_box.curvalue].skindisplaynamesKP[2][s_player_skin_boxKP[2].curvalue]+5); //legs_ xxx

#endif
	Cvar_Set( "skin", scratch );

#if KINGPIN
	CL_SendCmd (); //hypov8 add: does this get sent anywhere else??
#endif

	for ( i = 0; i < s_numplayermodels; i++ )
	{
		int j;

		for ( j = 0; j < s_pmi[i].nskinsKP[0]; j++ )
		{
			if ( s_pmi[i].skindisplaynamesKP[0][j] )
				free( s_pmi[i].skindisplaynamesKP[0][j] );
			s_pmi[i].skindisplaynamesKP[0][j] = 0;
		}
		free( s_pmi[i].skindisplaynamesKP[0] );
		s_pmi[i].skindisplaynamesKP[0] = 0;
		s_pmi[i].nskinsKP[0] = 0;
	}
}
#endif
const char *PlayerConfig_MenuKey (int key)
{
	if ( key == K_ESCAPE )
		PConfigAccept();

	return Default_MenuKey( &s_player_config_menu, key );
}


void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar( &s_multiplayer_menu, "No valid player models found" );
		return;
	}
	Menu_SetStatusBar( &s_multiplayer_menu, NULL );
	UI_PushMenu( PlayerConfig_MenuDraw, PlayerConfig_MenuKey );
}
