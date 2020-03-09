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
// sv_main.c -- server main program

#include "server.h"

/*
=============================================================================

Com_Printf redirection

=============================================================================
*/

char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect (int sv_redirected, char *outputbuf)
{
	if (sv_redirected == RD_PACKET)
	{
		Netchan_OutOfBandPrint (NS_SERVER, net_from, "print\n%s", outputbuf);
	}
	else if (sv_redirected == RD_CLIENT)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_print);
		MSG_WriteByte (&sv_client->netchan.message, PRINT_HIGH);
		MSG_WriteString (&sv_client->netchan.message, outputbuf);
	}
}


/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/


/*
=================
SV_ClientPrintf

Sends text across to be displayed if the level passes
=================
*/
void SV_ClientPrintf (client_t *cl, int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	if (level < cl->messagelevel)
		return;
	
	va_start (argptr, fmt);
//	vsprintf (string, fmt, argptr);
	Q_vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);
	
	MSG_WriteByte (&cl->netchan.message, svc_print);
	MSG_WriteByte (&cl->netchan.message, level);
	MSG_WriteString (&cl->netchan.message, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[2048];
	client_t	*cl;
	int			i;

	va_start (argptr, fmt);
//	vsprintf (string, fmt, argptr);
	Q_vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);
	
	// echo to console
	if (dedicated->value)
	{
		char	copy[1024];
		int		i;
		
		// mask off high bits
		for (i=0 ; i<1023 && string[i] ; i++)
			copy[i] = string[i]&127;
		copy[i] = 0;
		Com_Printf ("%s", copy);
	}

	for (i=0, cl = svs.clients ; i<(int)maxclients->value; i++, cl++)
	{
		if (level < cl->messagelevel)
			continue;
		if (cl->state != cs_spawned)
			continue;
		MSG_WriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, level);
		MSG_WriteString (&cl->netchan.message, string);
	}
}



#if KINGPIN_HYPO
static messagelist_t * SV_DeleteMessage (client_t *cl, messagelist_t *message, messagelist_t *last)
{
	//only free if it was malloced
	if (message->cursize > MSG_MAX_SIZE_BEFORE_MALLOC)
		free (message->data);

	last->next = message->next;
	
	//end of the list
	if (message == cl->msgListEnd)
		cl->msgListEnd = last;

#ifdef _DEBUG
	memset (message, 0xCC, sizeof(*message));
#endif

	return last;
}

void SV_ClearMessageList (client_t *client)
{
	messagelist_t *message, *last;

	message = client->msgListStart;

	for (;;)
	{
		last = message;
		message = message->next;

		if (!message)
			break;

		message = SV_DeleteMessage (client, message, last);
	}
}

