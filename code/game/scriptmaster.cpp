/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

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

// scriptmaster.cpp : Spawns at the beginning of the maps, parse scripts.

#include "glb_local.h"
#include "scriptmaster.h"
#include "scriptthread.h"
#include "gamescript.h"
#include "game.h"
#include "g_spawn.h"
#include "object.h"
#include <world.h>
#include <compiler.h>

#ifdef WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined ( CGAME_DLL )

#include <hud.h>

#define SCRIPT_Printf cgi.Printf
#define SCRIPT_DPrintf cgi.DPrintf

#elif defined ( GAME_DLL )

#include <hud.h>

#include "../game/dm_team.h"
#include "../game/player.h"
#include "../game/entity.h"
#include "../game/huddraw.h"
#include "../game/weaputils.h"
#include "../game/camera.h"
#include "../game/consoleevent.h"

#define SCRIPT_Printf gi.Printf
#define SCRIPT_DPrintf gi.DPrintf

#endif

con_set< str, ScriptThreadLabel > m_scriptCmds;

str vision_current;
qboolean disable_team_change;
qboolean disable_team_spectate;

ScriptMaster Director;

ScriptEvent scriptedEvents[ SE_MAX ];

CLASS_DECLARATION( Class, ScriptEvent, NULL )
{
	{ NULL, NULL }
};

//
// world stuff
//
Event EV_RegisterAlias
(
	"alias",
	EV_DEFAULT,
	"ssSSSS",
	"alias_name real_name arg1 arg2 arg3 arg4",
	"Sets up an alias."
);
Event EV_RegisterAliasAndCache
(
	"aliascache",
	EV_DEFAULT,
	"ssSSSS",
	"alias_name real_name arg1 arg2 arg3 arg4",
	"Sets up an alias and caches the resourse."
);
Event EV_Cache
(
	"cache",
	EV_CACHE,
	"s",
	"resourceName",
	"pre-cache the given resource."
);

CLASS_DECLARATION( Listener, ScriptMaster, NULL )
{
	{ &EV_RegisterAlias,						&ScriptMaster::RegisterAlias },
	{ &EV_RegisterAliasAndCache,				&ScriptMaster::RegisterAliasAndCache },
	{ &EV_Cache,								&ScriptMaster::Cache },
	{ NULL, NULL }
};

void ScriptMaster::Archive( Archiver& arc )
{
	ScriptClass *scr;
	ScriptVM *m_current;
	ScriptThread *m_thread;
	int num;
	int i, j;

	if( arc.Saving() )
	{
		num = ScriptClass_allocator.Count();
		arc.ArchiveInteger( &num );

		MEM_BlockAlloc_enum< ScriptClass, MEM_BLOCKSIZE > en = ScriptClass_allocator;
		for( scr = en.NextElement(); scr != NULL; scr = en.NextElement() )
		{
			scr->ArchiveInternal( arc );

			num = 0;
			for( m_current = scr->m_Threads; m_current != NULL; m_current = m_current->next )
				num++;

			arc.ArchiveInteger( &num );

			for( m_current = scr->m_Threads; m_current != NULL; m_current = m_current->next )
				m_current->m_Thread->ArchiveInternal( arc );
		}
	}
	else
	{
		arc.ArchiveInteger( &num );

		for( i = 0; i < num; i++ )
		{
			scr = new ScriptClass();
			scr->ArchiveInternal( arc );

			arc.ArchiveInteger( &num );

			for( j = 0; j < num; j++ )
			{
				m_thread = new ScriptThread( scr, NULL );
				m_thread->ArchiveInternal( arc );
			}
		}
	}

	timerList.Archive( arc );
	m_menus.Archive( arc );
}

void ScriptMaster::ArchiveString( Archiver& arc, const_str& s )
{
	str s2;
	byte b;

	if( arc.Loading() )
	{
		arc.ArchiveByte( &b );
		if( b )
		{
			arc.ArchiveString( &s2 );
			s = AddString( s2 );
		}
		else
		{
			s = 0;
		}
	}
	else
	{
		if( s )
		{
			b = 1;
			arc.ArchiveByte( &b );

			s2 = Director.GetString( s );
			arc.ArchiveString( &s2 );
		}
		else
		{
			b = 0;
			arc.ArchiveByte( &b );
		}
	}
}

