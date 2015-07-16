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

#include <vector>
#include <string>
#include <iostream>
#include <sstream>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for getopt_long
#endif

#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>

#include <libvpd-2/component.hpp>
#include <libvpd-2/system.hpp>
#include <libvpd-2/lsvpd.hpp>
#include <libvpd-2/debug.hpp>
#include <libvpd-2/logger.hpp>
#include <libvpd-2/vpddbenv.hpp>
#include <gatherer.hpp>
#include <devicetreecollector.hpp>
#include <platformcollector.hpp>

using namespace lsvpd;
using namespace std;

int initializeDB( bool limitSCSI );
int storeComponents( System* root, VpdDbEnv& db );
int storeComponents( Component* root, VpdDbEnv& db );
void printUsage( );
void printVersion( );
int ensureEnv( const string& env );
void archiveDB( const string& fullPath );
int __lsvpdInit(string env, string file);
void __lsvpdFini(void);
void lsvpdSighandler(int sig);

const string DB_DIR( "/var/lib/lsvpd" );
const string DB_FILENAME( "vpd.db" );
const string BASE( "/sys/bus" );

bool isRoot(void);
VpdDbEnv *db;

string env = DB_DIR, file = DB_FILENAME;

int main( int argc, char** argv )
{
	char opts [] = "vahsp:";
	bool done = false;
	string idNode;
	int index = 0;
	int rc;
	bool limitSCSISize = false;
	string platform = PlatformCollector::get_platform_name();

	switch (PlatformCollector::platform_type) {
	case PF_POWERKVM_PSERIES_GUEST:
	case PF_ERROR:
		cout<< "vpdupdate is not supported on the " << platform << endl;
		return 1;
	default:
		;
	}

	struct option longOpts [] =
	{
		{ "help", 0, 0, 'h' },
		{ "path", 1, 0, 'p' },
		{ "archive", 0, 0, 'a' },
		{ "version", 0, 0, 'v' },
		{ "scsi", 0, 0, 's' },
		{ 0, 0, 0, 0 }
	};

	while( !done )
	{
		switch( getopt_long( argc, argv, opts, longOpts, &index ) )
		{
			case 'p':
				env = optarg;
				index = env.rfind( '/' );
				file = env.substr( index + 1 );
				env = env.substr( 0, index );
				break;

			case 's':
				limitSCSISize = true;
				break;

			case 'v':
				printVersion( );
				return 0;

			case 'a':
				archiveDB( env + '/' + file );
				return 0;

			case -1:
				done = true;
				break;

			case 'h':
			default:
				printUsage( );
				return 0;
		}
	}

	/* Test to see if running as root: */
	if (!isRoot()) {
		cout << "vpdupdate must be run as root" << endl;
		return -1;
	}

	Logger l;

	l.log( "vpdupdate: Constructing full devices database", LOG_NOTICE );
	rc = initializeDB( limitSCSISize );
	if (rc) {
		__lsvpdFini();
		return rc;
	}

	__lsvpdFini();
}

bool isRoot()
{
	return (geteuid() == 0);
}

/**
 * Method moves the old db out of the way.
 */
void archiveDB( const string& fullPath )
{
	DIR * pDBdir = NULL;

	struct stat st;
	if( stat( fullPath.c_str( ), &st ) == 0 )
	{
		/*
		 * The old file is there, it needs to be timestamped,
		 * gzipped, and moved.
		 */
		struct dirent*  ent;
		pDBdir = opendir( env.c_str( ) );
		while( ( ent = readdir( pDBdir ) ) != NULL )
		{
			string fname = ent->d_name;
			string tag = "__" + file;
			if( fname.find( tag ) != string::npos )
			{
				unlink( fname.c_str( ) );
			}
		}
		closedir( pDBdir );

		ostringstream os;
		struct tm* timeval = NULL;
		os << fullPath << ".";

		if( ( timeval = localtime( &( st.st_mtime ) ) ) == NULL )
		{
			os << time( NULL );
		}
		else
		{
			os << timeval->tm_year + 1900 << "." << timeval->tm_mon + 1 << ".";
			os << timeval->tm_mday << "." << timeval->tm_hour << ".";
			os << timeval->tm_min << "." << timeval->tm_sec;
		}

		link( fullPath.c_str( ), os.str( ).c_str( ) );
		unlink( fullPath.c_str( ) );

		if( stat( os.str( ).c_str( ), &st ) == 0 )
		{
			char * buffer = new char[ st.st_size + 1 ];
			gzFile gzf = NULL;
			int tot = 0, in = 0, fd = -1;
			if( buffer == NULL )
			{
				cout << "Out of memory." << endl;
				goto ZDONE;
			}

			memset( buffer, 0, st.st_size + 1 );
			fd = open( os.str( ).c_str( ), O_RDONLY );
			if( fd < 0 )
			{
				delete [] buffer;
				cout << "Failed to open db file " << os.str( ) <<
					" for reading. " << endl;
				goto ZDONE;
			}

			// Read the original file into memory.
			while( ( in = read( fd, buffer + tot, st.st_size - tot ) ) > 0 &&
				tot < st.st_size )
				tot += in;
			close( fd );
			unlink( os.str( ).c_str( ) );

			os << ".gz";
			gzf = gzopen( os.str( ).c_str( ), "wb9" );
			if( gzf == NULL )
			{
				delete [] buffer;
				cout << "Error opening archive file " << os.str( ) <<
					" for writing." << endl;
				goto ZDONE;
			}

			in = 0;
			tot = 0;
			while( ( in = gzwrite( gzf, buffer + tot, st.st_size - tot ) ) > 0
				&& tot < st.st_size )
				tot += in;

			delete [] buffer;

			if( gzclose( gzf ) != 0 )
			{
				cout << "Failed to write compressed database, error = '" <<
					gzerror( gzf, &in ) << "'" << endl;
				goto ZDONE;
			}
ZDONE:;
		}
	}
}