static void SV_AddMessageSingle (client_t *cl, qboolean reliable)
{
	int				index;
	messagelist_t	*next;
	char			*data; // MH: added

	if (cl->state <= cs_zombie)
	{
		Com_Printf ("WARNING: SV_AddMessage to zombie/free client %d.\n", LOG_SERVER|LOG_WARNING, (int)(cl - svs.clients));
		return;
	}

	if (cl->state == cs_connected && sv_force_reconnect->string[0] && !cl->reconnect_done && !NET_IsLANAddress (&cl->netchan.remote_address) && !cl->reconnect_var[0])
	{
		Com_Printf ("Dropped a %sreliable message to connecting client %d.\n", LOG_SERVER|LOG_NOTICE, reliable ? "" : "un", (int)(cl - svs.clients));
		return;
	}

	//an overflown client
	if (!cl->messageListData || ((cl->notes & NOTE_OVERFLOWED) && !(cl->notes & NOTE_OVERFLOW_DONE)))
		return;

	//doesn't want unreliables (irc bots/etc)
	if (cl->nodata && !reliable)
		return;

	data = MSG_GetData();
	if (data[0] == svc_layout)
	{
		// MH: check layout messages aren't bigger than the client can handle
		if (MSG_GetLength() > 1025)
		{
			if (sv_gamedebug->intvalue)
			{
				Com_Printf ("GAME WARNING: Sending a too big (%d bytes) svc_layout message to %s.\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, MSG_GetLength(), cl->name);
				if (sv_gamedebug->intvalue >= 2)
					Sys_DebugBreak ();
			}
			// truncate it to avoid wasting bandwidth
			data[1024] = 0;
			MSG_GetRawMsg()->cursize = 1025;
		}

		// MH: drop duplicate layout messages
		if (!strcmp(data + 1, cl->layout) && (!reliable || !cl->layout_unreliable))
			return;
		strncpy(cl->layout, data + 1, sizeof(cl->layout) - 1);
		if (reliable)
			cl->layout_unreliable = 0;
	}

	//get next message position
	index = (int)((cl->msgListEnd - cl->msgListStart)) + 1;

	//have they overflown?
	if (index >= MAX_MESSAGES_PER_LIST-1)
	{
		Com_Printf ("WARNING: Index overflow (%d) for %s.\n", LOG_SERVER|LOG_WARNING, index, cl->name);

		//clear the buffer for overflow print and malloc cleanup
		SV_ClearMessageList (cl);

		//drop them
		cl->notes |= NOTE_OVERFLOWED;
		return;
	}

	//set up links
	next = &cl->messageListData[index];
	cl->msgListEnd->next = next;

	cl->msgListEnd = next;
	next->next = NULL;

	SV_MSGListIntegrityCheck (cl);

	//write message to this buffer
	MSG_EndWrite (next);

	SV_MSGListIntegrityCheck (cl);

	//check its sane, should never happen...
	if (next->cursize >= cl->netchan.message.buffsize)
	{
		//uh oh...
		Com_Printf ("ALERT: SV_AddMessageSingle: Message size %d to %s is larger than MAX_USABLEMSG (%d)!!\n", LOG_SERVER|LOG_WARNING, next->cursize, cl->name, cl->netchan.message.buffsize);

		//clear the buffer for overflow print and malloc cleanup
		SV_ClearMessageList (cl);

		//drop them
		cl->notes |= NOTE_OVERFLOWED;
		return;
	}

	//set reliable flag
	next->reliable = reliable;

	// MH: drop any other pending layout messages
	if (next->data[0] == svc_layout)
	{
		messagelist_t	*message, *last;
		message = cl->msgListStart;
		for (;;)
		{
			last = message;
			message = message->next;

			if (message == next)
				break;

			if (message->data[0] == svc_layout)
			{
				if (message->reliable)
					next->reliable = message->reliable;
				message = SV_DeleteMessage (cl, message, last);

				SV_MSGListIntegrityCheck (cl);
			}
		}
	}
}

/*
=================
Overflow Checking
=================
Note that we can't check for overflows in SV_AddMessage* functions as if the client
overflows, we will want to print '%s overflowed / %s disconnected' or such, which in
turn will call SV_AddMessage* again and possibly cause either infinite looping or loss
of whatever data was in the message buffer from the original call (eg, multicasting)
*/
static void SV_CheckForOverflowSingle (client_t *cl)
{
	if (!(cl->notes & NOTE_OVERFLOWED) || (cl->notes & NOTE_OVERFLOW_DONE))
		return;

	cl->notes |= NOTE_OVERFLOW_DONE;

	//drop message
	if (cl->name[0])
	{
		if (cl->state == cs_spawned)
		{
			SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", cl->name);
		}
		else
		{
			//let them know what happened
			SV_ClientPrintf (cl, PRINT_HIGH, "%s overflowed\n", cl->name);
			Com_Printf ("%s overflowed while connecting!\n", LOG_SERVER|LOG_WARNING, cl->name);
		}
	}

	Com_Printf ("Dropping %s, overflowed.\n", LOG_SERVER, cl->name);
	SV_DropClient (cl, true);
}

static void SV_CheckForOverflow ()
{
	int				i;
	client_t		*cl;

	for (i=0,cl=svs.clients ; i<maxclients->intvalue ; i++,cl++)
	{
		if (cl->state <= cs_zombie)
			continue;

		if (!(cl->notes & NOTE_OVERFLOWED) || (cl->notes & NOTE_OVERFLOW_DONE))
			continue;

		SV_CheckForOverflowSingle (cl);
	}
}

void SV_AddMessage (client_t *cl, qboolean reliable)
{
	SV_AddMessageSingle (cl, reliable);
	MSG_FreeData ();
	//SV_CheckForOverflowSingle (cl);
}
#endif
/*
=================
SV_BroadcastCommand

Sends text to all active clients
=================
*/
void SV_BroadcastCommand (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	if (!sv.state)
		return;
	va_start (argptr, fmt);
//	vsprintf (string, fmt, argptr);
	Q_vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&sv.multicast, svc_stufftext);
	MSG_WriteString (&sv.multicast, string);
	SV_Multicast (NULL, MULTICAST_ALL_R);
}


