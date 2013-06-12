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

#ifndef LSVPDPROCCOLLECTOR_H_
#define LSVPDPROCCOLLECTOR_H_

#include <icollector.hpp>

#include <string>

namespace lsvpd
{
	/**
	 * ProcCollector collects information from the /proc filesystem.  We do not
	 * do any device discovery in /proc because every device listed here
	 * should also be listed in /sys.  There is useful information for the
	 * System object and for any IDE devices.
	 *
	 * @class ProcCollector
	 *
	 * @ingroup lsvpd
	 *
	 * @author Eric Munson <munsone@us.ibm.com>,
	 *   Brad Peters <bpeters@us.ibm.com
	 */
	class ProcCollector : public ICollector
	{
		public:
			ProcCollector( );
			~ProcCollector( );
			bool init( );
			string myName();
			Component * fillComponent( Component * fillMe );
			void initComponent( Component * newComp );
			vector<Component*> getComponents( vector<Component*>& devs );
			void fillSystem( System* sys );
			void postProcess( Component* comp ) {}
			string resolveClassPath( const string& path );
	};
}

#endif