const char *ScriptMaster::ConstStrings[] =
{
	"",
	"touch", "block", "trigger", "use",
	"damage", "location",
	"say", "fail", "bump",
	"default", "all",
	"move_action", "resume", "open", "close", "pickup", "reach", "start", "teleport",
	"move", "move_end", "moveto", "walkto", "runto", "crouchto", "crawlto", "stop",
	"reset", "prespawn", "spawn", "playerspawn", "skip", "roudstart",
	"visible", "not_visible",
	"done", "animdone", "upperanimdone", "saydone", "flaggedanimdone",
	"idle", "walk", "shuffle", "anim/crouch.scr",
	"forgot",
	"jog_hunch", "jog_hunch_rifle",
	"killed", "alarm", "scriptclass", "ai/utils/fact_script_factory.scr", "death", "death_fall_to_knees",
	"enemy", "dead", "mood", "patrol", "runner", "follow", "action",
	"move_begin", "action_begin", "action_end", "success",
	"entry", "exit", "path", "node", "ask_count", "attacker", "usecover", "waitcover",
	"void", "end", "attack", "near",
	"papers", "check_papers",
	"timeout",
	"hostile", "leader",
	"gamemap",
	"bored", "nervous", "curious", "alert", "greet", "depend",
	"anim", "anim_scripted", "anim_curious", "animloop", "undefined", "notset",
	"increment", "decrement", "toggle",
	"normal", "suspend", "mystery", "surprise",
	"anim/crouch_run.scr", "anim/aim.scr", "anim/shoot.scr", "anim/mg42_shoot.scr", "anim/mg42_idle.scr", "anim_mg42_reload.scr",
	"drive",
	"global/weapon.scr", "global/moveto.scr",
	"global/anim.scr", "global/anim_scripted.scr", "global/anim_noclip.scr",
	"global/walkto.scr", "global/runto.scr",
	"aimat",
	"global/disabled_ai.scr",
	"global/crouchto.scr", "global/crawlto.scr", "global/killed.scr", "global/pain.scr",
	"pain", "track", "hasenemy",
	"anim/cower.scr", "anim/stand.scr", "anim/idle.scr", "anim/surprise.scr", "anim/standshock.scr", "anim/standidentify", "anim/standflinch.scr",
	"anim/dog_idle.scr", "anim/dog_attack.scr", "anim/dog_curious.scr", "anim/dog_chase.scr",
	"cannon", "grenade", "heavy",
	"item", "items", "item1", "item2", "item3", "item4",
	"stand", "mg", "pistol", "rifle", "smg",
	"turnto", "standing", "crouching", "prone", "offground", "walking", "running", "falling",
	"anim_nothing", "anim_direct", "anim_path", "anim_waypoint", "anim_direct_nogravity",
	"emotion_none", "emotion_neutral",
	"emotion_worry", "emotion_panic", "emotion_fear",
	"emotion_disgust", "emotion_anger",
	"emotion_aiming", "emotion_determined",
	"emotion_dead",
	"emotion_curious",
	"anim/emotion.scr",
	"forceanim", "forceanim_scripted",
	"turret", "cover",
	"anim/pain.scr", "anim/killed.scr", "anim/attack.scr", "anim/sniper.scr",
	"knees", "crawl", "floor",
	"anim/patrol.scr", "anim/run.scr",
	"crouch", "crouchwalk", "crouchrun",
	"anim/crouch_walk.scr", "anim/walk.scr", "anim/prone.scr",
	"anim/runawayfiring.scr", "anim/run_shoot.scr",
	"anim/runto_alarm.scr", "anim/runto_casual.scr", "anim/runto_cover.scr", "anim/runto_danger.scr", "anim/runto_dive.scr", "anim/runto_flee.scr", "anim/runto_inopen.scr",
	"anim/disguise_salute.scr", "anim/disguise_wait.scr", "anim/disguise_papers.scr", "anim/disguise_enemy.scr", "anim/disguise_halt.scr", "anim/disguise_accept.scr", "anim/disguise_deny.scr",
	"anim/cornerleft.scr", "anim/cornerright.scr", "anim/overattack.scr", "anim/continue_last_anim.scr",
	"flagged",
	"anim/fullbody.scr",
	"internal",
	"salute", "sentry", "officier", "rover",
	"none", "machinegunner",
	"disguise",
	"dog_idle", "dog_attack", "dog_curious", "dog_grenade",
	"anim/grenadeturn.scr", "anim/grenadekick.scr", "anim/grenadethrow.scr", "anim/grenadetoss.scr", "anim/grenademartyr.scr",
	"movedone",
	"aim", "ontarget",
	"unarmed",
	"balcony_idle", "balcony_curious", "balcony_attack", "balcony_disguise", "balcony_grenade", "balcony_pain", "balcony_killed",
	"weaponless",
	"death_balcony_intro", "death_balcony_loop", "death_balcony_outtro",
	"sounddone",
	"noclip",
	"german", "american", "spectator", "freeforall", "allies", "axis",
	"draw", "kills", "allieswin", "axiswin",
	"anim/say_curious_sight.scr", "anim/say_curious_sound.scr", "anim/say_grenade_sighted.scr", "anim/say_kill.scr", "anim/say_mandown.scr", "anim/say_sighted.scr",
	"vehicleanimdone",
	"postthink",
	"turndone",
	"anim/no_anim_killed.scr",
	"mg42", "mp40",
	"remove", "delete",
	"respawn",
	"none"
};

