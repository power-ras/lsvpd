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

#include <device.hpp>

#include <cstdlib>
#include <cstring>

namespace lsvpd
{
	const Device Device::DEFAULT_DEV = Device( );

	Device::Device( )
	{
		mID = UNKNOWN_ID;
		mName = "Unknown";
	}

	Device::Device( ifstream& pciID )
	{
		string in;
		char next;
		char str[4096];

		while( pciID.peek( ) == '#' )
		{
			pciID.ignore( 4096, '\n' );
		}

		next = pciID.peek( );

		pciID >> in;
		mID = strtol( in.c_str( ), NULL, 16 );

		while( pciID.peek( ) == ' ' )
			pciID.get( next );

		memset( str, '\0', 4096 );
		pciID.getline( str, 4096 );
		mName = str;

		next = pciID.peek( );
		bool done = false;
		while( !done && pciID )
		{
			if( next == '\t' )
			{
				pciID.get( next );
				if( pciID.peek( ) != '\t' )
				{
					pciID.putback( next );
					done = true;
				}
				else
				{
					pciID.get( next );
					SubDevice* s = new SubDevice( pciID );
					if( s == NULL )
					{
						Logger logger;
						logger.log( "Out of memory.", LOG_ERR );
						VpdException ve( "Out of memory." );
						throw ve;
					}

					if(
						!mSubDevs.insert( make_pair( s->getID( ), s ) ).second )
					{
						delete s;
					}
				}
			}
			else if( next == '#' )
			{
				pciID.ignore( 4096, '\n' );
				next = pciID.peek( );
			}
			else
			{
				done = true;
			}
		}
	}

	Device::~Device( )
	{
		map<int,SubDevice*>::iterator i, end;
		for( i = mSubDevs.begin( ), end = mSubDevs.end( ); i != end; ++i )
		{
			SubDevice* s = (*i).second;
			delete s;
		}
	}

	const SubDevice* Device::getSubDevice( int id ) const
	{
		if( id == UNKNOWN_ID )
		{
			return  &SubDevice::DEFAULT_SUB_DEV;
		}

		map<int,SubDevice*>::const_iterator i = mSubDevs.find( id );
		if( mSubDevs.end( ) != i )
		{
			return (*i).second;
		}
		return &SubDevice::DEFAULT_SUB_DEV;
	}

}
