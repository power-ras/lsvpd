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

/* Firmware version information from the RTAS */
string rtas_pfw;
/* Firmware version in MicroCodeImage ('MI') record */
string rtas_fw_t, rtas_fw_p, rtas_fw_b;
/* Firmware version in new MicroCodeLevel ('ML') record */
string rtas_fwl_t, rtas_fwl_p, rtas_fwl_b;

/* Firmware version information from the VPD db */
string db_pfw;
string db_fw_t, db_fw_p, db_fw_b;
string db_fwl_t, db_fwl_p, db_fwl_b;

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

static void getRtasFirmwareLevel()
{
	char name[256];
	char key[3];
	char value[256];
	char firmware[256],  pfw[256];
	char *rtasData, *buf, *section;
	char *end;
	string fw;
	int pos1, pos2;
	unsigned char type;
	int rtasDataSize, rlen, res = 0, size;
	int len;

	rtasDataSize = RtasCollector::rtasGetVPD("", &rtasData);
	if (rtasDataSize < 0)
		goto error;

	end = rtasData + rtasDataSize;
	section = rtasData;
	/*
	 * The format of the VPD Data is a series of 
	 * sections, with each section containing a header
	 * followed by one or more records as shown below:
	 *
	 *   _ section(1) start
	 *  |
	 *  v
	 *  ---------------------------------------------------------------
	 *  | size(4) |type(1)|name_len(2)| section name(name_len)| pad(3) |
	 *  ---------------------------------------------------------------
	 *  |key(2) |record_len(1)|record (record_len) | key(2) |..........|
	 *  ---------------------------------------------------------------
	 *  |....|size(4)|type(1)|name_len(2)| section name(name_len) |pad(|
	 *  ---------------------------------------------------------------
	 *       ^
	 *       |
	 *       - section (2) start.
	 */

	while (section < end)
	{
		/* Read the section size */
		memcpy(&size, section, 4);
		section += 4;
		buf = section;

		if (section + size > end)
			goto error;

		section += size;		/* Move to next section */
		type = *buf;
		buf++;
		if (type == 0x82)
		{
			/* length is 2 bytes big endian */
			len = *buf;
			buf++;
			len += ((*buf) << 8);
			buf++;

			memset(name, 0, 256);
			memcpy(name, buf, len);
			buf += len;


			/* Ignore the other section */
			if (strcmp(name, "System Firmware"))
				continue;

			buf += 3;		/* Skip 3 bytes  padding */
		} else	{
			cerr << "Found unknown section (type 0x" << hex << (unsigned int)type << ") in RTAS VPD\n";
			continue;
		}

		while (buf < section &&  *buf != 0x78 && *buf != 0x79)
		{
			memset(name, 0, 256);
			memset(key, 0, 3);

			if (buf + 3 > section)
				goto error;

			key[0] = buf[0];
			key[1] = buf[1];
			key[2] = '\0';

			rlen = buf[2];
			buf += 3;

			if (buf + rlen  > section)
				goto error;

			if (!strcmp(key, "MI")) {

				memset(firmware, 0, rlen + 1);
				memcpy(firmware, buf, rlen);
				fw = string(firmware);

				pos1 = fw.find( ' ' );
				pos2 = fw.rfind( ' ' );

				rtas_fw_t = fw.substr(0, pos1);
				rtas_fw_p = fw.substr(pos1 + 1, (pos2 - pos1) - 1);
				rtas_fw_b = fw.substr(pos2 + 1);
			} else if (!strcmp(key, "ML")) {

				memset(firmware, 0, rlen + 1);
				memcpy(firmware, buf, rlen);
				fw = string(firmware);

				pos1 = fw.find( ' ' );
				pos2 = fw.rfind( ' ' );

				rtas_fwl_t = fw.substr(0, pos1);
				rtas_fwl_p = fw.substr(pos1 + 1, (pos2 - pos1) - 1);
				rtas_fwl_b = fw.substr(pos2 + 1);
			}


			if (!strcmp(key, "CL") && !strncmp(buf, "PFW", 3)) {
				memset(pfw, 0, rlen);
				memcpy(pfw, buf +  4, rlen - 4);
				rtas_pfw = string(pfw);
			}

			buf += rlen;
		}

		/* 
		 * When we reach here we have gone through the 'System Firmware'
		 * section.
		 */
		break;
	}

	return;	
error:
	cerr << "Error in parsing the RTAS VPD\n";
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

				db_fw_t = mi.substr(0, pos1);
				db_fw_p = mi.substr(pos1 + 1, (pos2 - pos1) - 1);
				db_fw_b = mi.substr(pos2 + 1);

				if (rtas_fw_t.empty())
					rtas_fw_t = db_fw_t;
				if (rtas_fw_p.empty())
					rtas_fw_p = db_fw_p;
				if (rtas_fw_b.empty())
					rtas_fw_b = db_fw_b;

			}

			if ( ml != "" )
			{
				int pos1 = ml.find( ' ' );
				int pos2 = ml.rfind( ' ' );

				db_fwl_t = ml.substr(0, pos1);
				db_fwl_p = ml.substr(pos1 + 1, (pos2 - pos1) - 1);
				db_fwl_b = ml.substr(pos2 + 1);

				if (rtas_fwl_t.empty())
					rtas_fwl_t = db_fwl_t;
				if (rtas_fwl_p.empty())
					rtas_fwl_p = db_fwl_p;
				if (rtas_fwl_b.empty())
					rtas_fwl_b = db_fwl_b;

			}

			if (rtas_fwl_t != "" )
			{
				/* 
				 * When we have both ML and MI records, the output
				 * is in the following format.
				 * ML_t (MI_t) (t) ML_p (MI_p) (p) ML_b (MI_b) (b)
				 */
				fw = rtas_fwl_t;
				if (rtas_fw_t != "")
					fw +=  " (" + rtas_fw_t + ") "; 
				fw += "(t) " + rtas_fwl_p;
				if (rtas_fw_p != "")
					fw += " (" + rtas_fw_p + ") ";
				fw += "(p) " + rtas_fwl_b;
				if (rtas_fw_t != "")
					fw += " (" + rtas_fw_b + ") ";
				fw += "(b)";
			} else {

				/* Old style */
				fw = rtas_fw_t + " (t) " + rtas_fw_p + " (p) " +
					rtas_fw_b + " (b)";
			}


			if( all )
				cout << "sys0!system:";
			else
				cout << "Version of System Firmware is ";

			cout << fw ;

			if( all )
				cout << "|";
			else
				cout << endl;

			vector<DataItem*>::const_iterator j, stop;
			for( j = c->getDeviceSpecific( ).begin( ),
				stop = c->getDeviceSpecific( ).end( ); j != stop; ++j )
			{
				string val = (*j)->getValue( );
				if( val.find( "PFW" ) != string::npos )
				{
					db_pfw = val.substr( 4 );
					if (rtas_pfw.empty())
						rtas_pfw = db_pfw;
					break;
				}
			}

			if( all )
				cout << "service:";
			else if( rtas_pfw != "" )
				cout << "Version of PFW is ";

			cout << rtas_pfw << endl;
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
	int index;
	string platform = PlatformCollector::get_platform_name();

	switch (PlatformCollector::platform_type) {
	case PF_POWERKVM_PSERIES_GUEST:
	case PF_ERROR:
		cout<< "lsmcode is not supported on the " << platform << endl;
		return 1;
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
				break;

			case 'h':
				printUsage( );
				return 0;
			default:
				printUsage( );
				return 1;
		}
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
				cout << "Faile to open database archive " << path << endl;
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
			cout << "sbin/vpdupdate before running lsmcode." << endl;
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
	if (PlatformCollector::platform_type != PF_POWERKVM_HOST)
		getRtasFirmwareLevel();

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