ScriptMaster::~ScriptMaster()
{
	Reset( false );
}

void ScriptMaster::InitConstStrings( void )
{
	EventDef *eventDef;
	const_str name;
	unsigned int eventnum;
	con_map_enum< Event *, EventDef > en;

	for( int i = 0; i < sizeof( ConstStrings ) / sizeof( ConstStrings[ 0 ] ); i++ )
	{
		AddString( ConstStrings[ i ] );
	}

	Event::normalCommandList.clear();
	Event::returnCommandList.clear();
	Event::getterCommandList.clear();
	Event::setterCommandList.clear();

	en = Event::eventDefList;

	for( en.NextValue(); en.CurrentValue() != NULL; en.NextValue() )
	{
		eventDef = en.CurrentValue();
		eventnum = ( *en.CurrentKey() )->eventnum;
		str command = eventDef->command.c_str();
		command.tolower();

		name = AddString( command );

		if( eventDef->type == EV_NORMAL )
		{
			Event::normalCommandList[ name ] = eventnum;
		}
		else if( eventDef->type == EV_RETURN )
		{
			Event::returnCommandList[ name ] = eventnum;
		}
		else if( eventDef->type == EV_GETTER )
		{
			Event::getterCommandList[ name ] = eventnum;
		}
		else if( eventDef->type == EV_SETTER )
		{
			Event::setterCommandList[ name ] = eventnum;
		}
	}
}

void ScriptMaster::CloseGameScript( void )
{
	con_map_enum< const_str, GameScript * > en( m_GameScripts );
	GameScript **g;
	Container< GameScript * > gameScripts;

	for( g = en.NextValue(); g != NULL; g = en.NextValue() )
	{
		gameScripts.AddObject( *g );
	}

	for( int i = gameScripts.NumObjects(); i > 0; i-- )
	{
		delete gameScripts.ObjectAt( i );
	}

	m_GameScripts.clear();
}

void ScriptMaster::Reset( qboolean samemap )
{
	ScriptClass_allocator.FreeAll();

	stackCount = 0;
	cmdCount = 0;
	cmdTime = 0;
	maxTime = MAX_EXECUTION_TIME;
	//pTop = &avar_Stack[ 0 ];
	iPaused = 0;

#if defined ( GAME_DLL )
	for( int i = 1; i <= m_menus.NumObjects(); i++ )
	{
		Hidemenu( m_menus.ObjectAt( i ), true );
	}

	m_menus.ClearObjectList();
#endif

	if( !samemap )
	{
		for( int i = 0; i < SE_MAX; i++ )
		{
			scriptedEvents[ i ] = ScriptEvent();
		}

		CloseGameScript();
		StringDict.clear();
		InitConstStrings();
	}
}

