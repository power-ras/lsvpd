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

#ifndef LSVPDDEVICELOOKUP_H_
#define LSVPDDEVICELOOKUP_H_

#include <map>
#include <string>
#include <fstream>

#include <manufacturer.hpp>

using namespace std;

namespace lsvpd
{

	/**
	 * DeviceLookup is the front end to the [pci|usb].ids files.  This file
	 * contains every known hexidecimal code for each manufacturer, device,
	 * and sub device.  To retrieve information, first create a DeviceLookup
	 * object with the appropriate *.ids file.  Query the DeviceLookup object
	 * with the getManufacturer method, passing in the Manufacturer ID found
	 * in sysfs.
	 *
	 * @class DeviceLookup
	 * @ingroup lsvpd
	 * @brief
	 *   Holds information from *.ids file in memory to avoid rescanning.
	 *
	 * @author Eric Munson <munsone@us.ibm.com>,
	 *   Brad Peters <bpeters@us.ibm.com
	 */
	class DeviceLookup
	{
		private:
			map<int,Manufacturer*> mManus;
			static string idsPrefix;

			void fillManus( ifstream& idFile );
			static void findIdsPrefix( );

		public:
			DeviceLookup( ifstream& idFile );
			~DeviceLookup( );
			const Manufacturer* getManufacturer( int id ) const;

			/**
			 * @brief
			 *   Returns the requested Manufacturer Name
			 * @param manID
			 *   the integer manufacturer ID.
			 * @ret
			 *   The Manufacturer name.
			 */
			const string& getName( int manID ) const;

			/**
			 * @brief
			 *   Returns the requested Device Name
			 * @var manID
			 *   the integer manufacturer ID.
			 * @var devID
			 *   the integer device ID.
			 * @ret
			 *   The Device name.
			 */
			const string& getName( int manID, int devID ) const;

			/**
			 * @brief
			 *   Returns the requested Sub Device Name
			 * @var manID
			 *   the integer manufacturer ID.
			 * @var devID
			 *   the integer device ID.
			 * @var subID
			 *   the integer sub device ID.
			 * @ret
			 *   The Sub Device name.
			 */
			const string& getName( int manID, int devID, int subID )const;

			static string getPciIds( );
			static string getUsbIds( );
	};
}
#endif
