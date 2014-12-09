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
#include <libvpd-2/helper_functions.hpp>
#include <platformcollector.hpp>

#include <iostream>
#include <cerrno>
#include <fstream>
#include <iomanip>
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
#include <regex.h>
#include <string.h>

using namespace std;
using namespace lsvpd;

#define MAX_AIX_NAMES 64
#define IBM_CPU_MODEL_LIST     "/etc/lsvpd/cpu_mod_conv.conf"

/* struct model_conv matches model number, ex: 8842-P1Z, to model name, 
	ex: BladeCenter JS20 */
struct model_conv {
	string model_number;
	string model_name;
};

static vector<model_conv *> cpu_models;

extern char *optarg;
extern int optind, opterr, optopt;

bool verbose = false, specific = false, debug = false, devFound = false;
string devName = "", path = "";

/* Preferred AIX names.  Those names
 * appearing earlier in the list will always be picked (if available)
 * over those appearing later */
string AIXNamePrefs[MAX_AIX_NAMES] =
{
	"eth*",
	"ehc*",
	"ath*",
	"hd*",
	"ide*",
	"sd*",
	"sr*",
	"sg*",
	"fb*",
	"ib*",
	"mouse*",
	"js*",
	"wlan*",
	"pcspk*",
	"input*",
	"audio*",
	"serial*"
	"tty*",
	"audio*",
	"usb_host*",
	"usbdev*",
	"usb*",
	"card*",
	"acpi*",
	"rsxx*",
	""
};

void printUsage( )
{
	string prefix( DEST_DIR );
	cout << "Usage: " << prefix;
	if( prefix[ prefix.length( ) - 1 ] != '/' )
	{
		cout << "/";
	}
	cout << "sbin/lscfg [options]" << endl;
	cout << "options:" << endl;
	cout << " --help,      -h     Print this message" <<  endl;
	cout << " --debug,     -D     print extra information about devices (sysfs locations, etc)" << endl;
	cout << " --version,   -V     print the version of vpd tools" << endl;
	cout << " --verbose,   -v     Display VPD when available" << endl;
	cout << " --specific,  -p     Display system-specific device information" << endl;
	cout << " --list=NAME, -lNAME Displays device information for named device." << endl;
	cout << " --data=DB,   -dDB   Displays vpd from specified database DB, instead of default" << endl;
	cout << " --zip=GZ,    -zGZ   File GZ contains database archive (overrides -d)" << endl;
}

void printVersion( )
{
	cout << "lscfg " << VPD_VERSION << endl;
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

void printField( const string& label, const string& data )
{
	cout << left;	// Append fill characters at the end.
	cout << "        ";
	cout << setfill( '.' ) << setw( 26 ) << label;
	if( label.length( ) >= 26 )
	{
		cout << endl;
		cout << "        ";
		cout << setfill( '.' ) << setw( 26 ) << ".";
	}

	if( data.length( ) > 46 )
	{
		int over = data.length( ) - 46;
		int stop;
		// Find the first white space before we run over our line limit
		for( stop = data.length( ) - over;
			data.at( stop ) != ' ' && data.at( stop ) != '\t' && stop > 0;
			stop-- );

		if( stop <= 0 )
		{
			cout << data << endl;
		}
		else
		{
			cout << data.substr( 0, stop ) << endl;
			cout << "                                    ";
			cout << data.substr( stop + 1 ) << endl;
		}
	}
	else
	{
		cout << data << endl;
	}
}

void printSpecific( Component* root )
{
	if( root->getDevTreeNode( ) != "" )
	{
		string node;
		int loc = 0;
		node = root->getDevTreeNode( );
		loc = node.rfind( '/' );
		node = node.substr( loc +1 );
		loc = node.find( '@' );
		if( loc != (int) string::npos )
		{
			cout << "  Name:  ";
			if( loc == (int) string::npos )
				cout << node;
			else
				cout << node.substr( 0, loc );
			cout << endl;

			if( root->getModel( ) != "" )
				cout << "    " << root->getModelHN( ) << ":  " <<
					root->getModel( ) << endl;

			cout << "    Node:  " << node << endl;

			if( root->getPhysicalLocation( ) != "" )
				cout << "    " << root->getPhysicalLocationHN( ) << ": " <<
					root->getPhysicalLocation( ) << endl;
			cout << endl;
		}
	}

	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		printSpecific( *i );
	}
}