const_str ScriptMaster::AddString( const char *s )
{
	return StringDict.addKeyIndex( s );
}

const_str ScriptMaster::AddString( str& s )
{
	return StringDict.addKeyIndex( s );
}

const_str ScriptMaster::GetString( const char *s )
{
	const_str cs = StringDict.findKeyIndex( s );

	return cs ? cs : STRING_EMPTY;
}

const_str ScriptMaster::GetString( str s )
{
	return GetString( s.c_str() );
}

str& ScriptMaster::GetString( const_str s )
{
	return StringDict[ s ];
}

void ScriptMaster::AddTiming( ScriptThread *thread, float time )
{
	timerList.AddElement( thread, level.inttime + ( int )( time * 1000.0f + 0.5f ) );
}

void ScriptMaster::RemoveTiming( ScriptThread *thread )
{
	timerList.RemoveElement( thread );
}

ScriptThread *ScriptMaster::CreateScriptThread( GameScript *scr, Listener *self, str label )
{
	return CreateScriptThread( scr, self, Director.AddString( label ) );
}

ScriptThread *ScriptMaster::CreateScriptThread( GameScript *scr, Listener *self, const_str label )
{
	ScriptClass *scriptClass = new ScriptClass( scr, self );

	return CreateScriptThread( scriptClass, label );
}

ScriptThread *ScriptMaster::CreateScriptThread( ScriptClass *scriptClass, const_str label )
{
	unsigned char *m_pCodePos = scriptClass->FindLabel( label );

	if( !m_pCodePos )
	{
		throw ScriptException( "ScriptMaster::CreateScriptThread: label '%s' does not exist in '%s'.", Director.GetString( label ).c_str(), scriptClass->Filename().c_str() );
	}

	return CreateScriptThread( scriptClass, m_pCodePos );
}

ScriptThread *ScriptMaster::CreateScriptThread( ScriptClass *scriptClass, str label )
{
	if( label.length() && *label )
	{
		return CreateScriptThread( scriptClass, Director.AddString( label ) );
	}
	else
	{
		return CreateScriptThread( scriptClass, STRING_EMPTY );
	}
}

ScriptThread *ScriptMaster::CreateScriptThread( ScriptClass *scriptClass, unsigned char *m_pCodePos )
{
	return new ScriptThread( scriptClass, m_pCodePos );
}

ScriptThread *ScriptMaster::CreateThread( GameScript *scr, str label, Listener *self )
{
	try
	{
		return CreateScriptThread( scr, self, label );
	}
	catch( ScriptException& exc )
	{
		gi.DPrintf( "ScriptMaster::CreateThread: %s\n", exc.string.c_str() );
		return NULL;
	}
}

ScriptThread *ScriptMaster::CreateThread( str filename, str label, Listener *self )
{
	GameScript *scr = GetScript( filename );

	if( !scr )
	{
		return NULL;
	}

	return CreateThread( scr, label, self );
}

ScriptClass *ScriptMaster::CurrentScriptClass( void )
{
	return CurrentThread()->GetScriptClass();
}

ScriptThread *ScriptMaster::CurrentThread( void )
{
	assert( m_CurrentThread );
	if( !m_CurrentThread )
	{
		throw ScriptException( "current thread is NULL" );
	}

	return m_CurrentThread;
}

ScriptThread *ScriptMaster::PreviousThread( void )
{
	return m_PreviousThread;
}

void ScriptMaster::ExecuteThread( GameScript *scr, str label )
{
	ScriptThread *thread = CreateThread( scr, label );

	try
	{
		if( thread ) {
			thread->Execute();
		}
	}
	catch( ScriptException& exc )
	{
		gi.DPrintf( "ScriptMaster::ExecuteThread: %s\n", exc.string.c_str() );
	}
}

void ScriptMaster::ExecuteThread( str filename, str label )
{
	GameScript *scr = GetScript( filename );

	if( !scr )
	{
		return;
	}

	ExecuteThread( scr, label );
}

