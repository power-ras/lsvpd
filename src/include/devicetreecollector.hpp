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

#ifndef LSVPDDEVICETREECOLLECTOR_H_
#define LSVPDDEVICETREECOLLECTOR_H_

#include <icollector.hpp>
#include <fswalk.hpp>
#include <libvpd-2/component.hpp>
#include <libvpd-2/system.hpp>
#include <platformcollector.hpp>

#include <vector>

namespace lsvpd
{


	#define DEVTREEPATH		"/proc/device-tree"
	#define DT_SYS_ML_VERSION       "/proc/device-tree/ibm,opal/firmware/ml-version"
	#define DT_SYS_MI_VERSION       "/proc/device-tree/ibm,opal/firmware/mi-version"
	#define DT_SYS_CL_VERSION       "/proc/device-tree/ibm,opal/firmware/git-id"
	#define BUF_SZ                  80

	/**
	 * DeviceTreeCollector contains the logic for device discovery and VPD
	 * retrieval from /proc/device-tree and librtas if it is available.
	 *
	 * @class DeviceTreeCollector
	 *
	 * @ingroup lsvpd
	 *
	 * @author Eric Munson <munsone@us.ibm.com>,
	 *   Brad Peters <bpeters@us.ibm.com
	 */
	class DeviceTreeCollector : public ICollector
	{
		public:
			string rootDir;
			PlatformCollector *platform_collector;
			DeviceTreeCollector( );
			~DeviceTreeCollector( );

			/**
			 * This method check to see if /proc/device-tree exists.  If it
			 * does then this Collector can pull information from it.
			 *
			 * @return
			 *   true if /proc/device-tree exists, otherwise false
			 */
			bool init();

			/**
			 * fillComp() is used to fill those devices discovered through
			 * getComponents().
			 *
			 * @brief
			 *   Fills a Component with all available information.
			 *
			 * @param fillMe
			 *   The Component to fill.
			 */
			Component * fillComponent(Component * fillMe);

			void initComponent( Component * newComp );

			/**
			 * Discover devices available to this Collector.
			 *
			 * @param devs
			 *   A vector containing devices discovered by other Collectors.
			 *
			 * @return
			 *
			 */
			vector<Component*> getComponents( vector<Component*>& devs );

			/* Returns a vector of minimally filled Components */
			vector<Component*> listDevicesInTree();

			int numDevicesInTree(void);

			/**
			 * Returns a string containing s human readable name for *this.
			 *
			 * @return
			 *   A name for this collector.
			 */
			string myName(void);

			/**
			 * Attempt to fill DataItem with appropriate Source.
			 *
			 * @param di
			 *   The DataItem to fill.
			 * @param devTreeNode
			 *   The node to read from.
			 */
			void readSources( DataItem& di, const string& devTreeNode );

			/**
			 * Populate system VPD.
			 *
			 * @param sys
			 *   The object holding all the collected system VPD.
			 */
			void fillSystem( System* sys );

			void postProcess( Component* comp );

			string resolveClassPath( const string& path );

                        /* Interface to parse platform VPD data defined in RtasCollector
                         * and OpalCollector.
                        */
			void parseVPD( vector<Component*>& devs, char *Data,
					int DataSize);

                        /**
                         * Parse a provided vpd buffer into distinct key/value pairs and
                         * fill specified Component with these pairs.
                         *
                         * @param fillMe
                         *   The Component to fill
                         * @param buf
                         *   The VPD buffer to parse
                         * @return
                         *   The number of bytes consumed from the buffer
                         */
			unsigned int parseVPDBuffer( Component* fillMe, char * buf);
			void parseSysVPD( char * data, System* sys) ;

		private:

			void parseOpalVPD( vector<Component*>& devs, char *opalData,
                                int opalDataSize);

			unsigned int parseOpalVPDBuffer( Component* fillMe, char * buf);
			void parseOpalSysVPD( char * data, System* sys) ;

			void parseRtasVPD( vector<Component*>& devs, char *rtasData,
                                int rtasDataSize);

			unsigned int parseRtasVPDBuffer( Component* fillMe, char * buf);
			void parseRtasSysVPD( char * data, System* sys) ;

			void addSystemParms(Component *c);
			void cpyinto(Component *dest, Component *src);

			void transferChildren(Component *parentSrc,
					vector<Component*> *target,
					vector<Component*> *src);

			void mergeTrees(Component *, Component *, vector<Component*> *,
				vector<Component*> *);

			Component *findComponent(const vector<Component*> ,
				string  );

			Component *findCompIdNode(vector<Component*> *, const string );

			string getDevTreeName(Component *fillMe);

			Component * findSCSIParent(Component *fillMe,
				vector<Component*> devs);
			void buildSCSILocCode(Component *fillMe, vector<Component*> devs);

			vector<Component*> getComponentsVector( vector<Component*>& devs );

			bool setup(const string path_t );

			bool checkLocation( Component* comp );

			void parseMajorMinor( Component* comp, string& major,
				string& minor );

			string getBaseLoc( Component* comp );

			/**
			 * Read and parse the ibm,vpd file (if present for this Component)
			 * and fill specified Component with the values read.
			 *
			 * @param fillMe
			 *   The Component to fill
			 */
			void fillIBMVpd( Component* fillMe );

			/**
			 * fillQuickVPD
			 * @brief Collects VPD that is simply read from a file in the
			 *  devices proc/device-tree node
			 * @return Returns status of loc code lookup
			 */
			bool fillQuickVPD( Component* fillMe );

			/**
			 * Fill the description for specified Component.
			 *
			 * @param fillMe
			 *   The Component to fill
			 */
			void fillDS( Component* fillMe );

			FSWalk fsw;
	};

}

#endif /*LSVPDDEVICETREECOLLECTOR_H_*/
