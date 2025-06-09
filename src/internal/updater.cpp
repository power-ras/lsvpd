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
#include <map>

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
#include <cstdlib>

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
int ensureEnv( const string& env, const string& file );
void archiveDB( const string& fullPath );
int __lsvpdInit( VpdDbEnv::UpdateLock *lock );
void __lsvpdFini(void);
void lsvpdSighandler(int sig);

const string DB_DIR( "/var/lib/lsvpd" );
const string DB_FILENAME( "vpd.db" );
const string BASE( "/sys/bus" );

/* Global variables for spyre.db access */
const string SPYRE_DB_FILENAME("spyre.db");
VpdDbEnv *spyreDb = nullptr;
VpdDbEnv::UpdateLock *spyreDbLock = nullptr;

bool isRoot(void);
VpdDbEnv *db;
VpdDbEnv::UpdateLock *dblock;

string env = DB_DIR, file = DB_FILENAME;

extern std::map<std::string, bool> g_deviceAccessible;

/**
 * @brief Cleans up resources allocated by __spyreDbInit()
 */
void __spyreDbFini()
{
       if (spyreDb != NULL) {
               try {
                       delete spyreDb;
                       spyreDbLock = NULL;
               } catch (VpdException & ve) {  }
       } /* if */
       spyreDb = NULL;
}

/**
 * @brief Cleanup Spyre-related files
 */
void cleanupSpyreFiles(const string& env)
{
        __spyreDbFini();

        string spyreDbPath = env + "/" + SPYRE_DB_FILENAME;
        if (access(spyreDbPath.c_str(), F_OK) == 0) {
                unlink(spyreDbPath.c_str());
        }

        string spyreLockPath = env + "/" + SPYRE_DB_FILENAME + "-updatelock";
        if (access(spyreLockPath.c_str(), F_OK) == 0) {
                unlink(spyreLockPath.c_str());
        }
}

