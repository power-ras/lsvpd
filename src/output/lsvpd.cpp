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

#include <libvpd-2/vpdretriever.hpp>
#include <libvpd-2/component.hpp>
#include <libvpd-2/dataitem.hpp>
#include <libvpd-2/system.hpp>
#include <libvpd-2/vpdexception.hpp>
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

bool markedVpd = false;
bool debug = false;
string serial = "", machineType = "", path = "", devType = "", devName = "";

void printUsage( )
{
	string prefix( DEST_DIR );
	cout << "Usage: " << prefix;
	if( prefix[ prefix.length( ) - 1 ] != '/' )
	{
		cout << "/";
	}
	cout << "sbin/lsvpd [options]" << endl;
	cout << "options: " << endl;
	cout << " --help,       -h     print this usage message" << endl;
	cout << " --list=DEV,   -lDEV  print info about given device (location code, AIX name)." << endl;
	cout << " --debug,      -D     print extra information about devices (sysfs locations, etc)" << endl;
	cout << " --version,    -v     print the version of vpd tools" << endl;
	cout << " --mark,       -m     Mark VPD as global or partition-private." << endl;
	cout << " --serial=STR, -sSTR  Use STR as serial number." << endl;
	cout << " --type=STR,   -tSTR  Use STR as type/model number." << endl;
	cout << " --path=DB,    -pDB   DB is the full path to VPD db file, used instead of default." << endl;
	cout << " --zip=GZ,     -zGZ   File GZ contains database archive (overrides -d)." << endl;
}

void printVersion( )
{
	cout << "lsvpd " << VPD_VERSION << endl;
}

bool contains( const vector<DataItem*>& vec, const string& val )
{
        vector<DataItem*>::const_iterator i, end;

        for( i = vec.begin( ), end = vec.end( ); i != end; ++i )
        {
                const string name = (*i)->getValue( );
                if( name == val )
                        return true;
        }

        return false;
}

