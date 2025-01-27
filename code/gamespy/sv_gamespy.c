/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../server/server.h"
#include "sv_gqueryreporting.h"
#include "sv_gamespy.h"

#include "gcdkey/gcdkeys.h"

static char     gamemode[128];
static qboolean gcdInitialized = qfalse;

static const char *SECRET_GS_KEYS[] = {"M5Fdwc", "h2P1c9", "y32FDc"};

static const unsigned int GCD_GAME_IDS[] = {
    0,
    641,
    802,
};

static const char *GS_GAME_NAME[] = {"mohaa", "mohaas", "mohaab"};

static const unsigned int GAMESPY_DEFAULT_PORT = 12300;

void qr_send_statechanged(qr_t qrec);
void qr_shutdown(qr_t qrec);
void qr_process_queries(qr_t qrec);

int qr_init(
    qr_t              *qrec,
    const char         *ip,
    int                baseport,
    const char         *gamename,
    const char         *secret_key,
    qr_querycallback_t qr_basic_callback,
    qr_querycallback_t qr_info_callback,
    qr_querycallback_t qr_rules_callback,
    qr_querycallback_t qr_players_callback,
    void              *userdata
);

const char* GS_GetGameKey(unsigned int index) {
    return SECRET_GS_KEYS[index];
}

const char* GS_GetCurrentGameKey() {
    return GS_GetGameKey(com_target_game->integer);
}

unsigned int GS_GetGameID(unsigned int index) {
    return GCD_GAME_IDS[index];
}

unsigned int GS_GetCurrentGameID() {
    return GS_GetGameID(com_target_game->integer);
}

const char* GS_GetGameName(unsigned int index) {
    return GS_GAME_NAME[index];
}

const char* GS_GetCurrentGameName() {
    return GS_GetGameName(com_target_game->integer);
}

static const char *ConvertMapFilename(const char *mapname)
{
    static char converted[1024];

    const char *name = strstr(mapname, "/");
    if (!name) {
        return mapname;
    }

    strcpy(converted, name + 1);
    return converted;
}

static void basic_callback(char *outbuf, int maxlen, void *userdata)
{
    switch (com_target_game->integer) {
    case TG_MOH:
        Info_SetValueForKey(outbuf, "gamename", TARGET_GAME_NAME_MOH);
        Info_SetValueForKey(outbuf, "gamever", TARGET_GAME_VERSION_MOH);
        break;
    case TG_MOHTA:
        Info_SetValueForKey(outbuf, "gamename", TARGET_GAME_NAME_MOHTA);
        Info_SetValueForKey(outbuf, "gamever", TARGET_GAME_VERSION_MOHTA);
        break;
    case TG_MOHTT:
        Info_SetValueForKey(outbuf, "gamename", TARGET_GAME_NAME_MOH);
        Info_SetValueForKey(outbuf, "gamever", TARGET_GAME_VERSION_MOHTT);
        break;
    }

    Info_SetValueForKey(outbuf, "location", va("%i", sv_location->integer));

    if (sv_debug_gamespy->integer) {
        Com_DPrintf("Basic callback, sent: %s\n\n", outbuf);
    }
}

static void info_callback(char *outbuf, int maxlen, void *userdata)
{
    char     infostring[1024];
    qboolean allowlean = qfalse;

    infostring[0] = 0;
    Info_SetValueForKey(infostring, "hostname", sv_hostname->string);
    Info_SetValueForKey(infostring, "hostport", Cvar_Get("net_port", "12203", 32)->string);
    Info_SetValueForKey(infostring, "mapname", ConvertMapFilename(svs.mapName));
    Info_SetValueForKey(infostring, "gametype", g_gametypestring->string);
    Info_SetValueForKey(infostring, "numplayers", va("%i", SV_NumClients()));
    Info_SetValueForKey(infostring, "maxplayers", va("%i", svs.iNumClients - sv_privateClients->integer));
    Info_SetValueForKey(infostring, "gamemode", gamemode);
    Info_SetValueForKey(infostring, "gametype_i", va("%i", g_gametype->integer));
    Info_SetValueForKey(infostring, "dedicated", Cvar_Get("ui_dedicated", "0", 0)->string);
    Info_SetValueForKey(infostring, "sprinton", Cvar_Get("sv_sprinton", "1", 0)->string);
    Info_SetValueForKey(infostring, "realism", Cvar_Get("g_realismmode", "0", 0)->string);
    Info_SetValueForKey(infostring, "pure", va("%i", sv_pure->integer));
    if ((Cvar_VariableIntegerValue("dmflags") & 0x40000) != 0) {
        allowlean = 1;
    }

    Info_SetValueForKey(infostring, "allowlean", va("%i", allowlean));
    if (strlen(infostring) < maxlen) {
        strcpy(outbuf, infostring);
    }

    if (sv_debug_gamespy->integer) {
        Com_DPrintf("Info callback, sent: %s\n\n", outbuf);
    }
}

