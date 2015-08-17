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
#define TRACE_ON

#include <libvpd-2/vpdretriever.hpp>
#include <libvpd-2/component.hpp>
#include <libvpd-2/dataitem.hpp>
#include <libvpd-2/system.hpp>
#include <libvpd-2/debug.hpp>
#include <platformcollector.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for getopt_long
#endif

#include <unistd.h>
#include <getopt.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

using namespace std;
using namespace lsvpd;

extern char *optarg;
extern int optind, opterr, optopt;

bool scsiAdapters = false, ethernet = false, scsiDevices = false, debug = false;
string serial = "", machineType = "", path = "";

void printUsage( )
{
	string prefix( DEST_DIR );
	cout << "Usage: " << prefix;
	if( prefix[ prefix.length( ) - 1 ] != '/' )
	{
		cout << "/";
	}
	cout << "sbin/lsvio [options]" << endl;
	cout << "options: " << endl;
	cout << " --help,     -h   print this usage message" << endl;
	cout << " --debug,    -D   print extra information about devices (sysfs locations, etc)" << endl;
	cout << " --version,  -v   print the version of vpd tools" << endl;
	cout << " --scsi,     -s   List virtual SCSI adapters" << endl;
	cout << " --ethernet, -e   List virtual Ethernet adapters" << endl;
	cout << " --devices,  -d   List virtual SCSI devices and associated adapters" << endl;
	cout << " --path=DB,  -pDB DB is the full path to VPD db file, used instead of default." << endl;
	cout << " --zip=GZ,   -zGZ File GZ contains database archive (overrides -p)." << endl;
	cout << "At least one of -s, -e, -d must be given." << endl;
}

void printVersion( )
{
	cout << "lsvio " << VPD_VERSION << endl;
}

void printVPD( Component* root )
{
	if( debug )
	{
		cout << "-----------------------------" << endl;
		cout << "ID: " << root->getID( ) << endl;
		cout << "Deivce Tree Node: " << root->getDeviceTreeNode( ) << endl;
		cout << "Sysfs Node: " << root->getSysFsNode( ) << endl;
		cout << "Sysfs Link Target: " << root->getSysFsLinkTarget( ) << endl;
		cout << "HAL UDI: " << root->getHalUDI( ) << endl;
		cout << "Sysfs Device Class Node: " << root->getDevClass( ) << endl;
		cout << "Parent ID: " << root->getParent( ) << endl;
		cout << "Sysfs Name: " << root->getDevSysName( ) << endl;
		cout << "Device Tree Name: " << root->getDevTreeName( ) << endl;
		cout << "Bus: " << root->getDevBus( ) << endl;
		cout << "Device Type: " << root->getDevType( ) << endl;
		cout << "Bus Address: " << root->getDevBusAddr( ) << endl;
		cout << "Loc Code: " << root->getPhysicalLocation( ) << endl;
		cout << "Second Location: " << root->getSecondLocation( ) << endl;

		vector<string>::const_iterator i, end;
		for( i = root->getChildren( ).begin( ), end = root->getChildren( ).end( );
		     i != end; ++i )
		{
			cout << *(i) << endl;
		}
	}

	string dtNode = root->getDevTreeNode( );
	if( dtNode.find( "vdevice" ) != string::npos )
	{
		if( ethernet && ( dtNode.find( "ethernet" ) != string::npos ||
				  dtNode.find( "l-lan" ) != string::npos ) )
		{
			vector<DataItem*>::const_iterator j, stop;
			for( j = root->getAIXNames( ).begin( ),
			     stop = root->getAIXNames( ).end( ); j != stop; ++j )
			{
				cout << (*j)->getValue( ) << " ";
			}
			cout << root->getPhysicalLocation( ) << endl;
		}
		else if( ( scsiAdapters || scsiDevices )
			 && dtNode.find( "v-scsi" ) != string::npos )
		{
			if( !root->getAIXNames( ).empty( ) )
			{
				vector<DataItem*>::const_iterator j, stop;
				for( j = root->getAIXNames( ).begin( ),
				     stop = root->getAIXNames( ).end( ); j != stop; ++j )
				{
					cout << (*j)->getValue( ) << " ";
				}
			}
			else
			{
				string node;
				int loc = 0;
				node = root->getDevTreeNode( );
				loc = node.rfind( '/' );
				node = node.substr( loc +1 );
				loc = node.find( '@' );
				cout << node.substr( 0, loc ) << node.substr( node.length( ) - 1 ) << " ";
			}
			cout << root->getPhysicalLocation( ) << endl;
		}
	}
	else if( scsiDevices &&
		 root->getSysFsLinkTarget( ).find( "vio" ) != string::npos &&
		 root->getDevBus( ).find( "scsi" ) != string::npos )
	{
		if( !root->getAIXNames( ).empty( ) )
		{
			vector<DataItem*>::const_iterator j, stop;
			for( j = root->getAIXNames( ).begin( ),
			     stop = root->getAIXNames( ).end( ); j != stop; ++j )
			{
				cout << (*j)->getValue( ) << " ";
			}
		}
		else
		{
			string node;
			int loc = 0;
			node = root->getDevTreeNode( );
			loc = node.rfind( '/' );
			node = node.substr( loc +1 );
			loc = node.find( '@' );
			cout << node.substr( 0, loc ) <<  node.substr( node.length( ) - 1 ) << " ";
		}
		cout << root->getPhysicalLocation( ) << endl;
	}

	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		printVPD( *i );
	}
}

