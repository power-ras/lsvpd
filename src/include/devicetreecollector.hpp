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
#include <platformcollector.hpp>
#include <fswalk.hpp>
#include <libvpd-2/component.hpp>
#include <libvpd-2/system.hpp>

#include <vector>

namespace lsvpd
{


	#define DEVTREEPATH		"/proc/device-tree"

	/* RTAS Specific definitions */
	#define RTAS_VPD_TYPE	0x82

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
			/* Platform value */
			platform platForm;

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

			/* Handlers for collecting Platform Specific Data */

			/* Check whether we are on RTAS based system */
			inline bool isPlatformRTAS()
			{
				return (platForm == PF_POWERVM_LPAR);
			}

			/**
			 * Collect the platform VPD.
			 *
			 * This really depends on the underlying
			 * platform. i.e, RTAS or OPAL. Calls the platform specific routine.
			 */
			void getPlatformVPD(vector<Component*>& devs);
			/**
			 * Populate platform specific VPD for System.
			 */
			void getSystemVPD( System *sys );

			/**
			 * RTAS Platform Handlers.
			 */
			void getRtasSystemParams(vector<Component*>& devs);
			void getRtasVPD(vector<Component*>& devs);
			void getRtasSystemLocationCode( System *sys );
			void getRtasSystemVPD( System *sys );

			/**
			 * Populate system VPD.
			 *
			 * @param sys
			 *   The object holding all the collected system VPD.
			 */
			void fillSystem( System* sys );

			void postProcess( Component* comp );

			string resolveClassPath( const string& path );

		private:
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

			/**
			 * Parse VPD Header for the platform.
			 * Fills *fruName with the description.
			 * Fills *recordStart with the ptr to the beginning of
			 * record.
			 */ 
			unsigned int parseVPDHeader( char *buf,
								char **fruName, char **recordStart );
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
			unsigned int parseVPDBuffer( Component* fillMe, char * buf );

			/**
			 * Parse the Rtas Vpd Buffer into key value pairs and set the
			 * values in a Component.
			 *
			 * @param devs
			 *   The vector of devices discovered so far.
			 * @param rtasData
			 *   The buffer containing the information returned by the RTAS
			 * call
			 * @param rtasDataSize
			 *   The size of the rtasData buffer.
			 */
			void parseRtasVpd( vector<Component*>& devs, char *rtasData,
				int rtasDataSize);

			/**
			 * Pares the System VPD  and add the information to the
			 * System object.
			 *
			 * @param data
			 *   The VPD buffer for the System.
			 * @param sys
			 *   The System object.
			 */
			void parseSysVPD( char * data, System* sys );

			FSWalk fsw;
	};

}

#endif /*LSVPDDEVICETREECOLLECTOR_H_*/