void printVPD( Component* root )
{
	string firstAIXName;
	ostringstream names;
	bool doneAIX, islocation = false;
	int ii;

	if( debug )
	{
		cout << " ------------------------- " << endl;
		cout << "ID: " << root->getID( ) << endl;
		cout << "Device Tree Node: " << root->getDeviceTreeNode( ) << endl;
		cout << "Sysfs Node: " << root->getSysFsNode( ) << endl;
		cout << "Sysfs Link Target: " << root->getSysFsLinkTarget( ) << endl;
		cout << "Description: " << root->getDescription( ) << endl;
		if (root->getDeviceDriverName().length() > 0)
			cout << "Device Driver Name: " << root->getDeviceDriverName() << endl;
		cout << "HAL UDI: " << root->getHalUDI( ) << endl;
		cout << "Sysfs Device Class Node: " << root->getDevClass( ) << endl;
		cout << "Parent ID: " << root->getParent( ) << endl;
		cout << "Sysfs Name: " << root->getDevSysName( ) << endl;
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

//	if (root->getDevBus().length() > 0)
	if(!root->getAIXNames( ).empty( ))
	{
		if( devName == "" ||
		    contains( root->getAIXNames( ), devName ) ||
		    (islocation = (root->getPhysicalLocation() == devName)))
		{
			devFound = true;
			if( verbose )
			{
				cout << "  ";
			}
			else
			{
				cout << "+ ";
			}

			doneAIX = false;
			vector<DataItem*>::const_iterator j, stop;
			j = root->getAIXNames().begin();
			stop = root->getAIXNames().end();
			if (j != stop)
				firstAIXName = (*j)->getValue();
			else
				firstAIXName = "";

			if (debug || verbose) {
				for( j = root->getAIXNames( ).begin( ),
					stop = root->getAIXNames( ).end( ); j != stop; ++j ) {
						names << (*j)->getValue( ) << " ";
				}
			}
			else {
				if (devName != "" && !islocation) {
					names << devName;
				} else {
				/* look for a preferred name */
					ii = 0;
					while ((ii < MAX_AIX_NAMES)
							&& (AIXNamePrefs[ii].length() > 0)
							&& !doneAIX)
					{
						stop = root->getAIXNames( ).end( );
						j = root->getAIXNames( ).begin( );
						while ((j != stop) && (!doneAIX)) {
							if (HelperFunctions::matches(AIXNamePrefs[ii], (*j)->getValue()) ) {
								names << (*j)->getValue();
								doneAIX = true; /* We're done*/
								j = stop;
							}
							j++;
						}
						ii++;
					}
					if (!doneAIX) {
						names << firstAIXName;
					}
				}

			}

			if (debug) {
				cout << "Device Names: ";
			}

			cout << left << setfill( ' ' ) << setw( 17 ) << names.str( );
			if (debug)
				cout << endl << "Physical Location: " << endl;

			if( root->getPhysicalLocation( ).length( ) <= 22 )
			{
				cout << setw( 22 ) << root->getPhysicalLocation( );
			}
			else
			{
				cout << root->getPhysicalLocation( ) << endl;
				if( root->getDescription( ) != "" )
					cout << "                                         ";
			}

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
			}

			ostringstream os;
			os << root->getDescription( );

			if( val != "" )
			{
				os << " (" << val << ")";
			}

			if(	val.length( ) + root->getDescription( ).length( ) <= 35 )
			{
				cout << setw( 38 ) << os.str( ) << endl;
			}
			else
			{
				val = os.str( );
				int over = val.length( ) - 35;
				int stop;
				for( stop = val.length( ) - over;
					val.at( stop ) != ' ' && val.at( stop ) != '\t' &&
					stop > 0; stop-- );

				if( stop <= 0 )
				{
					cout << val << endl;
				}
				else
				{
					cout << val.substr( 0, stop ) << endl;
					cout << "                                         ";
					cout << val.substr( stop + 1 ) << endl;
				}
			}

			if( verbose )
			{
				if( root->getManufacturer( ) != "" )
					printField( root->getManufacturerHN( ),
						root->getManufacturer( ) );

				if( root->getModel( ) != "" )
					printField( root->getModelHN( ), root->getModel( ) );

				if( root->getMachineSerial( ) != "" )
					printField( root->getMachineSerialHN( ), root->getMachineSerial( ) );

				if( root->getEngChange( ) != "" )
					printField( root->getEngChangeHN( ),
						root->getEngChange( ) );

				if( root->getFRU( ) != "" )
					printField( root->getFRUHN( ), root->getFRU( ) );

				if( root->getManufacturerID( ) != "" )
					printField( root->getManufacturerIDHN( ),
						root->getManufacturerID( ) );

				if( root->getPartNumber( ) != "" )
					printField( root->getPartNumberHN( ),
						root->getPartNumber( ) );

				if( root->getSerialNumber( ) != "" )
					printField( root->getSerialNumberHN( ),
						root->getSerialNumber( ) );

				if( root->getFirmwareVersion( ) != "" )
					printField( root->getFirmwareVersionHN( ),
						root->getFirmwareVersion( ) );

				if( root->getFirmwareLevel( ) != "" )
					printField( root->getFirmwareLvlHN( ),
						root->getFirmwareLevel( ) );

				if( root->getNetAddr( ) != "" )
					printField( root->getNetAddrHN( ), root->getNetAddr( ) );

				vector<DataItem*>::const_iterator j, done;
				const vector<DataItem*> sp = root->getDeviceSpecific( );
				for( j = sp.begin( ), done = sp.end( ); j != done; ++j )
				{
					os.str( "" );
					os << (*j)->getHumanName( ) << ".(" << (*j)->getAC( ) <<
						")";
					printField( os.str( ), (*j)->getValue( ) );
				}

				if( root->getPhysicalLocation( ) != "" )
				{
					os.str( "" );
					os << root->getPhysicalLocationHN( ) << ".(" <<
						root->getPhysicalLocationAC( ) << ")";
					printField( os.str( ), root->getPhysicalLocation( ) );
				}

				const vector<DataItem*> data = root->getUserData( );
				for( j = data.begin( ), done = data.end( ); j != done; ++j )
				{
					os.str( "" );
					os << (*j)->getHumanName( ) << ".(" << (*j)->getAC( ) <<
						")";
					printField( os.str( ), (*j)->getValue( ) );
				}
				cout << endl;
			}
		}
	}

	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		if ( debug )
					cout << " - Child Device - " << endl;
		printVPD( *i );
	}
}

