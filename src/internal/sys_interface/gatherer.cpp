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

#include <gatherer.hpp>
#include <devicetreecollector.hpp>
#include <sysfstreecollector.hpp>
#include <proccollector.hpp>

#include <libvpd-2/lsvpd.hpp>
#include <libvpd-2/system.hpp>
#include <libvpd-2/debug.hpp>
#include <libvpd-2/vpdexception.hpp>
#include <libvpd-2/logger.hpp>

#include <sstream>
#include <iomanip>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

namespace lsvpd
{
	/**
	 * Gatherer class hosts all of the Collectors that are available on the
	 * system.  It provides unified access to the Collectors through methods
	 * that allow the user to retrieve VPD on one, many, or all devices
	 * available.
	 *
	 * @class Gatherer
	 *
	 * @ingroup lsvpd
	 *
	 * @author Eric Munson <munsone@us.ibm.com>, Brad Peters
	 * <bpeters@us.ibm.com>
	 */
	Gatherer::Gatherer( bool limitSCSISize = false )
	{
		sources = vector<ICollector*>( );

		/* -----------------------------------------------------------
		 * NOTE:  MUST MAINTAIN THIS ORDER!! sysFS called before Device-tree!
		 * -----------------------------------------------------------*/
		SysFSTreeCollector * sysFSTree =
			new SysFSTreeCollector( limitSCSISize );
		if( sysFSTree->init( ) )
		{
			sources.push_back( sysFSTree );
		}
		else
		{
			Logger logger;
			logger.log( "Error: SysFSTree (/sys) Not Found", LOG_ERR );
			delete sysFSTree;
		}

		DeviceTreeCollector * devTree = new DeviceTreeCollector( );
		if( devTree->init( ) )
		{
			sources.push_back( devTree );
		}
		else
		{
			Logger logger;
			logger.log( "Notice: /proc/device-tree Not found."
					"  This is expected on non-Power systems.", LOG_INFO );
			delete devTree;
		}

		ProcCollector * proc = new ProcCollector( );
		if( proc->init( ) )
		{
			sources.push_back( proc );
		}
		else
		{
			Logger logger;
			logger.log( "Warning: /proc Not Found", LOG_WARNING );
			delete proc;
		}
		//Any other collectors that become available need to be added here.
	}

	Gatherer::~Gatherer()
	{
		std::vector<ICollector*>::iterator i, end;
		for ( i = sources.begin( ), end = sources.end( ); i != end; ++i )
		{
			delete *i;
		}

	}

	/**
	 * Called to fill a single, partially filled component.
	 * Loop thru all data sources, filling component as going forward.
	 */
	Component * Gatherer::getComponentDetails(Component *fillMe)
	{
		vector<ICollector*>::const_iterator src, end;
		for( src = sources.begin( ), end = sources.end( ); src != end; ++src )
		{
			(*src)->fillComponent(fillMe);
		}

		return fillMe;
	}

	vector<string> Gatherer::getDeviceIDs( )
	{
		vector<Component*> devs;
		vector<Component*>::iterator start, stop;
		vector<string> devIDs;
		getComponentTree(devs);

		for( start = devs.begin( ), stop = devs.end( ); start != stop;
			++start )
		{
			devIDs.push_back(string((*start)->idNode.getValue()));
		}

		return devIDs;
	}