static void rules_callback(char *outbuf, int maxlen, void *userdata)
{
    char infostring[1024];

    infostring[0] = 0;

    Info_SetValueForKey(infostring, "timelimit", Cvar_VariableString("timelimit"));
    Info_SetValueForKey(infostring, "fraglimit", Cvar_VariableString("fraglimit"));
    Info_SetValueForKey(infostring, "rankedserver", Cvar_VariableString("g_rankedserver"));

    if (strlen(infostring) < maxlen) {
        strcpy(outbuf, infostring);
    }

    if (sv_debug_gamespy->integer) {
        Com_DPrintf("Rules callback, sent: %s\n\n", outbuf);
    }
}

static void players_callback(char *outbuf, int maxlen, void *userdata)
{
    int    i;
    char   infostring[128];
    size_t infolen;
    size_t currlen = 0;

    for (i = 0; i < svs.iNumClients; i++) {
        playerState_t *ps = SV_GameClientNum(i);

        Com_sprintf(
            infostring,
            128,
            "\\player_%d\\%s\\frags_%d\\%d\\deaths_%d\\%d\\ping_%d\\%d",
            i,
            svs.clients[i].name,
            i,
            ps->stats[STAT_KILLS],
            i,
            ps->stats[STAT_DEATHS],
            i,
            svs.clients[i].ping
        );

        infolen = strlen(infostring);
        if (currlen + infolen < maxlen) {
            strcat(outbuf, infostring);
            currlen += infolen;
        }
    }
}

void SV_GamespyHeartbeat()
{
    if (!g_gametype->integer || !sv_gamespy->integer) {
        return;
    }

    if (sv_debug_gamespy->integer) {
        Com_DPrintf("GameSpy Heartbeat\n");
    }

    qr_send_statechanged(NULL);
}

void SV_ProcessGamespyQueries()
{
    if (!g_gametype->integer || !sv_gamespy->integer) {
        return;
    }

    qr_process_queries(NULL);
    gcd_think();
}

void SV_ShutdownGamespy()
{
    if (!g_gametype->integer || !sv_gamespy->integer) {
        return;
    }

    strcpy(gamemode, "exiting");

    if (gcdInitialized) {
        gcd_shutdown();
        gcdInitialized = 0;
    }

    qr_send_statechanged(NULL);
    qr_shutdown(NULL);
}

qboolean SV_InitGamespy()
{
    cvar_t     *net_ip;
    cvar_t     *net_gamespy_port;
    char        secret_key[9];
    const char *secret_gs_key;
    const char *gs_game_name;
    int         gcd_game_id;

    if (com_target_game->integer > ARRAY_LEN(SECRET_GS_KEYS)) {
        Com_Error(ERR_DROP, "Invalid target game %d for GameSpy", com_target_game->integer);
        return qfalse;
    }

    secret_gs_key = SECRET_GS_KEYS[com_target_game->integer];
    gcd_game_id   = GCD_GAME_IDS[com_target_game->integer];
    gs_game_name  = GS_GAME_NAME[com_target_game->integer];

    sv_debug_gamespy = Cvar_Get("sv_debuggamespy", "0", 0);
    sv_location      = Cvar_Get("sv_location", "1", CVAR_ARCHIVE);
    sv_gamespy       = Cvar_Get("sv_gamespy", "1", CVAR_LATCH);

    if (!sv_gamespy->integer || !g_gametype->integer) {
        return qfalse;
    }

    strcpy(gamemode, "openplaying");
    strcpy(secret_key, secret_gs_key);

    net_ip           = Cvar_Get("net_ip", "localhost", CVAR_LATCH);
    net_gamespy_port = Cvar_Get("net_gamespy_port", va("%i", GAMESPY_DEFAULT_PORT), CVAR_LATCH);

    if (qr_init(
            NULL,
            net_ip->string,
            net_gamespy_port->integer,
            gs_game_name,
            secret_key,
            basic_callback,
            info_callback,
            rules_callback,
            players_callback,
            NULL
        )) {
        Com_DPrintf("Error starting query sockets in SV_GamespyInit\n");
        return qfalse;
    }

    if (!sv_gamespy->integer) {
        strcpy(gamemode, "exiting");
        qr_send_statechanged(NULL);
    }

    if (!gcdInitialized) {
        if (gcd_game_id) {
            gcd_init(gcd_game_id);
        }

        gcdInitialized = 1;
    }

    return qtrue;
}
