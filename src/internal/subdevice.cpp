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

#include <subdevice.hpp>

#include <cstdlib>
#include <cstring>

namespace lsvpd
{
	const SubDevice SubDevice::DEFAULT_SUB_DEV = SubDevice( );

	SubDevice::SubDevice( )
	{
		mID = UNKNOWN_ID;
		mManuID = UNKNOWN_ID;
		mName = "Unknown";
	}

	SubDevice::SubDevice( ifstream& pciID )
	{
		string in;
		char str[4096];
		char next;

		while( pciID.peek( ) == '#' )
		{
			pciID.ignore( 4096, '\n' );
		}

		if( pciID )
		{
			pciID >> in;
			mManuID = strtol( in.c_str( ), NULL, 16 );
		}

		if( pciID )
		{
			pciID >> in;
			mID = strtol( in.c_str( ), NULL, 16 );
		}

		while( pciID && pciID.peek( ) == ' ' )
		{
			pciID.get( next );
		}

		if( pciID )
		{
			memset( str, '\0', 4096 );
			pciID.getline( str, 4096 );
			mName = str;
		}
	}

	SubDevice::~SubDevice( )
	{
	}
}