	/**
	 * Contruct complete collection of devices on system,
	 * by calling 'getComponents()' on each specific collector type
	 */
	System* Gatherer::getComponentTree()
	{
		Component* root = new Component( );
		vector<Component*> devs;
		System* ret = new System( );

		if( root == NULL )
		{
            Logger l;
            l.log( "Gatherer.getDeviceTree: Failed to build new Component.",
            	LOG_ERR );
            VpdException ve(
            	"Gatherer.getDeviceTree: Failed to build new Component." );
            throw ve;
		}

		if( ret == NULL )
		{
            Logger l;
            l.log( "Gatherer.getDeviceTree: Failed to build new System.",
            	LOG_ERR );
            VpdException ve(
				"Gatherer.getDeviceTree: Failed to build new System." );
            throw ve;
		}

		devs.reserve(256);
		// Create default parent node
		root->sysFsNode.setValue("/sys/devices", 100, __FILE__, __LINE__);
		root->idNode.setValue("/sys/devices", 100, __FILE__, __LINE__);
		devs.push_back(root);
		root = NULL;
		// Iterate through each collector, calling getComponents()
		vector<ICollector*>::iterator start, stop;
		for( start = sources.begin( ), stop = sources.end( ); start != stop;
			++start )
		{
			(*start)->getComponents( devs );
			(*start)->fillSystem( ret );
		}

		root = *( devs.begin( ) );
		devs.erase( devs.begin( ) );

		if( !buildTree( devs, root ) )
		{
			// We had a problem building the device tree.
			// This shouldn't ever happen because buildTree throws
			VpdException ve( "Error building device tree." );
			throw ve;
		}

		if( !devs.empty( ) || root == NULL )
		{
			// There is a serious problem.
			Logger logger;
			ostringstream msg;
			msg << "Incomplete or corrupted device list. ";
			if( !devs.empty( ))
			{
				msg << "Devs not empty! size = " << devs.size() << ": "
					<< endl;
				vector<Component*>::iterator s, e;
				for( s = devs.begin( ), e = devs.end( ); s != e; ++s )
				{
					msg << (*s)->idNode.getValue( ) << "->";
					msg << (*s)->mParent.getValue( ) << endl;
				}
			}
			else
				msg << "Root == NULL, ";
			msg << " root = " << hex << (void*)root;
			logger.log( msg.str( ), LOG_ERR );
			VpdException ve( msg.str( ) );
			throw ve;
		}

		fillTree( root->mLeaves );

		for( start = sources.begin( ), stop = sources.end( ); start != stop;
			++start )
		{
			(*start)->postProcess( root );
		}

		ret->mChildren = root->mChildren;
		ret->mLeaves = root->mLeaves;
		root->mLeaves.clear( );

		delete root;

		return ret;
	}

	/**
	 * Builds the Component subtree with root as the root of the tree.  Uses
	 * the list of child device IDs found in each Component to retrieve the
	 * approriate Component* from the devs vector.  Then recursively build the
	 * subtree for the located children of this root.
	 *
	 * @param devs
	 *   The remaining unparented devices
	 * @param root
	 *   The Component that is the root of the new subtree
	 * @return
	 *   If building the subtree was successful
	 */
	bool Gatherer::buildTree( vector<Component*>& devs, Component* root )
	{
		vector<Component*>::iterator i, end;
		const vector<string> kids = root->getChildren( );
		vector<string>::const_iterator cur, last;
		Component* next;
		bool ret;

		for( cur = kids.begin( ); cur != kids.end(); ++cur )
		{
			next = NULL;
			for( i = devs.begin( ); i != devs.end(); ++i )
				if( (*i)->idNode.dataValue == (*cur) )
				{
					next = (*i);
					devs.erase( i );
					ret = buildTree( devs, next );
					break;
				}

			if( !ret || next == NULL )
			{
				// We had a problem building the device tree.
				ostringstream msg;
				msg << "Error building subtree. ret = " << ret
					<< ", we were looking for " << (*cur);
				if (next == NULL)
					msg << ", but did not find it in devices list." << endl;
				else
					msg << endl;

				VpdException ve( msg.str( ) );
				throw ve;
			}

			next->mpParent = root;
			root->addLeaf( next );
		}
		return true;
	}