void printPlatform( Component* root )
{
	if (root->getDescription( ) != "")
	{
		if( root->getDescription( ).length( ) > 18 )
			cout << "      " << root->getDescription( );
		else
			cout << "      " << setfill( ' ' ) << setw( 18 ) <<
				root->getDescription( );

		cout << ":" << endl;
	}

	if( root->getRecordType( ) != "" )
		printField( "Product Specific.(RT)", root->getRecordType( ) );
	if( root->getSerialNumber( ) != "" )
		printField( root->getSerialNumberHN( ), root->getSerialNumber( ) );
	if(root->getFRU( ) != "" )
		printField( root->getFRUHN( ), root->getFRU( ) );
	if( root->getPartNumber( ) != "" )
		printField( root->getPartNumberHN( ), root->getPartNumber( ) );

	const vector<DataItem*> data = root->getDeviceSpecific( );
	vector<DataItem*>::const_iterator i, end;
	ostringstream label;
	for( i = data.begin( ), end = data.end( ); i != end; ++i )
	{
		label.str( "" );
		label << (*i)->getHumanName( ) << ".(" << (*i)->getAC( ) << ")";
		printField( label.str( ), (*i)->getValue( ) );
	}

	if ( root->getMicroCodeImage() != "" ) {
		label.str( "" );
		label << root->getMicroCodeImageHN() << ".(" <<
			root->getMicroCodeImageAC() << ")";
		printField( label.str( ), root->getMicroCodeImage() );
	}

	const vector<DataItem*> uData = root->getUserData( );
	for( i = uData.begin( ), end = uData.end( ); i != end; ++i )
	{
		label.str( "" );
		label << (*i)->getHumanName( ) << ".(" << (*i)->getAC( ) << ")";
		printField( label.str( ), (*i)->getValue( ) );
	}

	if (root->getPhysicalLocation( ) != "")
		cout << "      Physical Location: " <<
			root->getPhysicalLocation( ) << endl << endl;
}

