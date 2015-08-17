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

#include <rtascollector.hpp>
#include <platformcollector.hpp>
#include <libvpd-2/vpdretriever.hpp>
#include <libvpd-2/component.hpp>
#include <libvpd-2/dataitem.hpp>
#include <libvpd-2/system.hpp>

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
#include <iomanip>

using namespace std;
using namespace lsvpd;

extern char *optarg;
extern int optind, opterr, optopt;

bool tabular = false, all = false, debug = false;
string device = "", path = "";

/* Firmware version information from the VPD db */
string pfw;
string fw_t, fw_p, fw_b;
string fwl_t, fwl_p, fwl_b;

void printUsage( )
{
	string prefix( DEST_DIR );
	cout << "Usage: " << prefix;
	if( prefix[ prefix.length( ) - 1 ] != '/' )
	{
		cout << "/";
	}
	cout << "sbin/lsmcode [options]" << endl;
	cout << "options: " << endl;
	cout << " --help,       -h     print this usage message" << endl;
	cout << " --debug,      -D     print extra information about devices (sysfs locations, etc)" << endl;
	cout << " --version,    -v     print the version of vpd tools" << endl;
	cout << " --All,        -A     Display microcode level for as many devices as possible." << endl;
	cout << " --device=DEV, -dDEV  Only display microcode level for specified device (DEV)." << endl;
	cout << " --path=DB,    -pDB   DB is the full path to VPD db file, used instead of default." << endl;
	cout << " --zip=GZ,     -zGZ   File GZ contains database archive (overrides -d)." << endl;
}

void printVersion( )
{
	cout << "lsmcode " << VPD_VERSION << endl;
}

bool printSystem( const vector<Component*>& leaves )
{
	vector<Component*>::const_iterator i, end;

	for( i = leaves.begin( ), end = leaves.end( ); i != end; ++i )
	{
		Component* c = (*i);
		if( c->getDescription( ) == "System Firmware" )
		{
			string mi( c->getMicroCodeImage( ) );
			const string *pml;
			string ml, fw;

			/* New firmware level record type */
			pml = c->getMicroCodeLevel( );
			if (pml)
				ml = *pml;
			else
				ml = string("");


			if( mi != "" )
			{
				int pos1 = mi.find( ' ' );
				int pos2 = mi.rfind( ' ' );

				fw_t = mi.substr(0, pos1);
				fw_p = mi.substr(pos1 + 1, (pos2 - pos1) - 1);
				fw_b = mi.substr(pos2 + 1);

			}

			if ( ml != "" )
			{
				int pos1 = ml.find( ' ' );
				int pos2 = ml.rfind( ' ' );

				fwl_t = ml.substr(0, pos1);
				fwl_p = ml.substr(pos1 + 1, (pos2 - pos1) - 1);
				fwl_b = ml.substr(pos2 + 1);
			}

			if (fwl_t != "" )
			{
				/*
				 * When we have both ML and MI records, the output
				 * is in the following format.
				 * ML_t (MI_t) (t) ML_p (MI_p) (p) ML_b (MI_b) (b)
				 */
				fw = fwl_t;
				if (fw_t != "")
					fw +=  " (" + fw_t + ") ";
				fw += "(t) " + fwl_p;
				if (fw_p != "")
					fw += " (" + fw_p + ") ";
				fw += "(p) " + fwl_b;
				if (fw_t != "")
					fw += " (" + fw_b + ") ";
				fw += "(b)";
			} else {

				/* Old style */
				fw = fw_t + " (t) " + fw_p + " (p) " +
					fw_b + " (b)";
			}


			if( all )
				cout << "sys0!system: ";
			else
				cout << "Version of System Firmware is ";

			cout << fw ;

			vector<DataItem*>::const_iterator j, stop;
			for( j = c->getDeviceSpecific( ).begin( ),
			     stop = c->getDeviceSpecific( ).end( ); j != stop; ++j )
			{
				string val = (*j)->getValue( );
				if( val.find( "PFW" ) != string::npos )
				{
					pfw = val.substr( 4 );
					break;
				}
			}

			if( pfw != "" ) {
				if ( all )
					cout << "|service: ";
				else
					cout << endl << "Version of PFW is ";
				cout << pfw;
			}
			cout << endl;

			return true;
		}

		if( printSystem( c->getLeaves( ) ) )
			return true;
	}

	return false;
}