	/**
	 * Builds the Component subtree with root as the root of the tree.  Uses
	 * the list of child device IDs found in the System object to retrieve the
	 * approriate Component* from the devs vector.  Then build the subtree for
	 * the located child using the above buildTree method.
	 *
	 * @param devs
	 *   The unparented device list
	 * @param root
	 *   The System that is the root of the Component tree
	 * @return
	 *   If building the subtree was successful
	 */
	bool Gatherer::buildTree( vector<Component*>& devs, System* root )
	{
		vector<Component*>::iterator i, end;
		const vector<string> kids = root->getChildren( );
		vector<string>::const_iterator cur, last;
		Component* next;
		bool ret;

		for( cur = kids.begin( ); cur != kids.end(); ++cur )
		{
			next = NULL;
			for( i = devs.begin( ), end = devs.end( ); i != end; ++i )
			{
				if( (*i)->idNode.dataValue == (*cur) )
				{
					next = (*i);
					devs.erase( i );
					ret = buildTree( devs, next );
					break;
				}
			}

			if( !ret || next == NULL )
			{
				// We had a problem biulding the device tree.
				ostringstream msg;
				msg << "Error building subtree. ret = " << ret
					<< " and we were looking for " << (*cur);
				VpdException ve( msg.str( ) );
				throw ve;
			}

			next->mpParent = NULL;
			root->addLeaf( next );
		}
		return true;
	}

	/**
	 * Take a vector of Components and fill each one using each Collector and
	 * then take each Components list of children and recursively call fillTree
	 * on that list.
	 */
	void Gatherer::fillTree( vector<Component*>& devs )
	{
		vector<ICollector*>::iterator start, stop;
		vector<Component*>::iterator cur, end;

		if( devs.empty( ) )
			return;

		for( cur = devs.begin( ), end = devs.end( ); cur != end; ++cur )
		{
			for( start = sources.begin( ), stop = sources.end( ); start != stop;
				 ++start )
			{
				(*start)->fillComponent(*cur);
			}

			if ( !(*cur)->mLeaves.empty() )
			{
				fillTree( (*cur)->mLeaves );
			}
		}

	}

	/**
	 * Contruct complete collection of devices on system,
	 * by calling 'getComponents()' on each specific collector type
	 *
	 * @arg devs: a Component* vector, which will be filled with the
	 * 	device tree
	 */
	System* Gatherer::getComponentTree(vector<Component*>& devs)
	{
		Component* root = new Component( );
		System* ret = new System( );

		if( root == NULL )
		{
            Logger l;
            l.log( "Gatherer.getDeviceTree: Failed to build new Component.",
            	LOG_ERR );
            VpdException ve(
            	"Gatherer.getDeviceTree: Failed to build new Component." );
            throw ve;
		}

		if( ret == NULL )
		{
            Logger l;
            l.log( "Gatherer.getDeviceTree: Failed to build new System.",
            	LOG_ERR );
            VpdException ve(
            	"Gatherer.getDeviceTree: Failed to build new System." );
            throw ve;
		}

		devs.reserve(256);
		// Create default parent node
		root->idNode.setValue("/sys/devices", 100, __FILE__, __LINE__);
		devs.push_back(root);
		// Iterate through each collector, calling getComponents()
		vector<ICollector*>::iterator start, stop;
		vector<Component*>::iterator cur, end;

		for( start = sources.begin( ), stop = sources.end( ); start != stop;
			++start )
		{
			(*start)->getComponents( devs );
		}

		// Fill the component tree with full details
		for( start = sources.begin( ), stop = sources.end( ); start != stop;
			++start )
		{
			for( cur = devs.begin( ), end = devs.end( ); cur != end; ++cur )
			{
				(*start)->fillComponent(*cur);
			}
			(*start)->fillSystem( ret );
		}

		root = *( devs.begin( ) );
		devs.erase( devs.begin( ) );
		if( !buildTree( devs, root ) )
		{
			// We had a problem biulding the device tree.
			VpdException ve( "Error building device tree." );
			throw ve;
		}

		if( !devs.empty( ) || root == NULL )
		{
			// There is a serious problem.
			Logger logger;
			ostringstream msg;
			msg << "Incomplete or corrupted device list. ";
			msg << "vector size = " << devs.size( );
			msg << " root = " << hex << (void*)root;
			logger.log( msg.str( ), LOG_ERR );
			VpdException ve( msg.str( ) );
			throw ve;
		}

		root->mPhysicalLocation.dataValue = ret->mLocationCode.dataValue;

		ret->mChildren = root->mChildren;
		ret->mLeaves = root->mLeaves;
		// Clears root child list so destructor won't delete children
		root->mLeaves.clear( );

		delete root;

		return ret;
	}