/*
=================
SV_Multicast

Sends the contents of sv.multicast to a subset of the clients,
then clears sv.multicast.

MULTICAST_ALL	same as broadcast (origin can be NULL)
MULTICAST_PVS	send to clients potentially visible from org
MULTICAST_PHS	send to clients potentially hearable from org
=================
*/
void SV_Multicast (vec3_t origin, multicast_t to)
{
	client_t	*client;
	byte		*mask;
	int			leafnum, cluster;
	int			j;
	qboolean	reliable;
	int			area1, area2;

	reliable = false;

	if (to != MULTICAST_ALL_R && to != MULTICAST_ALL)
	{
		leafnum = CM_PointLeafnum (origin);
		area1 = CM_LeafArea (leafnum);
	}
	else
	{
		leafnum = 0;	// just to avoid compiler warnings
		area1 = 0;
	}

	// if doing a serverrecord, store everything
	if (svs.demofile)
		SZ_Write (&svs.demo_multicast, sv.multicast.data, sv.multicast.cursize);
	
	switch (to)
	{
	case MULTICAST_ALL_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_ALL:
		leafnum = 0;
		mask = NULL;
		break;

	case MULTICAST_PHS_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_PHS:
		leafnum = CM_PointLeafnum (origin);
		cluster = CM_LeafCluster (leafnum);
		mask = CM_ClusterPHS (cluster);
		break;

	case MULTICAST_PVS_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_PVS:
		leafnum = CM_PointLeafnum (origin);
		cluster = CM_LeafCluster (leafnum);
		mask = CM_ClusterPVS (cluster);
		break;

	default:
		mask = NULL;
		Com_Error (ERR_FATAL, "SV_Multicast: bad to:%i", to);
	}

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < (int)maxclients->value; j++, client++)
	{
		if (client->state == cs_free || client->state == cs_zombie)
			continue;
		if (client->state != cs_spawned &&
			(!reliable
//kpded2
#if !KINGPIN //hypov8 todo: MSG_GetType
			||(
			//r1: don't send these types to connecting clients, they are pointless even if reliable.
			MSG_GetType() == svc_muzzleflash ||
			MSG_GetType() == svc_muzzleflash2 ||
			MSG_GetType() == svc_muzzleflash3 ||
			MSG_GetType() == svc_temp_entity ||
			MSG_GetType() == svc_print ||
			MSG_GetType() == svc_sound ||
			MSG_GetType() == svc_centerprint
			)
//end
#endif
			
			)
			)
			continue;

#if !KINGPIN //hypov8 todo: MSG_GetType
		// MH: don't send image configstring updates while downloadables are in their place
		if (client->state != cs_spawned && MSG_GetType() == svc_configstring && sv.dlconfigstrings[0][0])
		{
			byte *data = MSG_GetData();
			short cs = *(short*)(data + 1);
			if (cs > OLD_CS_IMAGES && cs < OLD_CS_IMAGES + OLD_MAX_IMAGES && sv.dlconfigstrings[cs - OLD_CS_IMAGES - 1][0])
				continue;
		}

		// MH: don't send temp entities when entities are disabled by the game DLL
		if ((svs.game_features & GMF_CLIENTNOENTS) && client->edict->client->noents)
		{
			if (MSG_GetType() == svc_muzzleflash ||
				MSG_GetType() == svc_muzzleflash2 ||
				MSG_GetType() == svc_muzzleflash3 ||
				MSG_GetType() == svc_temp_entity)
				continue;
		}
#endif

		if (mask)
		{
			leafnum = CM_PointLeafnum (client->edict->s.origin);
			cluster = CM_LeafCluster (leafnum);
			area2 = CM_LeafArea (leafnum);
			if (!CM_AreasConnected (area1, area2))
				continue;
			if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
				continue;
		}

		if (reliable)
			SZ_Write (&client->netchan.message, sv.multicast.data, sv.multicast.cursize);
		else
			SZ_Write (&client->datagram, sv.multicast.data, sv.multicast.cursize);
	}

	SZ_Clear (&sv.multicast);
}