int main( int argc, char** argv )
{
	char opts [] = "vahsp:";
	bool done = false;
	int index = 0, rc = 1;
	bool limitSCSISize = false;
	VpdDbEnv::UpdateLock *lock;
	string platform = PlatformCollector::get_platform_name();

	switch (PlatformCollector::platform_type) {
	case PF_PSERIES_KVM_GUEST: /* Fall through */
		rc = 0;
	case PF_NULL:	/* Fall through */
	case PF_ERROR:
		cout<< "vpdupdate is not supported on the " <<
			platform << " platform" << endl;
		return rc;
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
			lock = new VpdDbEnv::UpdateLock(env, file, false);
			archiveDB( env + '/' + file );
			delete lock;
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

	__lsvpdFini();
	cleanupSpyreFiles(env);
	return rc;
}

bool isRoot()
{
	return (geteuid() == 0);
}

/**
 * Method to remove old DB archives.
 */
void removeOldArchiveDB(void)
{
	int n, fp;
	struct dirent **namelist;
	Logger logger;

	n = scandir(env.c_str(), &namelist, NULL, alphasort);
	if (n <= 0) {
		ostringstream os;
		os << "Error scanning directory : " << env.c_str() << endl;
		logger.log( os.str( ), LOG_INFO );
		return;
        }

	for (int i = 0; i < n; i++)
	{
		string fname = string(namelist[i]->d_name);
		string pathname = env + "/" + fname;

		if ((fname.find("vpd.db.") == std::string::npos) &&
		    (fname.find(".gz") == std::string::npos)) {
			free(namelist[i]);
			continue;
		}
		unlink(pathname.c_str());
		free(namelist[i]);
	}

	fp = open(env.c_str(), O_RDWR);
	if (fp >= 0) {
		fsync(fp);
		close(fp);
	}

	free(namelist);
}

/**
 * Method moves the old db out of the way.
 */
void archiveDB( const string& fullPath )
{
	DIR * pDBdir = NULL;
	Logger logger;
	struct stat st;

	if( stat( fullPath.c_str( ), &st ) == 0 )
	{
		/*
		 * The old file is there, it needs to be timestamped,
		 * gzipped, and moved.
		 */
		struct dirent*  ent;
		pDBdir = opendir( env.c_str( ) );
		if (pDBdir == NULL) {
			ostringstream os;
			os << "Error opening directory " << env << endl;
			logger.log( os.str( ), LOG_WARNING );
			return;
		}

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

		if (link( fullPath.c_str( ), os.str( ).c_str( ) ) != 0) {
			cout << "Creating link of " <<  fullPath.c_str(  ) <<
			" to " <<os.str(  ).c_str(  ) << " failed" << endl;
			return;
		}
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
 * @brief Check if a component is a Spyre device
 */
bool isSpyreDevice(Component* comp)
{
       if (!comp)
               return false;

       string id = comp->getID();
       string deviceFile = id + "/device";
       ifstream deviceStream(deviceFile.c_str());
       if (deviceStream) {
               string deviceId;
               getline(deviceStream, deviceId);
               deviceStream.close();

               if (deviceId == "0x06a7" || deviceId == "0x06a8") {
                       Logger l;
                       l.log("Found Spyre device at: " + id, LOG_NOTICE);
                       return true;
               }
       }
       return false;
}

/**
 * @brief Extract spyre device data from existing vpd.db
 */
void extractSpyreData()
{
       if (spyreDb == NULL) {
               return;
       }

       string vpdDbPath = env + "/" + file;
       if (access(vpdDbPath.c_str(), F_OK) != 0) {
               return;
       }

       VpdDbEnv::UpdateLock* mainLock = new VpdDbEnv::UpdateLock(env, file, true);
       VpdDbEnv mainDb(*mainLock);

       vector<string> allKeys = mainDb.getKeys();
       for (const string& key : allKeys) {
               if (key.empty() || key == "/sys/bus") {
                       continue;
               }

               Component* comp = mainDb.fetch(key);
               if (comp) {
                       if (isSpyreDevice(comp)) {
                               spyreDb->store(comp);
                       }
                       delete comp;
               }
       }
}

/**
 * @brief Initializes spyre database access.
 * @return 0 on success, -1 on failure
 */
int __spyreDbInit()
{
       string spyreFullPath = env + "/" + SPYRE_DB_FILENAME;
       __spyreDbFini();

       if (access(spyreFullPath.c_str(), F_OK) == 0) {
               unlink(spyreFullPath.c_str());
       }

       spyreDbLock = new VpdDbEnv::UpdateLock(env, SPYRE_DB_FILENAME, false);

       spyreDb = new VpdDbEnv(*spyreDbLock);
       if (spyreDb == NULL)
               return -1;
       else
               return 0;
}

/**
 * Method does the initial population of the vpd db, this should only
 * be done once at boot time or any time that a user wishes to start with
 * a new db. And, handles spyre.db population with spyre devices.
 */
int initializeDB( bool limitSCSI )
{
	VpdDbEnv::UpdateLock *lock;
	System * root;
	int ret;

	if( ensureEnv( env, file ) != 0 )
		return -1;

	string fullPath = env + "/" + file;
	string spyreFullPath = env + "/" + SPYRE_DB_FILENAME;

	if (__spyreDbInit() != 0) {
		Logger l;
		l.log("Failed to initialize spyre database.", LOG_ERR);
		return -1;
	}

	if (access(fullPath.c_str(), F_OK) == 0) {
		Logger l;
		l.log("Extracting Spyre data from existing vpd.db", LOG_NOTICE);
		extractSpyreData();
	}

	lock = new VpdDbEnv::UpdateLock(env, file, false);
	removeOldArchiveDB( );
	archiveDB( fullPath );
	/* The db is now archived so when signal handler runs it should remove
	 * any db it finds */
	dblock = lock;

	Gatherer info( limitSCSI );
	ret = __lsvpdInit(lock);

	if ( ret != 0 ) {
		Logger l;
		l.log( "Could not allocate memory for the VPD database.", LOG_ERR);
		__spyreDbFini();
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
	}

	delete root;
	return ret;
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

int ensureEnv( const string& env, const string& file )
{
	struct stat info;
	int ret = -1;
	Logger logger;

	if( stat( env.c_str( ), &info ) == 0 )
	{
		if (!S_ISDIR(info.st_mode & S_IFMT)) {
			logger.log(env + " is not a directory\n", LOG_ERR);
			return ret;
		}

		if ( ((info.st_mode & S_IRWXU) != S_IRWXU) ||
		     ((info.st_mode & S_IRGRP) != S_IRGRP) ||
		     ((info.st_mode & S_IROTH) != S_IROTH) ) {
			logger.log("Failed to create " + file + ", no valid "
				"permission\n", LOG_ERR);
			return ret;
		}
		return 0;
	}

	int idx;
	if( ( idx = env.rfind( '/' ) ) == (int) string::npos )
	{
		return ret;
	}

	if( ( ret = ensureEnv( env.substr( 0, idx ) , env.substr( idx + 1 ) ) ) != 0 )
	{
		return ret;
	}

	if( mkdir( env.c_str( ),
		   S_IRUSR | S_IWUSR | S_IXUSR |
		   S_IRGRP | S_IWGRP | S_IXGRP |
		   S_IROTH | S_IXOTH ) != 0 )
	{
		logger.log( "Failed to create directory " + env + " for vpd db.", LOG_ERR );
		return -1;
	}
	return ret;
}

/** __lsvpdInit
 * @brief initializes data base access, sets up signal handling
 * to ensure proper cleanup if process if prematurely aborted
 */
int __lsvpdInit( VpdDbEnv::UpdateLock *lock )
{
	struct sigaction sigact;

	memset(&sigact, 0, sizeof(sigact));

	sigact.sa_handler = lsvpdSighandler;
	sigemptyset(&sigact.sa_mask);

	/* Set up signal handler for terminating signals;
	*/
	sigaction(SIGABRT, &sigact, NULL);
	sigaction(SIGILL, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	db = new VpdDbEnv( *lock );
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
			dblock = NULL;
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

		if (dblock != NULL) {
			/* Remove temporary file */
			unlink((env + "/" + file).c_str());
		}

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