	void Gatherer::diplayInheritanceTree(System * top)
	{
		displayBriefInheritanceTree(top, 0);
	}

	void Gatherer::diplayInheritanceTree(Component * top)
	{
		displayBriefInheritanceTree(top, 0);
	}

	/**
	 * Walker of tree for display
	 * @brief: Recursively walks the component tree, displaying all component
	 * fields for the entire component collection
	 */
	void Gatherer::displayFullInheritanceTree( Component* cur, int level)

	{
		int count = 0;
		const vector<Component*> children = cur->getLeaves( );

		vector<Component*>::const_iterator i, end;
		cout << "*** " << level << " ***" << endl;

		level++;
		vector<Component*> devsTmp;

		if (cur == NULL)
			return;

		cout << "Current Component has " << children.size( ) << " kids" << endl;

		for( i = children.begin( ), end = children.end( ); i != end; ++i )
		{
			cout << "Count = " << count << endl;
			displayFullInheritanceTree( (*i), level );
		}
	}

	/**
	 * @brief: Recursively walks the component tree, displaying component
	 * collection and the inheritance relationship thru indentation of output
	 */
	void Gatherer::displayBriefInheritanceTree(Component *cur, int tabWidth)
	{
		string tab;
		const vector<Component*> children = cur->getLeaves( );

		vector<Component*>::const_iterator i, end;

		tab = string("    ");
		for (int i = 1; i < tabWidth; i++)
			tab += "    ";

		cout << tab << cur->getID( ) << endl;

		for( i = children.begin( ), end = children.end( ); i != end; ++i )
		{
			int tabs = tabWidth + 1;
			displayBriefInheritanceTree( (*i), tabs );
		}
	}

	/**
	 * @brief: Recursively walks the component tree, displaying component
	 *  collection and the inheritance relationship thru indentation of output
	 */
	void Gatherer::displayBriefInheritanceTree(System *cur, int tabWidth)
	{
		string tab;
		const vector<Component*> children = cur->getLeaves( );

		vector<Component*>::const_iterator i, end;

		tab = string("    ");
		for (int i = 1; i < tabWidth; i++)
			tab += "    ";

		cout << tab << cur->getID( ) << endl;

		for( i = children.begin( ), end = children.end( ); i != end; ++i )
		{
			int tabs = tabWidth + 1;
			displayBriefInheritanceTree( (*i), tabs );
		}
	}

	/**
	 * Walker of tree for display
	 * @brief: Recursively walks the component tree, displaying all component
	 * fields for the entire component collection
	 */
	void Gatherer::displayFullInheritanceTree( System* cur, int level)
	{
		int count = 0;
		const vector<Component*> children = cur->getLeaves( );

		vector<Component*>::const_iterator i, end;
		cout << "*** " << level << " ***" << endl;

		level++;
		vector<Component*> devsTmp;

		if (cur == NULL)
			return;

		cout << "Current Component has " << children.size( ) << " kids" << endl;

		for( i = children.begin( ), end = children.end( ); i != end; ++i )
		{
			cout << "Count = " << count << endl;
			displayFullInheritanceTree( (*i), level );
		}
	}

	/**
	 * Walkes a vector of component ptr's, looking for a particular one
	 * @return the specified component, or NULL on failure
	 */
	Component *Gatherer::findComponent( const vector<Component*> devs,
		string idNode )
	{
		for (int i = 0; i < (int) devs.size(); i++)
			if(devs[i]->idNode.getValue() == idNode)
				return devs[i];

		return NULL;
	}

	string Gatherer::resolveClassPath( const string& path )
	{
		return sources[ 0 ]->resolveClassPath( path );
	}
}