/*  
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

If cahnnel & 8, the sound will be sent to everyone, not just
things in the PHS.

FIXME: if entity isn't in PHS, they must be forced to be sent or
have the origin explicitly sent.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

Timeofs can range from 0.0 to 0.1 to cause sounds to be started
later in the frame than they normally would.

If origin is NULL, the origin is determined from the entity origin
or the midpoint of the entity box for bmodels.
==================
*/  
void SV_StartSound (vec3_t origin, edict_t *entity, int channel,
					int soundindex, float volume,
					float attenuation, float timeofs)
{       
	int			sendchan;
    int			flags;
    int			i;
	int			ent;
	vec3_t		origin_v;
	qboolean	use_phs;
#if KINGPIN //hypov8 add: kpOnly?	
	client_t	*client;
//	sizebuf_t	*to;
	qboolean	force_pos= false;
	qboolean	calc_attn;
#endif
	if (volume < 0 || volume > 1.0)
		Com_Error (ERR_FATAL, "SV_StartSound: volume = %f", volume);

	if (attenuation < 0 || attenuation > 4)
		Com_Error (ERR_FATAL, "SV_StartSound: attenuation = %f", attenuation);

//	if (channel < 0 || channel > 15)
//		Com_Error (ERR_FATAL, "SV_StartSound: channel = %i", channel);

	if (timeofs < 0 || timeofs > 0.255)
		Com_Error (ERR_FATAL, "SV_StartSound: timeofs = %f", timeofs);

	ent = NUM_FOR_EDICT(entity);

	if (channel & 8)	// no PHS flag
	{
		use_phs = false;
		channel &= 7;
	}
#if KINGPIN //hypov8 add: kpOnly?	
	else if (channel & CHAN_PVS) // MH: PVS only flag
		use_phs = 2;
#endif		
	else
		use_phs = true;

#if KINGPIN //hypov8 add: kpOnly?
	if (channel & CHAN_SERVER_ATTN_CALC)
	{
		calc_attn = true;
		channel &= ~CHAN_SERVER_ATTN_CALC;
	}
	else
		calc_attn = false;
#endif	
	sendchan = (ent<<3) | (channel&7);

	flags = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= SND_ATTENUATION;
		
#if KINGPIN //hypov8 add: kpOnly?
	if (attenuation == ATTN_NONE)
	{
		use_phs = false;
	}
	else
#endif	
	{
	// the client doesn't know that bmodels have weird origins
	// the origin can also be explicitly set
	if ( (entity->svflags & SVF_NOCLIENT)
		|| (entity->solid == SOLID_BSP) 
		|| origin )
		flags |= SND_POS;

#if !KINGPIN //hypov8 add: kpOnly?
		// use the entity origin unless it is a bmodel or explicitly specified
		if (!origin)
		{
			origin = origin_v;
			if (entity->solid == SOLID_BSP)
			{
				origin_v[0] = entity->s.origin[0]+0.5f*(entity->mins[0]+entity->maxs[0]);
				origin_v[1] = entity->s.origin[1]+0.5f*(entity->mins[1]+entity->maxs[1]);
				origin_v[2] = entity->s.origin[2]+0.5f*(entity->mins[2]+entity->maxs[2]);
			}
			else
			{
				FastVectorCopy (entity->s.origin, origin_v);
			}
		}
#endif		
	}

	// always send the entity number for channel overrides
	flags |= SND_ENT;

	if (timeofs)
		flags |= SND_OFFSET;

#if KINGPIN //hypov8 add: multicast...
	// use the entity origin unless it is a bmodel or explicitly specified
	if (!origin)
	{
		origin = origin_v;
		if (entity->solid == SOLID_BSP)
		{
			for (i=0; i<3; i++)
				origin_v[i] = entity->s.origin[i]+0.5*(entity->mins[i]+entity->maxs[i]);
		}
		else
		{
			VectorCopy (entity->s.origin, origin_v);
		}
	}

	MSG_WriteByte (&sv.multicast, svc_sound);
	MSG_WriteByte (&sv.multicast, flags);

	//Knightmare- 12/23/2001- changed to short
	MSG_WriteShort (&sv.multicast, soundindex);
		
	if (flags & SND_VOLUME)
		MSG_WriteByte (&sv.multicast, volume*255);
	if (flags & SND_ATTENUATION)
		MSG_WriteByte (&sv.multicast, attenuation*64);
	if (flags & SND_OFFSET)
		MSG_WriteByte (&sv.multicast, timeofs*1000);

	if (flags & SND_ENT)
		MSG_WriteShort (&sv.multicast, sendchan);

	if (flags & SND_POS)
		MSG_WritePos (&sv.multicast, origin);

	// if the sound doesn't attenuate,send it to everyone
	// (global radio chatter, voiceovers, etc)
	if (attenuation == ATTN_NONE)
		use_phs = false;

	if (channel & CHAN_RELIABLE)
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS_R);
		else
			SV_Multicast (origin, MULTICAST_ALL_R);
	}
	else
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS);
		else
			SV_Multicast (origin, MULTICAST_ALL);
	}
