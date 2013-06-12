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

#ifndef LSVPDDEVICE_H_
#define LSVPDDEVICE_H_

#include <map>
#include <string>
#include <fstream>

#include <subdevice.hpp>

using namespace std;

namespace lsvpd
{
	/**
	 * The Device object will store the id and name of a device, to retrieve
	 * subdevice information query the Device object using the getSubDevice
	 * method with the SubDevice id found in sysfs.
	 *
	 * @class Device
	 * @ingroup lsvpd
	 * @brief
	 *   Holds Device information from [pci|usb].ids file
	 *
	 * @author Eric Munson <munsone@us.ibm.com>,
	 *   Brad Peters <bpeters@us.ibm.com
	 */
	class Device
	{
		private:
			int mID;
			string mName;
			map<int,SubDevice*> mSubDevs;

		public:
			Device( );
			Device( ifstream& pciID );
			~Device( );
			const SubDevice* getSubDevice( int id ) const;

			inline const string& getName( ) const { return mName; }
			inline int getID( ) const { return mID; }

			static const Device DEFAULT_DEV;

	};
}

#endif