void printVPD( Component* root )
{

	bool devFound = false;

	if( debug )
	{
		vector<string> children = root->getChildren( );
		vector<string>::iterator vi, vend;

		cout << "ID: " << root->getID( ) << endl;
		if (root->getDeviceTreeNode( ).length() > 0)
			cout << "Device Tree Node: " << root->getDeviceTreeNode( ) << endl;
		cout << "Sysfs Node: " << root->getSysFsNode( ) << endl;
		cout << "Sysfs Link Target: " << root->getSysFsLinkTarget( ) << endl;
		if (root->getDeviceDriverName().length() > 0)
			cout << "Device Driver:" << root->getDeviceDriverName() << endl;
		if (root->getHalUDI( ).length() > 0)
			cout << "HAL UDI: " << root->getHalUDI( ) << endl;
		cout << "Sysfs Device Class Node: " << root->getDevClass( ) << endl;
		cout << "Parent ID: " << root->getParent( ) << endl;
		cout << "Sysfs Name: " << root->getDevSysName( ) << endl;
//		cout << "Device Tree Name: " << root->getDevTreeName( ) << endl;
		cout << "Bus: " << root->getDevBus( ) << endl;
		if (root->getDevType( ).length() > 0)
			cout << "Device Type: " << root->getDevType( ) << endl;
		cout << "Bus Address: " << root->getDevBusAddr( ) << endl;
		cout << "Loc Code: " << root->getPhysicalLocation( ) << endl;
		cout << "Second Location: " << root->getSecondLocation( ) << endl;

		if (children.size() > 0) {
			cout << "Child devices: " << endl;
			for( vi = children.begin( ), vend = children.end( ); vi != vend; ++vi )
				cout << "  " <<  *(vi) << endl;
		}
	}

	string bn = root->getDevTreeNode( );
	if( bn != "" )
	{
		bn = bn.substr( bn.rfind( '/' ) + 1 );
	}

	if( ( !root->getAIXNames( ).empty( ) || root->getDescription( ) != "" )
		&& !( devType == "chrp" && root->getDevTreeNode( ).find( "/vdevice/" )
		!= string::npos ) && ( bn.find( "pci@" ) == string::npos &&
		bn.find( "isa@" ) == string::npos ) )
	{
		if( devName != "" )
		{
			if ( (root->getPhysicalLocation() == devName) ||
			     contains(root->getAIXNames(), devName) ) {
				devFound = true;
			}

		}

		if ( devName == "" || (devName != "" && devFound == true ) )
		{
			cout << "*" << root->getFeatureCodeAC( ) << " ";
			if( markedVpd && root->getFeatureCode( ) == "")
				cout << "********";
			else
				cout << root->getFeatureCode( );
			cout << endl;

			if( root->getDescription( ) != "" )
			{
				cout << "*" << root->getDescriptionAC( ) << " " <<
					root->getDescription( );
				string val = string( root->getCD( ) );

				if( val != "" )
				{
					char s1, s2;
					s1 = val[ 0 ];
					s2 = val[ 1 ];
					val[ 0 ] = val[ 2 ];
					val[ 1 ] = val[ 3 ];
					val[ 2 ] = s1;
					val[ 3 ] = s2;

					s1 = val[ 4 ];
					s2 = val[ 5 ];
					val[ 4 ] = val[ 6 ];
					val[ 5 ] = val[ 7 ];
					val[ 6 ] = s1;
					val[ 7 ] = s2;

					cout << " (" << val << ")";
				}
				cout << endl;
			}

			if( root->getRecordType( ) != "" )
				cout << "*" << root->getRecordTypeAC( )<< " " <<
					root->getRecordType( ) << endl;
			/*AX*/
			vector<DataItem*>::const_iterator j, stop;
			for( j = root->getAIXNames( ).begin( ),
				stop = root->getAIXNames( ).end( ); j != stop; ++j )
			{
				cout << "*" << (*j)->getAC( ) << " " << (*j)->getValue( ) << endl;
			}

			/*MF*/
			if( root->getManufacturer( ) != "" )
				cout << "*" << root->getManufacturerAC( ) << " " <<
					root->getManufacturer( ) << endl;
			/*TM*/
			if( root->getModel( ) != "" )
				cout << "*" << root->getModelAC( ) << " " <<
					root->getModel( ) << endl;
			/* SE */
			if( root->getMachineSerial( ) != "" )
				cout << "*" << root->getMachineSerialAC() << " " <<
					root->getMachineSerial( ) << endl;
			/*CD*/
			if( root->getCD( ) != "" )
				cout << "*" << root->getCDAC( ) << " " << root->getCD( ) << endl;
			/*NA*/
			if( root->getNetAddr( ) != "" )
				cout << "*" << root->getNetAddrAC( ) << " " <<
					root->getNetAddr( ) << endl;
			/*FN*/
			if( root->getFRU( ) != "" )
				cout << "*" << root->getFRUAC( ) << " " << root->getFRU( ) <<
					endl;
			/*RM*/
			if( root->getFirmwareVersion( ) != "" )
				cout << "*" << root->getFirmwareVersionAC( ) << " "
					<< root->getFirmwareVersion( ) << endl;
			/*MN*/
			if( root->getManufacturerID( ) != "" )
				cout << "*" << root->getManufacturerIDAC( ) << " " <<
					root->getManufacturerID( ) << endl;
			/*RL*/
			if( root->getFirmwareLevel( ) != "" )
				cout << "*" << root->getFirmwareLevelAC( ) << " "
					<< root->getFirmwareLevel( ) << endl;
			/*SN*/
			if( root->getSerialNumber( ) != "" )
				cout << "*" << root->getSerialNumberAC( ) << " " <<
					root->getSerialNumber( ) << endl;
			/*EC*/
			if( root->getEngChange( ) != "" )
				cout << "*" << root->getEngChangeAC( ) << " " <<
					root->getEngChange( ) << endl;
			/*PN*/
			if( root->getPartNumber( ) != "" )
				cout << "*" << root->getPartNumberAC( ) << " " <<
					root->getPartNumber( ) << endl;
			/*N5*/
			if( root->getn5( ) != "" )
				cout << "*" << root->getn5AC( ) << " " <<
					root->getn5( ) << endl;
			/*N6*/
			if( root->getn6( ) != "" )
				cout << "*" << root->getn6AC( ) << " " <<
					root->getn6( ) << endl;

			for( j = root->getDeviceSpecific( ).begin( ),
				stop = root->getDeviceSpecific( ).end( );
				j != stop; ++j )
			{

				if (!(*j)->getAC().compare("SZ")) {
					/* Accounts for AIX lsvpd output, which uses key
					 * 'Size' rather than 'SZ'.  BZ 54756 */
					cout << "*Size " << (*j)->getValue( ) << endl;
				}
				else
					cout << "*" << (*j)->getAC( ) << " " << (*j)->getValue( ) << endl;
			}

			if( root->getMicroCodeImage( ) != "" )
				cout << "*" << root->getMicroCodeImageAC( ) << " " <<
					root->getMicroCodeImage( ) << endl;

			for( j = root->getUserData( ).begin( ),
				stop = root->getUserData( ).end( );
				j != stop; ++j )
			{
				cout << "*" << (*j)->getAC( ) << " " << (*j)->getValue( ) << endl;
			}

			if( root->getPhysicalLocation( ) != "" )
				cout << "*" << root->getPhysicalLocationAC( ) << " " <<
					root->getPhysicalLocation( ) << endl;

			if( root->getKeywordVersion( ) != "" )
				cout << "*" << root->getKeywordVersionAC( ) << " " <<
					root->getKeywordVersion( ) << endl;
		}
	}
	if( debug )
		cout << " ------------------------- " << endl;

	vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		printVPD( *i );
	}

}