#else	
	for (j = 0, client = svs.clients; j < (int)maxclients->value; j++, client++)
	{
		//r1: do we really want to be sending sounds to clients who have no entity state?
		//if (client->state <= cs_zombie || (client->state != cs_spawned && !(channel & CHAN_RELIABLE)))
		if (client->state != cs_spawned)
			continue;

		if (use_phs)
		{
#if !KINGPIN //hypov8 todo: kpded2
			// MH: don't send PHS/PVS sounds when entities are disabled by the game DLL
			if ((svs.game_features & GMF_CLIENTNOENTS) && client->edict->client->noents)
				continue;
#endif

			if (force_pos)
			{
				flags |= SND_POS;
			}
			// MH: send only to clients in PVS
			else if (use_phs == 2)
			{
				if (!PF_inPVS (client->edict->s.origin, origin))
					continue;
			}
			else
			{
				if (!PF_inPHS (client->edict->s.origin, origin))
					continue;

/*				if (!PF_inPVS (client->edict->s.origin, origin)) // MH: disabled
					flags |= SND_POS;
				else
					flags &= ~SND_POS;*/
			}

			//server side attenuation calculations, used on doors/plats to avoid multicasting over entire map
			if (calc_attn)
			{
				float	distance;
				float	distance_multiplier;
				vec3_t	source_vec;

				distance_multiplier = attenuation * 0.001f;

				VectorSubtract (origin, client->edict->s.origin, source_vec);

				distance = VectorNormalize(source_vec);
				distance -= 80.0f;

				if (FLOAT_LT_ZERO(distance))
					distance = 0;

				distance *= distance_multiplier;

				//inaudible, plus 0.5 for lag compensation
				if (distance > 1.5f)
				{
					Com_DPrintf ("Dropping out of range sound %s to %s, distance = %g.\n", sv.configstrings[CS_SOUNDS+soundindex], client->name, distance);
					continue;
				}
			}
		}

#if !KINGPIN //hypov8 todo: kpded2
		// MH: drop curse sounds when parental lock is enabled
		if (client->nocurse)
		{
			const char *path = sv.configstrings[CS_SOUNDS + soundindex];
			if (((!strncmp(path, "actors/male/", 12) || !strncmp(path, "actors/female/", 14)) && strchr(path + 14, '/')) || !strncmp(path, "actors/player/male/profanity/", 29))
				continue;
		}
#endif
		MSG_WriteByte (&sv.multicast, svc_sound);
		//MSG_BeginWriting (svc_sound);
		MSG_WriteByte (&sv.multicast, flags);
		MSG_WriteByte (&sv.multicast, soundindex);
		
		if (flags & SND_VOLUME)
			MSG_WriteByte (&sv.multicast, (int)(volume*255));

		if (flags & SND_ATTENUATION)
		{
			if (attenuation >= 4.0f)
			{
				Com_Printf ("GAME WARNING: Attempt to play sound '%s' with illegal attenuation %f, fixed.\n", LOG_WARNING|LOG_SERVER|LOG_GAMEDEBUG, sv.configstrings[CS_SOUNDS+soundindex], attenuation);
#if !KINGPIN //hypov8 todo: kpded2
				if ((int)sv_gamedebug->value >= 2)
					Sys_DebugBreak ();
#endif
				attenuation = 3.984375f;
			}
			MSG_WriteByte (&sv.multicast, (int)(attenuation*64));
		}

		if (flags & SND_OFFSET)
			MSG_WriteByte (&sv.multicast, (int)(timeofs*1000));

		if (flags & SND_ENT)
			MSG_WriteShort (&sv.multicast, sendchan);

		if (flags & SND_POS)
			MSG_WritePos (&sv.multicast, origin);

		//SV_AddMessage (client, (channel & CHAN_RELIABLE)); //kpded2

	SV_AddMessageSingle (cl, reliable);
	//MSG_FreeData ();

		Q_assert (msgbuff.cursize > 0);
	SZ_Clear (&msgbuff);
#ifdef _DEBUG
	memset (message_buff, 0xcc, sizeof(message_buff));
#endif
	}
	// if the sound doesn't attenuate,send it to everyone
	// (global radio chatter, voiceovers, etc)
	/*if (attenuation == ATTN_NONE)
		use_phs = false;

	if (channel & CHAN_RELIABLE)
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS_R);
		else
			SV_Multicast (origin, MULTICAST_ALL_R);
	}
	else
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS);
		else
			SV_Multicast (origin, MULTICAST_ALL);
	}*/
	#endif
}           


