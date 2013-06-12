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

#ifndef LSVPDMANUFACTURER_H_
#define LSVPDMANUFACTURER_H_

#include <map>
#include <string>
#include <fstream>

#include <device.hpp>

using namespace std;

namespace lsvpd
{
	/**
	 * The Manufacturer contains all the devices under this manufacturer.
	 * For specific Device information query the Manufacturer object using
	 * the getDevice method with the Device id found in sysfs.
	 */
	class Manufacturer
	{
		private:
			int mID;
			map<int,Device*> mDevices;
			string mManuName;

		public:
			Manufacturer( );
			Manufacturer( ifstream& pciID );
			~Manufacturer( );
			const Device* getDevice( int id ) const;

			inline const string& getName( ) const { return mManuName; }
			inline int getID( ) const { return mID; }

			static const Manufacturer DEFAULT_MANUFACTURER;
	};

}

#endif