void ScriptMaster::ExecuteThread( GameScript *scr, str label, Event &parms )
{
	ScriptThread *thread = CreateThread( scr, label );

	try
	{
		thread->Execute( parms );
	}
	catch( ScriptException& exc )
	{
		gi.DPrintf( "ScriptMaster::ExecuteThread: %s\n", exc.string.c_str() );
	}
}

void ScriptMaster::ExecuteThread( str filename, str label, Event &parms )
{
	GameScript *scr = GetScript( filename );

	if( !scr )
	{
		return;
	}

	ExecuteThread( scr, label, parms );
}

GameScript *ScriptMaster::GetTempScript( const char *data )
{
	GameScript *scr = new GameScript;

	scr->Load( ( void * )data, strlen( data ) );

	if( !scr->successCompile )
	{
		return NULL;
	}

	return scr;
}

GameScript *ScriptMaster::GetGameScript( str filename, qboolean recompile )
{
	const_str s = StringDict.findKeyIndex( filename );
	GameScript *scr = m_GameScripts[ s ];
	int i;

	if( scr != NULL && !recompile )
	{
		if( !scr->successCompile )
		{
			throw ScriptException( "Script '%s' was not properly loaded\n", filename.c_str() );
		}

		return scr;
	}
	else
	{
		if( scr && recompile )
		{
			Container< ScriptClass * > list;
			MEM_BlockAlloc_enum< ScriptClass, MEM_BLOCKSIZE > en = ScriptClass_allocator;
			ScriptClass *scriptClass;
			m_GameScripts[ s ] = NULL;

			for( scriptClass = en.NextElement(); scriptClass != NULL; scriptClass = en.NextElement() )
			{
				if( scriptClass->GetScript() == scr )
				{
					list.AddObject( scriptClass );
				}
			}

			for( i = 1; i <= list.NumObjects(); i++ )
			{
				delete list.ObjectAt( i );
			}

			delete scr;
		}

		return GetGameScriptInternal( filename );
	}
}

GameScript *ScriptMaster::GetGameScript( const_str filename, qboolean recompile )
{
	return GetGameScript( Director.GetString( filename ), recompile );
}

GameScript *ScriptMaster::GetGameScriptInternal( str& filename )
{
	void *sourceBuffer = NULL;
	int sourceLength;
	char filepath[ MAX_QPATH ];
	GameScript *scr;

	if( filename.length() >= MAX_QPATH )
	{
		gi.Error( ERR_DROP, "Script filename '%s' exceeds maximum length of %d\n", filename.c_str(), MAX_QPATH );
	}

	scr = m_GameScripts[ StringDict.findKeyIndex( filename ) ];

	if( scr != NULL )
	{
		return scr;
	}

	strcpy( filepath, filename.c_str() );
	gi.FS_CanonicalFilename( filepath );
	filename = filepath;

	scr = new GameScript( filename );

	m_GameScripts[ StringDict.addKeyIndex( filename ) ] = scr;

	if( GetCompiledScript( scr ) )
	{
		scr->m_Filename = Director.AddString( filename );
		return scr;
	}

	sourceLength = gi.FS_ReadFile( filename.c_str(), &sourceBuffer, true );

	if( sourceLength < 0 )
	{
		throw ScriptException( "Can't find '%s'\n", filename.c_str() );
	}

	scr->Load( sourceBuffer, sourceLength );

	gi.FS_FreeFile( sourceBuffer );

	if( !scr->successCompile )
	{
		throw ScriptException( "Script '%s' was not properly loaded", filename.c_str() );
	}

	return scr;
}

GameScript *ScriptMaster::GetScript( str filename, qboolean recompile )
{
	try
	{
		return GetGameScript( filename, recompile );
	}
	catch( ScriptException& exc )
	{
		gi.DPrintf( "ScriptMaster::GetScript: %s\n", exc.string.c_str() );
	}

	return NULL;
}

GameScript *ScriptMaster::GetScript( const_str filename, qboolean recompile )
{
	try
	{
		return GetGameScript( filename, recompile );
	}
	catch( ScriptException& exc )
	{
		gi.DPrintf( "ScriptMaster::GetScript: %s\n", exc.string.c_str() );
	}

	return NULL;
}