/*
===============================================================================

FRAME UPDATES

===============================================================================
*/



/*
=======================
SV_SendClientDatagram
=======================
*/
qboolean SV_SendClientDatagram (client_t *client)
{
	byte		msg_buf[MAX_MSGLEN];
	sizebuf_t	msg;

	SV_BuildClientFrame (client);

	SZ_Init (&msg, msg_buf, sizeof(msg_buf));

	// Knightmare- limit message size to 2800 for non-local clients in multiplayer
	if ((int)maxclients->value > 1 && client->netchan.remote_address.type != NA_LOOPBACK && sv_limit_msglen->integer != 0)
		msg.maxsize = MAX_MSGLEN_MP;

	msg.allowoverflow = true;

	// send over all the relevant entity_state_t
	// and the player_state_t
	SV_WriteFrameToClient (client, &msg);

	// copy the accumulated multicast datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if (client->datagram.overflowed)
		Com_Printf (S_COLOR_YELLOW"WARNING: datagram overflowed for %s\n", client->name);
	else
		SZ_Write (&msg, client->datagram.data, client->datagram.cursize);
	SZ_Clear (&client->datagram);

	if (msg.overflowed)
	{	// must have room left for the packet header
		Com_Printf (S_COLOR_YELLOW"WARNING: msg overflowed for %s\n", client->name);
		SZ_Clear (&msg);
	}

	// send the datagram
	Netchan_Transmit (&client->netchan, msg.cursize, msg.data);

	// record the size for rate estimation
	client->message_size[sv.framenum % RATE_MESSAGES] = msg.cursize;

	return true;
}


