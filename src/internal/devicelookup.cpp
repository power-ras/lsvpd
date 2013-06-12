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

#include <devicelookup.hpp>

#include <libvpd-2/logger.hpp>
#include <libvpd-2/vpdexception.hpp>
#include <libvpd-2/lsvpd.hpp>

/**
 * The Manufacturer object will store the id and name of a single manufacturer
 * entry from the pci.ids file.  It will also store a hash_map of devices
 * for this manufacturer.
 */
namespace lsvpd
{
	const string DeviceLookup::PCI_ID_FILE( PCI_IDS );
	const string DeviceLookup::USB_ID_FILE( USB_IDS );

	DeviceLookup::DeviceLookup( ifstream& pciID )
	{
		fillManus( pciID );
	}

	DeviceLookup::~DeviceLookup( )
	{
		map<int,Manufacturer*>::iterator i, end;
		for( i = mManus.begin( ), end = mManus.end( ); i != end; ++i )
		{
			Manufacturer* m = (*i).second;
			delete m;
		}
	}

	void DeviceLookup::fillManus( ifstream& pciID )
	{
		char next;

		while( pciID && !pciID.eof( ) )
		{
			next = pciID.peek( );
			if( next == '#' || next == '\n' )
			{
				pciID.ignore( 4096, '\n' );
			}
			else
			{
				Manufacturer* m = new Manufacturer( pciID );
				if( m == NULL )
				{
					Logger logger;
					logger.log( "Out of memory.", LOG_ERR );
					VpdException ve( "Out of memory." );
					throw ve;
				}

				if( !mManus.insert( make_pair( m->getID( ), m ) ).second )
				{
					delete m;
				}
			}
		}
	}

	const Manufacturer* DeviceLookup::getManufacturer( int id ) const
	{
		if( id == UNKNOWN_ID )
		{
			return &Manufacturer::DEFAULT_MANUFACTURER;
		}

		map<int,Manufacturer*>::const_iterator i = mManus.find( id );
		if( i == mManus.end( ) )
		{
			// Requested Manufacturer was not in the table.
			// Return the default Manucaturer Object.
			return &Manufacturer::DEFAULT_MANUFACTURER;
		}
		else
		{
			return (*i).second;
		}
	}

	const string& DeviceLookup::getName( int manID ) const
	{
		return (getManufacturer( manID ))->getName( );
	}

	const string& DeviceLookup::getName( int manID, int devID ) const
	{
		const Manufacturer *m = getManufacturer( manID );
		return (m->getDevice( devID ))->getName( );
	}

	const string& DeviceLookup::getName( int manID, int devID, int subID ) const
	{
		const Manufacturer* m = getManufacturer( manID );
		const Device* d = m->getDevice( devID );
		return (d->getSubDevice( subID ))->getName( );
	}
}
