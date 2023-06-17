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

// ammo.h: Base class for all ammunition for entities derived from the Weapon class.

#ifndef __AMMO_H__
#define __AMMO_H__

#include "g_local.h"
#include "item.h"

class AmmoEntity : public Item
	{
	public:
      CLASS_PROTOTYPE( AmmoEntity );

	                  AmmoEntity();
      Item   *ItemPickup( Entity *other, qboolean add_to_inventory ) override;
	};

class Ammo : public Class
	{
   int amount;
   int maxamount;
   str name;
   int name_index;

	public:
      CLASS_PROTOTYPE( Ammo );

	                  Ammo();
                     Ammo(str name, int amount, int name_index );

      void           setAmount( int a );
      int            getAmount( void );
      void           setMaxAmount( int a );
      int            getMaxAmount( void );
      void           setName( str name);
      str            getName( void );
      int            getIndex( void );
      void Archive( Archiver &arc ) override;
	};

inline void Ammo::Archive
	(
	Archiver &arc
	)

   {
   Class::Archive( arc );

   arc.ArchiveInteger( &amount );
   arc.ArchiveInteger( &maxamount );
   arc.ArchiveString( &name );
	//
	// name_index not archived, because it is auto-generated by gi.itemindex
	//
   if ( arc.Loading() )
      {
      setName( name );
      }
   }

#endif /* ammo.h */