/*
==================
SV_DemoCompleted
==================
*/
void SV_DemoCompleted (void)
{
	if (sv.demofile)
	{
		FS_FCloseFile (sv.demofile);
		sv.demofile = 0; // clear the file handle
	}
	SV_Nextserver ();
}


/*
=======================
SV_RateDrop

Returns true if the client is over its current
bandwidth estimation and should not be sent another packet
=======================
*/
qboolean SV_RateDrop (client_t *c)
{
	int		total;
	int		i;

	// never drop over the loopback
	if (c->netchan.remote_address.type == NA_LOOPBACK)
		return false;

	total = 0;

	for (i = 0 ; i < RATE_MESSAGES ; i++)
	{
		total += c->message_size[i];
	}

	if (total > c->rate)
	{
		c->surpressCount++;
		c->message_size[sv.framenum % RATE_MESSAGES] = 0;
		return true;
	}

	return false;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int			i;
	client_t	*c;
	int			msglen;
	byte		msgbuf[MAX_MSGLEN];
	int			r;

	msglen = 0;

	// read the next demo message if needed
	if (sv.state == ss_demo && sv.demofile)
	{
		if (sv_paused->value)
			msglen = 0;
		else
		{
			// get the next message
			r = FS_FRead (&msglen, 4, 1, sv.demofile);
			if (r != 4)
			{
				SV_DemoCompleted ();
				return;
			}
			msglen = LittleLong (msglen);
			if (msglen == -1)
			{
				SV_DemoCompleted ();
				return;
			}
			if (msglen > MAX_MSGLEN)
				Com_Error (ERR_DROP, "SV_SendClientMessages: msglen > MAX_MSGLEN");
			r = FS_FRead (msgbuf, msglen, 1, sv.demofile);
			if (r != msglen)
			{
				SV_DemoCompleted ();
				return;
			}
		}
	}

	// send a message to each connected client
	for (i=0, c = svs.clients ; i<(int)maxclients->value; i++, c++)
	{
		if (!c->state)
			continue;
		// if the reliable message overflowed,
		// drop the client
		if (c->netchan.message.overflowed)
		{
			SZ_Clear (&c->netchan.message);
			SZ_Clear (&c->datagram);
			SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", c->name);
			SV_DropClient (c);
		}

		if (sv.state == ss_cinematic 
			|| sv.state == ss_demo 
			|| sv.state == ss_pic
			)
			Netchan_Transmit (&c->netchan, msglen, msgbuf);
		else if (c->state == cs_spawned)
		{
			// don't overrun bandwidth
			if (SV_RateDrop (c))
				continue;

			SV_SendClientDatagram (c);
		}
		else
		{
	// just update reliable	if needed
			if (c->netchan.message.cursize	|| curtime - c->netchan.last_sent > 1000 )
				Netchan_Transmit (&c->netchan, 0, NULL);
		}
	}
}