void ScriptMaster::ExecuteRunning( void )
{
	int i;
	int startTime;

	if( stackCount )
	{
		return;
	}

	if( timerList.IsDirty() )
	{
		cmdTime = 0;
		cmdCount = 0;
		startTime = level.svsTime;

		while( ( m_CurrentThread = ( ScriptThread * )timerList.GetNextElement( i ) ) )
		{
			level.setTime( level.svsStartTime + i );

			m_CurrentThread->m_ScriptVM->m_ThreadState = THREAD_RUNNING;
			m_CurrentThread->m_ScriptVM->Execute();
		}

		level.setTime( startTime );
		level.m_LoopProtection = true;
	}

	startTime = gi.Milliseconds();
}

void ScriptMaster::SetTime( int time )
{
	timerList.SetTime( time );
	timerList.SetDirty();
}

void ScriptMaster::PrintStatus( void )
{
	str status;
	int iThreadNum = 0;
	int iThreadRunning = 0;
	int iThreadWaiting = 0;
	int iThreadSuspended = 0;
	MEM_BlockAlloc_enum< ScriptClass, MEM_BLOCKSIZE > en = ScriptClass_allocator;
	ScriptClass *scriptClass;
	char szBuffer[ MAX_STRING_TOKENS ];

	status = "num     state      label           script         \n";
	status += "------- ---------- --------------- ---------------\n";

	for( scriptClass = en.NextElement(); scriptClass != NULL; scriptClass = en.NextElement() )
	{
		ScriptVM *vm;

		for( vm = scriptClass->m_Threads; vm != NULL; vm = vm->next )
		{
			sprintf( szBuffer, "%.7d", iThreadNum );
			status += szBuffer + str( " " );

			switch( vm->ThreadState() )
			{
			case THREAD_CONTEXT_SWITCH:
			case THREAD_RUNNING:
				sprintf( szBuffer, "%8s", "running" );
				iThreadRunning++;
				break;
			case THREAD_WAITING:
				sprintf( szBuffer, "%8s", "waiting" );
				iThreadWaiting++;
				break;
			case THREAD_SUSPENDED:
				sprintf( szBuffer, "%8s", "suspended" );
				iThreadSuspended++;
				break;
			}

			status += szBuffer;

			sprintf( szBuffer, "%15s", vm->Label().c_str() );
			status += szBuffer + str( " " );

			sprintf( szBuffer, "%15s", vm->Filename().c_str() );
			status += szBuffer;

			status += "\n";
			iThreadNum++;
		}
	}

	status += "------- ---------- --------------- ---------------\n";
	status += str( m_GameScripts.size() ) + " total scripts compiled\n";
	status += str( iThreadNum ) + " total threads ( " + str( iThreadRunning ) + " running thread(s), " + str( iThreadWaiting ) + " waiting thread(s), " + str( iThreadSuspended ) + " suspended thread(s) )\n";

	gi.Printf( status.c_str() );
}