/**
 * Method does the initial population of the vpd db, this should only
 * be done once at boot time or any time that a user wishes to start with
 * a new db.
 */
int initializeDB( bool limitSCSI )
{
	System * root;
	int ret;

	if( ensureEnv( env ) != 0 )
		return -1;

	string fullPath = env + "/" + file;

	archiveDB( fullPath );

	Gatherer info( limitSCSI );
	ret = __lsvpdInit(env, file);

	if ( ret != 0 ) {
		Logger l;
		l.log( " Could not allocate memory for the VPD database.", LOG_ERR);
		return ret;
	}

	root = info.getComponentTree( );

	/*
	coutd << "After Merge: " << endl;
	info.diplayInheritanceTree(root);
	*/

	ret = storeComponents( root, *db );

	if( ret != 0 )
	{
		Logger l;
		l.log( "Saving components to database failed.", LOG_ERR );
		return ret;
	}

	delete root;
	return 0;
}

/**
 * Recursively descend the component tree and store each in the db.
 */
int storeComponents( Component* root, VpdDbEnv& db )
{
	if( !db.store( root ) )
	{
		return -1;
	}

	vector<Component*>::const_iterator i, end;
	const vector<Component*> children = root->getLeaves( );
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		int ret = storeComponents( *i, db );
		if(  ret != 0 )
		{
			return ret;
		}
	}
	return 0;
}

int storeComponents( System* root, VpdDbEnv& db )
{

	if( !db.store( root ) )
	{
		return -1;
	}

	vector<Component*>::const_iterator i, end;
	const vector<Component*> children = root->getLeaves( );
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		int ret = storeComponents( *i, db );
		if(  ret != 0 )
		{
			return ret;
		}
	}
	return 0;
}

int ensureEnv( const string& env )
{
	struct stat info;
	int ret = 0;

	if( stat( env.c_str( ), &info ) == 0 )
	{
		return ret;
	}

	int idx;
	if( ( idx = env.rfind( '/' ) ) == (int) string::npos )
	{
		return ret;
	}

	if( ( ret = ensureEnv( env.substr( 0, idx ) ) ) != 0 )
	{
		return ret;
	}

	if( mkdir( env.c_str( ),
			S_IRUSR | S_IWUSR | S_IXUSR |
			S_IRGRP | S_IWGRP | S_IXGRP |
			S_IROTH | S_IXOTH ) != 0 )
	{
		Logger logger;
		logger.log( "Failed to create directory for vpd db.", LOG_ERR );
		return -1;
	}
	return ret;
}

/** __lsvpdInit
 * @brief initializes data base access, sets up signal handling
 * to ensure proper cleanup if process if prematurely aborted
 */
int __lsvpdInit(string env, string file)
{
	struct sigaction sigact;

	sigact.sa_handler = lsvpdSighandler;
	sigemptyset(&sigact.sa_mask);

	/* Set up signal handler for terminating signals;
	 */
	sigaction(SIGABRT, &sigact, NULL);
	sigaction(SIGILL, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	db = new VpdDbEnv( env, file, false );
	if ( db == NULL )
		return -1;
	else
		return 0;
}

/** __lsvpdFini
 * @brief Cleans up resources allocated by _lsvpdInit()
 */

void __lsvpdFini()
{
	if (db != NULL) {
		try {
			delete db;
		} catch (VpdException & ve) {  }
	} /* if */

	db = NULL;
}

void lsvpdSighandler(int sig)
{
	struct sigaction sigact;

	switch (sig) {
		case SIGABRT:
		case SIGILL:
		case SIGINT:
		case SIGQUIT:
		case SIGTERM:
			/* remove the handler for this action and raise
			 * the signal again so that the expected default
			 * behavior for the signal occurs.
			 */
			sigact.sa_handler = SIG_DFL;
			sigemptyset(&sigact.sa_mask);
			sigaction(sig, &sigact, NULL);
			__lsvpdFini();
			raise(sig);
			break;
	}
}

void printUsage( )
{
	string prefix( DEST_DIR );
	cout << "Usage: " << prefix;
	if( prefix[ prefix.length( ) - 1 ] != '/' )
	{
		cout << "/";
	}
	cout << "sbin/vpdupdate [options]" << endl;
	cout << "options:" << endl;
	cout << " --help,      -h     Prints this message." << endl;
	cout << " --version,   -v     Prints the version of vpd tools." << endl;
	cout << " --path=PATH, -pPATH Sets the path to the vpd db to PATH" << endl;
	cout << " --archive,   -a     Archives the current VPD database" << endl;
	cout << " --scsi,      -s     Limit size of SCSI device inquiry to 36 bytes" << endl;
}

void printVersion( )
{
	cout << "vpdupdate " << VPD_VERSION << endl;
}
