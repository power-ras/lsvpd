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
#include <bitset>

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
	/* Convert a string to its hexDump */
	static string hexDump(char *arr, int len)
	{
		stringstream ss;
		int i = 0;

		ss << "0x";
		while(i < len)
		{
			unsigned long int var = arr[i];
			i++;
			bitset<8> set(var);
			ss << hex << set.to_ulong();
		}

		return ss.str();
	}

	/* Check if a char array contains printable symbols */
	static bool isPrintable(char *val, int len)
	{
		int i, j;
		bool nonSpace = false;

		for (i = 0; i < len;i++)
		{
			if (!isprint(val[i]))
				return false;
			if (!isspace(val[i]))
				nonSpace = true;
			/* Check if we have trailing 0's */
			j = i;
			while(++j < len && val[j] == '\0');
			/* After index i, we have all 0s' */
				if (j == len)
					break;
		}
		return nonSpace;
	}

	/**
	 * Sanitize a VPD value
	 * Some records have binary data that may not be captured
	 * as string. Convert such binary data to a string of its
	 * hexadecimal dump.
	 * Returns the original string it if can be represented as
	 * a string.
	 */
	string ICollector::sanitizeVPDField( char *val, int len )
	{
		if (isPrintable(val, len))
			return string(val);
		else
			return hexDump(val, len);
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
		else if( key == "SE" ) {
			sys->mSerialNum1.setValue( val, 70, file, lineNum );
			sys->mProcessorID.setValue( val, 70, file, lineNum );
		} else
			/* XXX: Un-recognized key */
			sys->addDeviceSpecific( key, "System Specific", val, 90 );
	}

	string ICollector::getFruDescription(string &key)
	{
		const char *fru_type = key.c_str();

		switch (fru_type[0]) {
		case 'A':
			switch (fru_type[1]) {
			case 'B':
				return string( "Combined AC  bulk power supply" );
			case 'M':
				return string( "Air mover" );
			case 'V':
				return string( "Anchor VPD" );
			}
			break;
		case 'B':
			switch (fru_type[1]) {
			case 'A':
				return string( "Bus adapter card" );
			case 'C':
				return string( "Battery charger" );
			case 'D':
				return string( "Bus/Daughter card" );
			case 'E':
				return string( "Bus expansion card" );
			case 'P':
				return string( "Backplane" );
			case 'R':
				return string( "Backplane riser" );
			case 'X':
				return string( "Backplane extender" );
			}
			break;
		case 'C':
			switch (fru_type[1]) {
			case 'A':
				return string( "Calgary bridge" );
			case 'B':
				return string( "Connector - Infiniband" );
			case 'C':
				return string( "Clock card" );
			case 'D':
				return string( "Card connector" );
			case 'E':
				return string( "Connector - Ethernet interface" );
			case 'L':
				return string( "Calgary PHB VPD" );
			case 'I':
				return string( "Interactive capacity card" );
			case 'O':
				return string( "Connector - SMA interface" );
			case 'P':
				return string( "Processor capacity card" );
			case 'R':
				return string( "Connector - RIO interface" );
			case 'S':
				return string( "Connector - Serial interface" );
			case 'U':
				return string( "Connector - USB interface" );
			}
			break;
		case 'D':
			switch (fru_type[1]) {
			case 'B':
				return string( "DASD Backplane" );
			case 'C':
				return string( "Drawer connector card" );
			case 'E':
				return string( "Drawer etension" );
			case 'I':
				return string( "Drawer interposer" );
			case 'L':
				return string( "P7 IH D-link connector" );
			case 'T':
				return string( "Legacy PCI daughter card" );
			case 'V':
				return string( "Media drawer LED" );
			}
			break;
		case 'E':
			switch (fru_type[1]) {
			case 'I':
				return string( "Enclosure LED" );
			case 'F':
				return string( "Enclosure fault LED" );
			case 'S':
				return string( "Embedded SAS" );
			case 'T':
				return string( "Ethernet riser card (no GX bus)" );
			case 'V':
				return string( "Enclosure VPD" );
			}
			break;
		case 'F':
			switch (fru_type[1]) {
			case 'M':
				return string( "Frame" );
			}
			break;
		case 'H':
			switch (fru_type[1]) {
			case 'B':
				return string( "Host bridge RIO to PCI card" );
			case 'D':
				return string( "High speed daughter card" );
			case 'M':
				return string( "HMC connector" );
			}
			break;
		case 'I':
			switch (fru_type[1]) {
			case 'B':
				return string( "IO backplane" );
			case 'C':
				return string( "IO card" );
			case 'D':
				return string( "IDE connector" );
			case 'I':
				return string( "IO drawer enclosure LEDs" );
			case 'P':
				return string( "Interplane card" );
			case 'S':
				return string( "SMP V-Bus interconnection cable" );
			case 'T':
				return string( "Enclosure interconnection cable" );
			case 'V':
				return string( "IO drawer enclosure VPD" );
			}
			break;
		case 'K':
			switch (fru_type[1]) {
			case 'V':
				return string( "Keyboard video mouse LED" );
			}
			break;
		case 'L':
			switch (fru_type[1]) {
			case '2':
				return string( "Level 2 cache module/card" );
			case '3':
				return string( "Level 3 cache module/card" );
			case 'C':
				return string( "Squadrons H llight strip connector" );
			case 'R':
				return string( "P7 IH L-link connector" );
			case 'O':
				return string( "System locate LED" );
			case 'T':
				return string( "Squadrons H light strip" );
			}
			break;
		case 'M':
			switch (fru_type[1]) {
			case 'B':
				return string( "Media backplane" );
			case 'E':
				return string( "Map extension" );
			case 'M':
				return string( "MIP meter" );
			case 'S':
				return string( "Main store card or DIMM" );
			}
			break;
		case 'N':
			switch (fru_type[1]) {
			case 'B':
				return string( "NVRAM battery" );
			case 'C':
				return string( "Service processor node controller" );
			case 'D':
				return string( "NUMA DIMM" );
			}
			break;
		case 'O':
			switch (fru_type[1]) {
			case 'D':
				return string( "CUoD card" );
			case 'P':
				return string( "Operator panel" );
			case 'S':
				return string( "Oscillator" );
			}
			break;
		case 'P':
			switch (fru_type[1]) {
			case '2':
				return string( "IOC chip and its devices" );
			case '5':
				return string( "IOC/IOC2 PCI bridge" );
			case 'B':
				return string( "IO drawer main backplane" );
			case 'C':
				return string( "Power capacitor" );
			case 'D':
				return string( "Processor card" );
			case 'F':
				return string( "Processor FRU" );
			case 'I':
				return string( "IOC/IOC2 PHB" );
			case 'O':
				return string( "SPCN" );
			case 'N':
				return string( "SPCN connector" );
			case 'R':
				return string( "PCI riser card" );
			case 'S':
				return string( "Power supply" );
			case 'T':
				return string( "Pass-through card" );
			case 'X':
				return string( "PSC power sync card" );
			case 'W':
				return string( "Power connector" );
			}
			break;
		case 'R':
			switch (fru_type[1]) {
			case 'G':
				return string( "Regulator" );
			case 'I':
				return string( "Riser" );
			case 'K':
				return string( "Rack indicator connector" );
			case 'W':
				return string( "RiscWatch connector" );
			}
			break;
		case 'S':
			switch (fru_type[1]) {
			case 'A':
				return string( "System attention LED" );
			case 'B':
				return string( "Backup system VPD" );
			case 'C':
				return string( "SCSI connector" );
			case 'D':
				return string( "SAS connector" );
			case 'I':
				return string( "SCSI to IDE converter" );
			case 'L':
				return string( "PHB slot - PHB child" );
			case 'P':
				return string( "Service processor" );
			case 'R':
				return string( "Service interconnection card" );
			case 'S':
				return string( "Soft switch" );
			case 'V':
				return string( "System VPD" );
			case 'Y':
				return string( "Legacy system VPD FRU type" );
			}
			break;
		case 'T':
			switch (fru_type[1]) {
			case 'D':
				return string( "Time of Day clock" );
			case 'I':
				return string( "Torrent PCIE PHB" );
			case 'L':
				return string( "Torrent riser PCIE PHB slot" );
			case 'M':
				return string( "Thermal sensor" );
			case 'P':
				return string( "TPMD adapter" );
			case 'R':
				return string( "Torrent octant PCIE bridge" );
			}
			break;
		case 'V':
			switch (fru_type[1]) {
			case 'V':
				return string( "Root node VPD" );
			}
			break;
		case 'W':
			switch (fru_type[1]) {
			case 'D':
				return string( "Water device" );
			}
			break;
		}
		return string( "Unknown" );
	}
}
