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
#ifndef LSVPDICOLLECTOR_H
#define LSVPDICOLLECTOR_H

#include <string>
#include <vector>

#include <libvpd-2/component.hpp>
#include <libvpd-2/system.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

namespace lsvpd
{

	/**
	 * ICollector is the interface for all the system data collection objects.
	 * This provides a standard way for a client to ask for hardware
	 * information.
	 *
	 * @class ICollector
	 *
	 * @ingroup lsvpd
	 *
	 * @author Eric Munson <munsone@us.ibm.com>,
	 *   Brad Peters <bpeters@us.ibm.com
	 */
	class ICollector
	{
		public:

			/**
			 * This method does any initialization that is needed after object
			 * construction it will also communicate to the Gatherer object if
			 * it has all the resources it needs to provide Component
			 * information.
			 */
			virtual bool init( ) = 0;

			virtual string myName() = 0;

			/**
			 * With this method the Collector will fill out all the
			 * information that it has on a Component.
			 */
			virtual Component * fillComponent
					(Component * fillMe ) = 0;

			virtual void initComponent
					( Component * newComp ) = 0;

			/**
			 * getComponents returns all the Components that this collector
			 * knows about exclusively.  For instance, the SysFsCollector will
			 * fill all the devices in /sys/bus/X, but the DeviceTreeCollector
			 * will only return Components that are unique to it (e.g. Memory
			 * and System Planar devices).
			 */
			virtual vector<Component*> getComponents(
				vector<Component*>& devs ) = 0;

			/**
			 * fillSystem collects all of the system level vpd of which this
			 * collector is aware and stores it into the System object passed
			 * in.
			 */
			virtual void fillSystem( System* sys ) = 0;

			/**
			 * postProcess does any processing that might be required after
			 * all the data has been collected and the Components have been
			 * assembled into their tree.
			 */
			virtual void postProcess( Component* comp ) = 0;

			/**
			 * Resolve /sys/class device path to one into /sys/bus
			 *
			 * @brief This method will resolve a /sys/devices path to the
			 * /sys/bus path that we use for device IDs.
			 *
			 * @param path
			 *   The supplied path into /sys/class
			 *
			 * @return
			 *   The resolved path into /sys/bus (device ID)
			 */
			virtual string resolveClassPath( const string& path ) = 0;

			virtual ~ICollector( ){}

			protected:
				string getAttrValue( const string& path,
					const string& attrName );

				/**
				 * Read a binary blob from given @path and store it in *data.
				 * Allocates enough memory @*data, which has to be freed by the
				 * caller.
				 * @return : Size of the blob read.
				 */
				int getBinaryData( const string& path, char **data );

				/**
				 * Sanitize a VPD value
				 * Some records have binary data that may not be captured
				 * as string. Convert such binary data to a string of its
				 * hexadecimal dump.
				 * Returns the original string it if can be represented as
				 * a string.
				 */
				string sanitizeVPDField( char *val, int len );

				/**
				 * Set a specific DataItem in specified Component with the
				 * given key/value pair.
				 *
				 * @param fillMe
				 *   The Component to fill
				 * @param key
				 *   The VPD Acronymn for the specified data
				 * @param val
				 *   The specified data
				 */
				void setVPDField( Component* fillMe, const string& key,
					const string& val , const char *file, int lineNum);

				/**
				 * Set the appropriate data field in the System object.
				 *
				 * @param sys
				 *   The System object
				 * @param key
				 *   The VPD Acronymn for the data
				 * @param val
				 *   THe VPD data
				 */
				void setVPDField( System* sys, const string& key,
					const string& val , const char *file, int lineNum);
				/**
				 * Convert the fru-type key to description.
				 */
				string getFruDescription(string &key);
	};

}

#endif
