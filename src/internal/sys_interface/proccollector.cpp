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

#include <proccollector.hpp>

#include <libvpd-2/helper_functions.hpp>
#include <libvpd-2/debug.hpp>
#include <libvpd-2/logger.hpp>
#include <libvpd-2/lsvpd.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

#include <fstream>
#include <sstream>
#include <string>

using namespace std;

namespace lsvpd
{

	ProcCollector::ProcCollector( )
	{ }

	ProcCollector::~ProcCollector( )
	{ }

	bool ProcCollector::init( )
	{
		struct stat info;
		if( stat( "/proc", &info ) != 0 )
		{
			return false;
		}
		return true;
	}

	string ProcCollector::myName()
	{
		return string( "ProcCollector" );
	}

	Component * ProcCollector::fillComponent( Component * fillMe )
	{

		if( fillMe->devBus.dataValue == "ide" )
		{
			if( fillMe->mAIXNames.empty( ) )
			{
				return fillMe;
			}

			string val;
			ifstream in;
			ostringstream os;

			os << "/proc/ide/" << fillMe->mAIXNames[ 0 ]->dataValue <<
				"/model";
			in.open( os.str( ).c_str( ) );
			if( in )
			{
				char buf[ 4096 ] = { 0 };
				in.getline( buf, 4095 );
				val = buf;
				in.close( );
			}

			if( val != "" )
			{
				fillMe->mModel.setValue( val, 80, __FILE__, __LINE__ );
				string::iterator i, end;
				for( i = val.begin( ), end = val.end( ); i != end; ++i )
				{
					(*i) = toupper( *i );
				}

				if( val.find( "IBM" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "IBM", 80,
						__FILE__, __LINE__  );
				}
				else if( val.find( "QUANTUM" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Quantum", 80,
						__FILE__, __LINE__  );
				}
				else if( val.find( "MATSHITA" ) == 0 ||
						val.find( "UJDA" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Matshita", 80,
						__FILE__, __LINE__  );
				}
				else if( val.find( "HL-DT-ST" ) == 0 ||
						val.find( "LG" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "LG Electronics", 80,
						__FILE__, __LINE__  );
				}
				else if( val.find( "TOSHIBA" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Toshiba", 80,
						__FILE__, __LINE__  );
				}
				else if( val.find( "LTN" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Lite-On", 80,
						__FILE__, __LINE__  );
				}
				else if( val.find( "AOPEN" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "AOpen", 80,
						__FILE__, __LINE__ );
				}
				else if( val.find( "RICOH" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Ricoh", 80,
						__FILE__, __LINE__ );
				}
				else if( val.find( "NEC" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "NEC", 80,
						__FILE__, __LINE__ );
				}
				else if( val.find( "MAXTOR" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Maxtor", 80,
						__FILE__, __LINE__ );
				}
				else if( val.find( "HTS" ) == 0 ||
						val.find( "IC25N" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Hitachi", 80,
						__FILE__, __LINE__ );
				}
				else if( val.find( "PIONEER" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Pioneer", 80,
						__FILE__, __LINE__ );
				}
				else if( val.find( "ST" ) == 0 )
				{
					fillMe->mManufacturer.setValue( "Seagate", 80,
						__FILE__, __LINE__ );
				}
				else
				{
					fillMe->mManufacturer.setValue( "Unknown", 10,
						__FILE__, __LINE__ );
				}
			}
		}
		return fillMe;
	}

	void ProcCollector::initComponent( Component * newComp )
	{ }

	vector<Component*> ProcCollector::getComponents( vector<Component*>& devs )
	{
		return devs;
	}

	void ProcCollector::fillSystem( System* sys )
	{
		char line[ 2048 ] = { '\0' };
		ifstream in;

		in.open( "/proc/cpuinfo" );

		if( in )
		{
			u32 count = 0;
			while( in )
			{
				in.getline( line, 2048 );
				string ln = line;
				if( ln.find( "model name" ) != string::npos )
				{
					sys->mMachineType.setValue( ln.substr( ln.find( ':' ) + 2 ), 40,
						__FILE__, __LINE__ );
				}
				else if( ln.find( "processor" ) != string::npos )
					count++;

				memset( line, '\0', 2048 );
			}
			if( count == 0 )
				count = 1;

			sys->mCPUCount = count;
			in.close( );
		}

		in.open( "/proc/sys/kernel/hostname" );
		if( in )
		{
			in.getline( line, 2048 );
			string val = line;
			sys->mSerialNum1.setValue( val, 40, __FILE__, __LINE__ );
			sys->mProcessorID.setValue( val, 40, __FILE__, __LINE__ );
			in.close( );
			memset( line, '\0', 2048 );
		}

		ostringstream os;

		in.open( "/proc/sys/kernel/ostype" );
		if( in )
		{
			in.getline( line, 2048 );
			os << line << " ";
			in.close( );
			memset( line, '\0', 2048 );
		}

		in.open( "/proc/sys/kernel/osrelease" );
		if( in )
		{
			in.getline( line, 2048 );
			os << line;
			in.close( );
		}

		sys->mOS.setValue( os.str( ), 40, __FILE__, __LINE__ );
	}

	string ProcCollector::resolveClassPath( const string& path )
	{
		return "";
	}
}
