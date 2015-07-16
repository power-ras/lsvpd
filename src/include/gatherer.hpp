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
#ifndef LSVPDGATHERER_H
#define LSVPDGATHERER_H

#include <vector>

#include <icollector.hpp>
#include <libvpd-2/component.hpp>
#include <libvpd-2/system.hpp>

using namespace std;
using namespace lsvpd;

namespace lsvpd
{

	/**
	 * Gatherer is the interface that will be used to abstract access to the
	 * underlying system.  Gatherer will contain a vector of collector
	 * "objects" (Objects that implement the ICollector interface) that each
	 * represent a data collection point available on the system.  For
	 * instance if /sys is available there will be a SysFsCollector for it
	 * etc.
	 *
	 * @author Eric Munson <munsone@us.ibm.com>,
	 *   Brad Peters <bpeters@us.ibm.com
	 *
	 * @class Gatherer
	 *
	 * @ingroup lsvpd
	 */
	class Gatherer
	{
		public:
			Gatherer( bool limitSCSISize );
			~Gatherer( );

			/**
			 * This method returns a Component object that is filled with all of the
			 * relavent VPD that could be retrieved from the system about the
			 * specified device.  All of the available Collectors will be utilized
			 * in retrieving VPD.
			 *
			 * It takes a partially filled component, as returned from either:
			 * 		- getComponents (boot-time discovered devices) or
			 * 		- hotplugDiscoverComponent (hotplug added devices)
			 *
			 * @param fillMe
			 *   The Component to fill
			 */

			Component * getComponentDetails(Component *fillMe);

			/**
			 * Get a vector of all available device IDs.
			 *
			 * @return
			 *   A vector of all available device IDs.
			 */
			vector<string> getDeviceIDs( );

			Component * hotplugAdd(string sysFsNode);

			/**
			 * getComponentTree will query all the available Collectors for
			 * all of their Components and return the Component tree as it
			 * exists at that point with all of the appropriate parentage
			 * information updated and ready to be stored in the VPD db.  The
			 * called to this method will be responsible for deleting the
			 * pointer that is returned, but the tree itself will be deleted
			 * by System::~System and Component::~Component.
			 *
			 * @return
			 *   A System objec that is the root of the Component tree
			 */
			System* getComponentTree();

			System* getComponentTree(vector<Component*>& devs);

			/**
			 * Recursively display the device tree with parentage
			 *
			 * @param top
			 *   The root of the tree to display
			 */
			void diplayInheritanceTree(Component * top);

			/**
			 * Recursively display the device tree with parentage
			 *
			 * @param top
			 *   The root of the tree to display
			 */
			void diplayInheritanceTree(System * top);

			/**
			 * Resolve /sys/class device path to one into /sys/bus
			 *
			 * Hotplug events give us a path into /sys/class for the device
			 * being added, this method will use the first Collector (which is
			 * always the SysFsCollector) to resolve the given path to the path
			 * that we use for device IDs.
			 *
			 * @param path
			 *   The supplied path into /sys/class
			 *
			 * @return
			 *   The resolved path into /sys/bus (device ID)
			 */
			string resolveClassPath( const string& path );

		private:
			/**
			 * Recursively print the entire contents of the Component as well
			 * as parentage.
			 *
			 * @param cur
			 *   The current root of the Component tree.
			 * @param level
			 *   The level of the tree we are on (for printing indentation).
			 */
			void displayFullInheritanceTree( Component* cur, int level);

			/**
			 * Recursively print the entire contents of the System as well
			 * as its children.
			 *
			 * @param cur
			 *   The current root of the tree.
			 * @param level
			 *   The level of the tree we are on (for printing indentation).
			 */
			void displayFullInheritanceTree( System* cur, int level);

			/**
			 * Recursively print the id of the Component as well as parentage.
			 *
			 * @param cur
			 *   The current root of the Component tree.
			 * @param tabWidth
			 *   The number of tabs to print before printing the device.
			 */
			void displayBriefInheritanceTree(Component *cur, int tabWidth);

			/**
			 * Recursively print the id of the System as well as its children.
			 *
			 * @param cur
			 *   The current root of the tree.
			 * @param tabWidth
			 *   The number of tabs to print before printing the device.
			 */
			void displayBriefInheritanceTree(System *cur, int tabWidth);

			Component *findComponent(const vector<Component*> devs,
				string idNode );

			/**
			 * Assemble the vector of devices into a tree from parentage
			 * information.
			 *
			 * @param devs
			 *   The vector of devices to use
			 * @param root
			 *   The current root of the tree.
			 * @return
			 *   If bulding the subtree was successful
			 */
			bool buildTree( vector<Component*>& devs, System* root );

			/**
			 * Assemble the vector of devices into a tree from parentage
			 * information.
			 *
			 * @param devs
			 *   The vector of devices to use
			 * @param root
			 *   The current root of the tree.
			 * @return
			 *   If building the subtree was successful
			 */
			bool buildTree( vector<Component*>& devs, Component* root );

			void fillTree( vector<Component*>& devs );

			vector<ICollector *> sources;
			vector<Component*> devices;
	};

}

#endif