void printSpecific( System* root )
{
	if( root->getDevTreeNode( ) == "" )
		return;

	cout << endl << "  PLATFORM SPECIFIC" << endl;

	cout << "  Name:" << endl;
	cout << "    " << root->getMachineTypeHN( ) << ":  " <<
		root->getMachineType( ) << endl;
	cout << "    Node:  /" << endl;
	cout << "    Device Type:  " << root->getArch( ) << endl << endl;

	cout << "      System VPD      :" << endl;
	printField( "Product Specific.(RT)", root->getRecordType( ) );
	printField( root->getFlagFieldHN( ), root->getFlagField( ) );
	printField( root->getBrandHN( ), root->getBrand( ) );
	printField( root->getSerial1HN( ), root->getSerial1( ) );
	printField( root->getMachineTypeHN( ), root->getMachineType( ) );
	printField( root->getKeywordVerHN( ), root->getKeywordVer( ) );
	cout << "      Physical Location: " << root->getLocation( ) << endl <<
		endl;

	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		if( (*i)->getID( ).find( "/rtas/" ) != string::npos )
			printPlatform( *i );
	}

	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		if( (*i)->getID( ).find( "/rtas/" ) == string::npos )
			printSpecific( *i );
	}
}

/** initCPUModelList
 * @brief Loads CPU model conversion table.  
 * Data loaded enables translation from encoded model
 * data pulled from /proc/device-tree/model to actual CPU release name
 * @param filename Path and filename of cpu model conversion file
 */
int initCPUModelList(const string& filename)
{
	char tmp_line[512];
	string line;
	model_conv *tmp;
	ostringstream err;
	ifstream fin(filename.c_str());

	if (fin.fail()) {
		cerr << "Error opening model conversion file at: "
			<< filename
			<< ";  Details: lsvpd not installed?" << endl;
		return -ENOENT;
	}

	while(!fin.eof()) {
		tmp = new model_conv;
		if( tmp == NULL )
			return -ENOMEM;

		fin.getline(tmp_line,512);
		line = string(tmp_line);

		HelperFunctions::parseString(line, 1, tmp->model_number);
		HelperFunctions::parseString(line, 2, tmp->model_name);
		
//		cout << "Parsed from cpu_models file: " << tmp->model_number << " , " << tmp->model_name << endl;

		cpu_models.push_back(tmp);
	}
	return 0;
}

int getCPUModelNameFromList(System *root, string &name)
{
	vector<model_conv *>::iterator i, end;

	if (initCPUModelList(IBM_CPU_MODEL_LIST))
		return -ENOENT;

	i = cpu_models.begin();
	end = cpu_models.end();
	while (++i != end) {
		if ((*i)->model_number == root->getMachineModel()) {
			name = (*i)->model_name;
			return 0;
		}
	}

	return -1;
}

int OpalgetCPUModelName(System *root, string &name)
{
	FILE *fin;
	char buf[512];

	fin = fopen ("/proc/device-tree/model-name", "r");
	if (fin != NULL) {
		if (fgets(buf, 512, fin) != NULL) {
			name = string (buf);
			return 0;
		}
	}

	return -1;
}

