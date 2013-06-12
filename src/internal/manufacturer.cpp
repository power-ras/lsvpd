/***************************************************************************
 *   Copyright (C) 2006, IBM                                               *
 *                                                                         *
 *   Maintained by:                                                        *
 *   Eric Munson and Brad Peters                                           *
 *   munsone@us.ibm.com, bpeters@us.ibm.com                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <manufacturer.hpp>

#include <cstdlib>
#include <cstring>

/**
 * The Manufacturer object will store the id and name of a single manufacturer
 * entry from the pci.ids file.  It will also store a map of devices
 * for this manufacturer.
 */
namespace lsvpd
{
	const Manufacturer Manufacturer::DEFAULT_MANUFACTURER = Manufacturer( );

	Manufacturer::Manufacturer( )
	{
		mID = UNKNOWN_ID;
		mManuName = "Unknown";
	}

	Manufacturer::Manufacturer( ifstream& pciID )
	{
		int devID;
		string in;
		bool done = false;
		char str[4096];
		char next;

		pciID >> in;
		mID = strtol( in.c_str( ), NULL, 16 );
		next = pciID.peek( );
		while( next == ' ' || next == '\t' )
		{
			pciID.get( next );
			next = pciID.peek( );
		}

		memset( str, '\0', 4096 );
		pciID.getline( str, 4096 );
		mManuName = str;

		while( !done && pciID )
		{
			char next = pciID.peek( );
			if( next == '#' || next == '\n' )
			{
				pciID.ignore( 4096, '\n' );
			}
			else if( next == '\t' )
			{
				pciID.get( next );
				Device* d = new Device( pciID );
				if( d == NULL )
				{
					Logger logger;
					logger.log( "Out of memory.", LOG_ERR );
					VpdException ve( "Out of memory." );
					throw ve;
				}

				if( d->getID( ) != UNKNOWN_ID )
				{
					if( !mDevices.insert( make_pair( d->getID( ), d ) ).second )
					{
						delete d;
					}
				}
				else
				{
					delete d;
				}
			}
			else
			{
				done = true;
			}
		}
	}

	Manufacturer::~Manufacturer( )
	{
		map<int,Device*>::iterator i, end;
		for( i = mDevices.begin( ), end = mDevices.end( ); i != end; ++i )
		{
			Device* d = (*i).second;
			delete( d );
		}
	}

	const Device* Manufacturer::getDevice( int id ) const
	{
		if( mID == UNKNOWN_ID )
		{
			return &Device::DEFAULT_DEV;
		}

		map<int,Device*>::const_iterator i = mDevices.find( id );
		if( i == mDevices.end( ) )
		{
			return &Device::DEFAULT_DEV;
		}

		return (*i).second;
	}
}