void printVPD( System* root )
{
	if( debug )
	{
		cout << "ID: " << root->getID( ) << endl;
		cout << "Device Tree Node: " << root->getDevTreeNode( ) << endl;
		vector<string>::const_iterator i, end;
		cout << "Children: " << endl;
		for( i = root->getChildren( ).begin( ), end = root->getChildren( ).end( );
		     i != end; ++i )
		{
			cout << *(i) << endl;
		}
	}

	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		printVPD( *i );
	}
}

int main( int argc, char** argv )
{
	char opts [] = "hsDvedp:z:";
	bool done = false;
	bool compressed = false;
	System * root = NULL;
	VpdRetriever* vpd = NULL;
	int index, first = 1;
	int rc = 1;

	string platform = PlatformCollector::get_platform_name();

	switch (PlatformCollector::platform_type) {
	case PF_PSERIES_KVM_GUEST:	/* Fall through */
	case PF_OPAL:		/* Fall through */
		rc = 0;
	case PF_NULL:	/* Fall through */
	case PF_ERROR:
		cout<< "lsvio is not supported on the "
			<< platform << " platform" << endl;
		return rc;
	default:
		;
	}

	struct option longOpts [] =
	{
		{ "help", 0, 0, 'h' },
		{ "version", 0, 0, 'v' },
		{ "scsi", 0, 0, 's' },
		{ "ethernet", 0, 0, 'e' },
		{ "devices", 0, 0, 'd' },
		{ "path", 1, 0, 'p' },
		{ "zip", 1, 0, 'z' },
		{ "debug", 0, 0, 'D' },
		{ 0, 0, 0, 0 }
	};

	if (geteuid() != 0) {
		cout << "Must be run as root!" << endl;
		return -1;
	}

	while( !done )
	{
		switch( getopt_long( argc, argv, opts, longOpts, &index ) )
		{
		case 's':
			scsiAdapters = true;
			break;

		case 'v':
			printVersion( );
			return 0;

		case 'e':
			ethernet = true;
			break;

		case 'd':
			scsiDevices = true;
			break;

		case 'p':
			if( path == "" )
				path = optarg;
			break;

		case 'D':
			debug = true;
			scsiDevices = true;
			scsiAdapters = true;
			ethernet = true;
			break;

		case 'z':
			path = optarg;
			compressed = true;
			break;

		case -1:
			done = true;
			if ( first && argc > 1 ) {
				printUsage( );
				return 0;
			}
			break;

		case 'h':
			printUsage( );
			return 0;
		default:
			printUsage( );
			return -1;
		}
		first = 0;
	}

	if( !( scsiAdapters || ethernet || scsiDevices ) )
	{
		printUsage( );
		return 0;
	}

	if( path != "" )
	{
		string env, db;
		int index;

		if( compressed )
		{
			gzFile gzf = gzopen( path.c_str( ), "rb" );
			if( gzf == NULL )
			{
				cout << "Failed to open database archive " << path << endl;
				return 1;
			}

			index = path.rfind( '.' );
			path = path.substr( 0, index );
			int fd = open( path.c_str( ), O_CREAT | O_WRONLY,
				       S_IRGRP | S_IWUSR | S_IRUSR | S_IROTH );
			if( fd < 0 )
			{
				cout << "Failed to open file for uncompressed database archive"
					<< endl;
				return 1;
			}

			char buffer[ 4096 ] = { 0 };

			int in = 0;

			while( ( in = gzread( gzf, buffer, 4096 ) ) > 0 )
			{
				int out = 0, tot = 0;
				while( ( out = write( fd, buffer + tot, in - tot ) ) > 0 &&
				       tot < in )
					tot += out;
				memset( buffer, 0 , 4096 );
			}
			close( fd );

			if( gzclose( gzf ) != 0 )
			{
				int err;
				cout << "Error reading archive " << path << ".gz: " <<
					gzerror( gzf, &err ) << endl;
				return 1;
			}
		}

		index = path.rfind( "/" );
		env = path.substr( 0, index + 1 );
		db = path.substr( index + 1 );
		try
		{
			vpd = new VpdRetriever( env, db );
		}
		catch( exception& e )
		{
			string prefix( DEST_DIR );
			cout << "Please run " << prefix;
			if( prefix[ prefix.length( ) - 1 ] != '/' )
			{
				cout << "/";
			}
			cout << "sbin/vpdupdate before running lsvio." << endl;
			return 1;
		}
	}
	else
	{
		try
		{
			vpd = new VpdRetriever( );
		}
		catch( exception& e )
		{
			string prefix( DEST_DIR );
			cout << "Please run " << prefix;
			if( prefix[ prefix.length( ) - 1 ] != '/' )
			{
				cout << "/";
			}
			cout << "sbin/vpdupdate before running lsvio." << endl;
			return 1;
		}
	}

	if( vpd != NULL )
	{
		try
		{
			root = vpd->getComponentTree( );
		}
		catch( VpdException& ve )
		{
			cout << "Error reading VPD DB: " << ve.what( ) << endl;
			delete vpd;
			return 1;
		}

		delete vpd;
	}

	if( root != NULL )
	{
		if( root->getDevTreeNode( ) == "" )
		{
			cout << "lsvio is only implemented on systems that support virtual IO." << endl;
			delete root;
			return 0;
		}

		printVPD( root );
		delete root;
	}

	if( compressed )
	{
		unlink( path.c_str( ) );
	}

	return 0;
}