void printVPD( Component* root )
{
	string fwVersion;
	string fwLevel;

	if( debug )
	{
		cout << "-----------------------------" << endl;
		cout << "ID: " << root->getID( ) << endl;
		cout << "Device Tree Node: " << root->getDeviceTreeNode( ) << endl;
		cout << "Sysfs Node" << root->getSysFsNode( ) << endl;
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

	fwVersion = root->getFirmwareVersion( );
	fwLevel = root->getFirmwareLevel( );
	if (fwVersion != "" || fwLevel != "")
	{
		const vector<DataItem*> aixNames = root->getAIXNames( );
		vector<DataItem*>::const_iterator j, stop = aixNames.end( );
		bool report = true;
		string fwString = "";

		if (device != "")
		{
			/* Report only the specified device. */
			report = false;
			for( j = aixNames.begin( ); j != stop; j++ )
			{
				if( (*j)->getValue( ) == device )
				{
					report = true;
					break;
				}
			}
		}
		if (report)
		{
			for( j = aixNames.begin( ); j != stop; j++ )
			{
				cout << (*j)->getValue( ) << " ";
			}
			/* Construct fw string */
			if (fwLevel != "")
				fwString = fwLevel + " (";
			fwString += fwVersion;
			if (fwLevel != "")
				fwString += ")";
			cout << "!" << root->getModel( );
			cout << "." << fwString << endl;
		}
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
		cout << "-----------------------------" << endl;
	}

	if( device == "" )
		printSystem( root->getLeaves( ) );

	if( !all && device == "" )
		return;

	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		printVPD( *i );
	}
}

int main( int argc, char** argv )
{
	char opts [] = "hvDAd:p:z:";
	bool done = false;
	bool compressed = false;
	System * root = NULL;
	VpdRetriever* vpd = NULL;
	int index, first = 1;
	int rc = 1;
	string platform = PlatformCollector::get_platform_name();

	switch (PlatformCollector::platform_type) {
	case PF_PSERIES_KVM_GUEST:	/* Fall through */
		rc = 0;
	case PF_NULL:	/* Fall through */
	case PF_ERROR:
		cout<< "lsmcode is not supported on the "
			<< platform << " platform" << endl;
		return rc;
	default:
		;
	}

	struct option longOpts [] =
	{
		{ "help", 0, 0, 'h' },
		{ "version", 0, 0, 'v' },
		{ "All", 0, 0, 'A' },
		{ "device", 1, 0, 'd' },
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

		case 'v':
			printVersion( );
			return 0;

		case 'A':
			all = true;
			tabular = true;
			break;

		case 'D':
			debug = true;
			break;

		case 'd':
			if( device != "" )
			{
				cout << "Please specify at most one device."
					<< endl;
				return 1;
			}
			device = optarg;
			break;

		case 'p':
			if( path == "" )
				path = optarg;
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
			return 1;
		}
		first = 0;
	}

	if( all && device != "" )
	{
		cout << "-A option conflicts with -d." << endl;
		return 1;
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
		if(index == -1)
			env = "./";
		else
			env = path.substr( 0, index + 1 );

		db = path.substr( index + 1 );
		try
		{
			vpd = new VpdRetriever( env, db );
		}
		catch( exception& e )
		{
			cout << "Unable to process vpd DB " << path << ". Possibly corrupted DB" << endl;
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
			cout << "sbin/vpdupdate before running lsmcode." << endl;
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
		printVPD( root );
		delete root;
	}

	if( compressed )
	{
		unlink( path.c_str( ) );
	}

	return 0;
}
