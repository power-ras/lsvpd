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

#include <icollector.hpp>

#include <libvpd-2/logger.hpp>
#include <libvpd-2/helper_functions.hpp>

#include <cerrno>
#include <sys/stat.h>
#include <sstream>
#include <fstream>
#include <cstring>

using namespace std;

namespace lsvpd
{

	/**
	 * Read a device attribute, given dev path and attribute name
	 * @var path Full path to device in sysfs
	 * @var attrName Name of file or link that contains the desired data
	 * @return Data contained in 'devPath'/'attrName'
	 */
	string ICollector::getAttrValue( const string& path,
								const string& attrName )
	{
		Logger logger;
		struct stat info;
		string fullPath;
		string ret = "";

		if( path == "" )
		{
			return ret;
		}

		ostringstream os;
		os << path << "/" << attrName;
		fullPath = os.str( );

		if( stat( fullPath.c_str( ), &info ) != 0 )
		{
			ostringstream os;
			if( errno != ENOENT )
			{
				os << "Error statting " << fullPath << " errno: " << errno;
				logger.log( os.str( ), LOG_ERR );
			}
			return ret;
		}

		ifstream attrIn;
		attrIn.open( fullPath.c_str( ) );

		if( attrIn )
		{
			char * strBuf;
			strBuf = new char [ info.st_size + 1 ];
			if( strBuf == NULL )
			{
				logger.log( string( "Out of memory." ), LOG_ERR );
				return ret;
			}

			memset( strBuf, '\0', info.st_size + 1 );

			attrIn.read( strBuf, info.st_size );
			ret = strBuf;
			attrIn.close( );
			delete [] strBuf;
		}

		return ret;
	}

	/*
	 * Read a binary blob from given @path and store it in *data.
	 * Allocates enough memory @*data, which has to be freed by the
	 * caller.
	 * @return : Size of the blob read.
	 */
	int ICollector::getBinaryData( const string& path, char **data )
	{
		int size = 0, rc = 0;
		struct stat info;
		char *buf;
		int fd = -1;

		if (stat(path.c_str(), &info) != 0)
		{
			*data = NULL;
			goto out;
		}

		fd = open(path.c_str(), O_RDONLY);
		if (fd < 0)
			goto out;

		buf = *data = new char [ info.st_size ];
		if (!*data)
			goto out;

		while(size < info.st_size)
		{
			rc = read(fd, buf + size, info.st_size - size);
			if (rc <= 0)
				break;
			size += rc;
		}

out:
		if (rc < 0)
			size = 0;
		if (fd >= 0)
			close(fd);

		if (size == 0 && *data)
		{
			delete [] *data;
			*data = NULL;
		}

		return size;
	}

	/* Generic field setter, allow field name to be passed in
	 * and data value to be stored in the correct place.
	 * @arg file: Source file of call, for simple back tracing.
	 * 		Should be set to __FILE__ at call point
	 * @arg lineNum: Source Line of call, for simple back tracing.
	 * 		Should be set to __LINE__ at call point
	 */
	void ICollector::setVPDField( Component* fillMe, const string& key,
		const string& val , char *file, int lineNum)
	{
		if( key == "EC" )
			fillMe->mEngChangeLevel.setValue( val, 90, file, lineNum );
		else if( key == "FN" )
			fillMe->mFRU.setValue( val, 90, file, lineNum );
		else if( key == "PN" )
			fillMe->mPartNumber.setValue( val, 90, file, lineNum );
		else if( key == "RL" )
			fillMe->mFirmwareLevel.setValue( val, 90, file, lineNum );
		else if( key == "RM" )
			fillMe->mFirmwareVersion.setValue( val, 90, file, lineNum );
		else if( key == "SN" )
			fillMe->mSerialNumber.setValue( val, 90, file, lineNum );
		else if( key == "MN" )
			fillMe->mManufacturerID.setValue( val, 90, file, lineNum );
		else if( key == "MF" )
			fillMe->mManufacturer.setValue( val, 90, file, lineNum );
		else if( key == "TM" )
			fillMe->mModel.setValue( val, 90, file, lineNum );
		else if( key == "FN" )
			fillMe->mFRU.setValue( val, 90, file, lineNum );
		else if( key == "FC" )
			fillMe->mFeatureCode.setValue( val, 90, file, lineNum );
		else if( key == "RT" )
			fillMe->mRecordType.setValue( val, 90, file, lineNum );
		else if( key == "YL" )
			fillMe->mPhysicalLocation.setValue( val, 90, file, lineNum );
		else if( key == "MI" )
			fillMe->mMicroCodeImage.setValue( val, 90, file, lineNum );
		else if( key == "SE" )
			fillMe->plantMfg.setValue( val, 90, file, lineNum );
		else if( key == "VK" )
			fillMe->mKeywordVersion.setValue( val, 90, file, lineNum );
		else if( key == "SZ" )
			fillMe->addDeviceSpecific( key, "Size", val, 90 );
		else if( key == "CC" )
			fillMe->addDeviceSpecific( key, "Customer Card ID Number",
				val, 90 );
		else if( key == "PR" )
			fillMe->addDeviceSpecific( key, "Power Control", val, 90 );
		else if ( key == "ML" )
			fillMe->addDeviceSpecific( key, "Microcode Level",
							val, 90 );
		else if ( key == "MG" )
			fillMe->addDeviceSpecific( key, "Microcode Build Date",
							val, 90 );
		else if ( key == "ME" )
			fillMe->addDeviceSpecific( key, "Update Access Key Exp"
						   " Date", val, 90 );
		else if( key[ 0 ] == 'U' )
			fillMe->addUserData( key, "User Data", val, 90, true );
		else if( key[ 0 ] == 'Z' )
			fillMe->addDeviceSpecific( key, "Device Specific", val, 90 );
		else if( key == "CL" )
			fillMe->addDeviceSpecific( key, "Firmware", val, 90 );
		else if( key[ 0 ] == 'C' )
			fillMe->addDeviceSpecific( key, "Device Specific", val, 90 );
		else if( key[ 0 ] == 'Y' )
			fillMe->addDeviceSpecific( key, "Device Specific", val, 90 );
		else if( key[ 0 ] == 'H' )
			fillMe->addDeviceSpecific( key, "Device Specific", val, 90 );
		else if( key[ 0 ] == 'B' )
			fillMe->addDeviceSpecific( key, "Device Specific", val, 90 );
		else if( key[ 0 ] == 'P' )
			fillMe->addDeviceSpecific( key, "Device Specific", val, 90 );

	}

	void ICollector::setVPDField( System* sys, const string& key,
		const string& val , char *file, int lineNum)
	{
		if( key == "BR" )
			sys->mBrand.setValue( val, 70, file, lineNum );
		else if( key == "OS" )
			sys->mOS.setValue( val, 70, file, lineNum );
		else if( key == "PI" )
			sys->mProcessorID.setValue( val, 70, file, lineNum );
		else if( key == "TM" )
			sys->mMachineType.setValue( val, 70, file, lineNum );
		else if( key == "FC" )
			sys->mFeatureCode.setValue( val, 70, file, lineNum );
		else if( key == "FG" )
			sys->mFlagField.setValue( val, 70, file, lineNum );
		else if( key == "RT" )
			sys->mRecordType.setValue( val, 70, file, lineNum );
		else if( key == "SU" )
			sys->mSUID.setValue( val, 70, file, lineNum );
		else if( key == "VK" )
			sys->mKeywordVersion.setValue( val, 70, file, lineNum );
	}
}