void ScriptMaster::PrintThread( int iThreadNum )
{
	int iThread = 0;
	ScriptVM *vm;
	bool bFoundThread = false;
	str status;
	MEM_BlockAlloc_enum< ScriptClass, MEM_BLOCKSIZE > en = ScriptClass_allocator;
	ScriptClass *scriptClass;

	for( scriptClass = en.NextElement(); scriptClass != NULL; scriptClass = en.NextElement() )
	{
		for( vm = scriptClass->m_Threads; vm != NULL; vm = vm->next )
		{
			if( iThread == iThreadNum )
			{
				bFoundThread = true;
				break;
			}

			iThread++;
		}

		if( bFoundThread )
		{
			break;
		}
	}

	if( !bFoundThread )
	{
		gi.Printf( "Can't find thread id %i.\n", iThreadNum );
	}

	status = "-------------------------\n";
	status += "num: " + str( iThreadNum ) + "\n";

	switch( vm->ThreadState() )
	{
	case THREAD_CONTEXT_SWITCH:
		status += "state: running (context switch)\n";
		break;
	case THREAD_RUNNING:
		status += "state: running\n";
		break;
	case THREAD_WAITING:
		status += "state: waiting\n";
		break;
	case THREAD_SUSPENDED:
		status += "state: suspended\n";
		break;
	}

	status += "script: '" + vm->Filename() + "'\n";
	status += "label: '" + vm->Label() + "'\n";
	status += "waittill: ";

	if( !vm->m_Thread->m_WaitForList )
	{
		status += "(none)\n";
	}
	else
	{
		con_set_enum< const_str, ConList > en = *vm->m_Thread->m_WaitForList;
		Entry< const_str, ConList > *entry;
		int i = 0;

		for( entry = en.NextElement(); entry != NULL; entry = en.NextElement() )
		{
			str& name = Director.GetString( entry->key );

			if( i > 0 )
			{
				status += ", ";
			}

			status += "'" + name + "'";

			for( int j = 1; j <= entry->value.NumObjects(); j++ )
			{
				Listener *l = entry->value.ObjectAt( j );

				if( j > 1 )
				{
					status += ", ";
				}

				if( l )
				{
					status += " on " + str( l->getClassname() );
				}
				else
				{
					status += " on (null)";
				}
			}

			i++;
		}

		status += "\n";
	}

	gi.Printf( status.c_str() );
}

static int bLoadForMap( char *psMapsBuffer, const char *name )
{
	cvar_t *mapname = gi.Cvar_Get( "mapname", "", 0 );
	const char *token;

	if( !strncmp( "test", mapname->string, sizeof( "test" ) ) ) {
		return true;
	}

	token = COM_Parse( &psMapsBuffer );
	if( !token || !*token )
	{
		Com_Printf( "ERROR bLoadForMap: %s alias with empty maps specification.\n", name );
		return false;
	}

	while( token && *token )
	{
		if( !Q_stricmpn( token, mapname->string, strlen( token ) ) ) {
			return true;
		}

		token = COM_Parse( &psMapsBuffer );
	}

	return false;
}

void ScriptMaster::RegisterAliasInternal
	(
	Event *ev,
	bool bCache
	)

{
#ifdef GAME_DLL
	int i;
	char parameters[ MAX_STRING_CHARS ];
	char *psMapsBuffer;
	int subtitle;

	// Get the parameters for this alias command

	parameters[ 0 ] = 0;
	subtitle = 0;
	psMapsBuffer = NULL;

	for( i = 3; i <= ev->NumArgs(); i++ )
	{
		str s;

		// MOHAA doesn't check that
		if( ev->IsListenerAt( i ) )
		{
			Listener *l = ev->GetListener( i );

			if( l && l == Director.CurrentThread() )
			{
				s = "local";
			}
			else
			{
				s = ev->GetString( i );
			}
		}
		else
		{
			s = ev->GetString( i );
		}

		if( subtitle )
		{
			strcat( parameters, "\"" );
			strcat( parameters, s );
			strcat( parameters, "\" " );

			subtitle = 0;
		}
		else if( !s.icmp( "maps" ) )
		{
			i++;
			psMapsBuffer = ( char * )ev->GetToken( i ).c_str();
		}
		else
		{
			subtitle = s.icmp( "subtitle" ) == 0;
			
			strcat( parameters, s );
			strcat( parameters, " " );
		}
	}

	if( bLoadForMap( psMapsBuffer, ev->GetString( 1 ) ) )
	{
		gi.GlobalAlias_Add( ev->GetString( 1 ), ev->GetString( 2 ), parameters );

		if( bCache )
			CacheResource( ev->GetString( 2 ) );
	}
#endif
}

void ScriptMaster::RegisterAlias
	(
	Event *ev
	)

{
	RegisterAliasInternal( ev );
}

void ScriptMaster::RegisterAliasAndCache
	(
	Event *ev
	)

{
	RegisterAliasInternal( ev, true );
}

void ScriptMaster::Cache
	(
	Event *ev
	)

{
#ifdef GAME_DLL
	if( !precache->integer )
		return;

	CacheResource( ev->GetString( 1 ) );
#endif
}