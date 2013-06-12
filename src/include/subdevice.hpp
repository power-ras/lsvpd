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

#ifndef LSVPDSUBDEVICE_H_
#define LSVPDSUBDEVICE_H_

#include <string>
#include <fstream>
#include <libvpd-2/logger.hpp>
#include <libvpd-2/vpdexception.hpp>

using namespace std;

namespace lsvpd
{

	static const int UNKNOWN_ID = 0xffff;

	/**
	 * The SubDevice object will contain the SubDevice ID, the manufacurer id,
	 * and the SubDevice name.
	 */
	class SubDevice
	{
		private:
			int mID;
			int mManuID;
			string mName;

		public:
			static const SubDevice DEFAULT_SUB_DEV;

			SubDevice( );
			SubDevice( ifstream& pciID );
			~SubDevice( );

			inline int getID( ) const { return mID; }
			inline int getManuID( ) const { return mManuID; }
			inline const string& getName( ) const { return mName; }

	};
}
#endif