void printVPD( System* root )
{
	if( devName == "" )
	{
		/* bpeters: Where did this come from?  Seems sketchy. */
		cout << "*VC 5.0" << endl;

		devType = root->getArch( );

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

		if( machineType != "" )
			cout << "*" << root->getMachineTypeAC( ) << " " <<
				machineType << endl;
		else if( root->getMachineType( ) != "" )
			cout << "*" << root->getMachineTypeAC( ) << " " <<
				root->getMachineType( ) << endl;

		if( serial != "" )
			cout << "*" << root->getSerial1AC( ) << " " << serial << endl;
		else if( root->getSerial1( ) != "" )
			cout << "*" << root->getSerial1AC( ) << " " <<
				root->getSerial1( ) << endl;

		if( serial != "" )
			cout << "*" << root->getProcessorIDAC( ) << " " << serial << endl;
		else if( root->getProcessorID( ) != "" )
			cout << "*" << root->getProcessorIDAC( ) << " " <<
				root->getProcessorID( ) << endl;

		if( root->getOS( ) != "" )
			cout << "*" << root->getOSAC( ) << " " << root->getOS( ) << endl;

		if( root->getSerial2( ) != "" )
		{
			cout << "*" << root->getFeatureCodeAC( ) << " ";
			if( markedVpd && root->getFeatureCode( ) == "" )
				cout << "********";
			else
				cout << root->getFeatureCode( );
			cout << endl;

			cout << "*" << root->getDescriptionAC( ) << " " <<
				root->getDescription( ) << endl;
			cout << "*" << root->getRecordTypeAC( ) << " " <<
				root->getRecordType( ) << endl;
			cout << "*" << root->getFlagFieldAC( ) << " " <<
				root->getFlagField( ) << endl;
			cout << "*" << root->getBrandAC( ) << " " <<
				root->getBrand( ) << endl;
			cout << "*" << root->getLocationAC( ) << " " <<
				root->getLocation( ) << endl;
			cout << "*" << root->getSerial2AC( ) << " " <<
				root->getSerial2( ) << endl;
			cout << "*" << root->getMachineModelAC( ) << " " <<
				root->getMachineModel( ) << endl;
			cout << "*" << root->getSUIDAC( ) << " " << root->getSUID( ) << endl;
			cout << "*" << root->getKeywordVerAC( ) << " " <<
				root->getKeywordVer( ) << endl;
		}

		vector<DataItem*>::const_iterator j, stop;
		for( j = root->getDeviceSpecific( ).begin( ), stop = root->getDeviceSpecific( ).end( );
			j != stop; ++j )
		{
			cout << "*" << (*j)->getAC( ) << " " << (*j)->getValue( ) << endl;
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
	char opts [] = "hDvms::t::z:l:p:";
	bool done = false;
	bool compressed = false;
	System * root = NULL;
	VpdRetriever* vpd = NULL;
	int index, first = 1;

	string platform = PlatformCollector::get_platform_name();

	switch (PlatformCollector::platform_type) {
	case PF_POWERKVM_PSERIES_GUEST:
	case PF_ERROR:
		cout<< "lsvpd is not supported on the " << platform << " platform" << endl;
		return 1;
	default:
		;
	}

	struct option longOpts [] =
	{
		{ "help", 0, 0, 'h' },
		{ "version", 0, 0, 'v' },
		{ "mark", 0, 0, 'm' },
		{ "serial", 2, 0, 's' },
		{ "type", 2, 0, 't' },
		{ "path", 1, 0, 'p' },
		{ "zip", 1, 0, 'z' },
		{ "list", 1, 0, 'l' },
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
			case 'm':
				markedVpd = true;
				break;

			case 'v':
				printVersion( );
				return 0;

			case 's':
				if( optarg != NULL )
					serial = optarg;
				break;

			case 't':
				if( optarg != NULL )
					machineType = optarg;
				break;

			case 'p':
				if( path == "" )
					path = optarg;
				break;

			case 'z':
				path = optarg;
				compressed = true;
				break;

                        case 'l':
				if ( optarg != NULL )
					devName = optarg;
				break;

			case 'D':
				debug = true;
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
		{
			env = "./";
		}
		else
		{
			env = path.substr( 0, index + 1 );
		}
		db = path.substr( index + 1 );

		try
		{
			vpd = new VpdRetriever( env, db );
		}
		catch( exception& e )
		{
			cout << "Unable to process vpd DB " << path << ". Possibly corrupted DB" <<endl;
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
			cout << "sbin/vpdupdate before running lsvpd." << endl;
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