int getCPUModelName(System *root, string &name)
{
	vector<model_conv *>::iterator i, end;
	int platform = PlatformCollector::platform_type;

	if (root->getMachineModel().length() <= 0) {
		/* Likely on a non-Power system.  Get CPU model info from /proc/cpuinfo */
		string model_tmp = HelperFunctions::readMatchFromFile("/proc/cpuinfo", "model name");
		if (model_tmp.length() > 0) {
			int beg = model_tmp.find(": ");
			if (beg != (int) string::npos)
				name = model_tmp.substr(beg + 1, model_tmp.length() - beg + 1);
				return 0;
		} 
		return -1;
	}

	/*
	 * On PowerNV platform we get model name in device tree.
	 * On pSeries we have to rely on static file.
	 */
	if (platform ==  PF_POWERKVM_HOST)
		return OpalgetCPUModelName(root, name);
	else
		return getCPUModelNameFromList(root, name);
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

	if( devName == "" )
	{
		string name;
		
		cout << "INSTALLED RESOURCE LIST";
		if( verbose )
			cout << " WITH VPD";
		cout << endl << endl;

		cout << "The following resources are installed on the machine." << endl;
		if( !verbose )
		{
			cout << "+/- = Added or deleted from Resource List." << endl;
			cout << "*   = Diagnostic support not available." << endl;
		}
		cout << endl;

		cout << "  Model Architecture: " << root->getArch( ) << endl;

		cout << "  Model Implementation: ";
		if( root->getCPUCount( ) > 1 )
			cout << "Multiple Processor";
		else
			cout << "Single Processor";
		cout << ", PCI Bus" << endl;;

		cout << "  Model Name: ";
		if (root->getMachineModel().length() != 0)
			cout << root->getMachineModel();
		/* Load model list - post processing addition of
		 * model info seems appropriate for such a simple
		 * lookup.  The alternative is to add a model name
		 * Dataitem to the Component class. */
		if (!getCPUModelName(root, name)) {
			/* PPC/PPC64/Cell */
			if (root->getMachineModel().length() != 0)
				cout  << ", ";

			cout << name;
		}
		cout << endl << endl;

		if( root->getDevTreeNode( ) != "" )
		{
			if( verbose )
			{
				cout << "  sys0                                   System Object" << endl;
				cout << "  sysplanar0                             System Planar" << endl;
			}
			else
			{
				cout << "+ sys0                                   System Object" << endl;
				cout << "+ sysplanar0                             System Planar" << endl;
			}
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
	char opts [] = "hvVDpl:d:z:";
	bool done = false, listdev = false;
	bool compressed = false;
	System * root = NULL;
	VpdRetriever* vpd = NULL;
	int index, first = 1;

	struct option longOpts [] =
	{
		{ "help", 0, 0, 'h' },
		{ "version", 0, 0, 'V' },
		{ "debug", 0, 0, 'D' },
		{ "verbose", 0, 0, 'v' },
		{ "specific", 0, 0, 'p' },
		{ "list", 1, 0, 'l' },
		{ "data", 1, 0, 'd' },
		{ "zip", 1, 0, 'z' },
		{ 0, 0, 0, 0 }
	};

	string platform = PlatformCollector::get_platform_name();

	switch (PlatformCollector::platform_type) {
	case PF_POWERKVM_PSERIES_GUEST:
	case PF_ERROR:
		cout<< argv[0] << " is not supported on the "
			<< platform << endl;
		return 1;
	default:
		;
	}

	if (geteuid() != 0) {
		cout << "Must be run as root!" << endl;
		return -1;
	}

	while( !done )
	{
		switch( getopt_long( argc, argv, opts, longOpts, &index ) )
		{
			case 'v':
				verbose = true;
				break;

			case 'V':
				printVersion( );
				return 0;

			case 'p':
				specific = true;
				break;

			case 'D':
				debug = true;
				break;

			case 'l':
				listdev = true;
				if( optarg != NULL )
					devName = optarg;
				break;

			case 'd':
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
				return -1;
		}
		first = 0;
	}

	if (specific && listdev) {
		// Unsupported combination of flags: -lp not to be used together.
		cout << "Unsupported combination of flags: -p not supported with -l." << endl;
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
			cout << "sbin/vpdupdate before running lscfg." << endl;
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
		if (!devFound) {
			cout << "Device " << devName << " not found." << endl;
			delete root;
			return 1;
		}
		if( specific )
			printSpecific( root );
		delete root;
	}

	if( compressed )
	{
		unlink( path.c_str( ) );
	}

	return 0;
}
