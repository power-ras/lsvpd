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

/* General SG_UTILS Notes and code comments:
 * sysfstreecollector.cpp calls into fillSCSIComponent below, which
 * kicks of a series of ioctl and sgutils calls.  This code is responsible
 * for doing all necessary direct to device querying, pulling all the fields
 * described in the template described by 'struct scsi_templates'.
 *
 * sg_utils notes: The doSGQuery is the heart of this code, abstracting
 * away the nastiness of the sg_ll call.  Several calls to sg_ll_inquiry
 * are required, the first to get us a size flag for the expected return,
 * the second to pass in this size arg as the bufSize arg, and obtain the
 * data we desire.  This is an odd approach and one I initially avoided,
 * however, the buffer obtained with a single call is not the same as that
 * obtained thru the second call.  Thus, I have to assume the bufSize arg
 * is actually a flag of some sort, modifying buffer obtained.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libvpd-2/helper_functions.hpp>
#include <libvpd-2/debug.hpp>
#include <libvpd-2/lsvpd.hpp>
#include <libvpd-2/lsvpd_error_codes.hpp>
#include <libvpd-2/vpdexception.hpp>
#include <libvpd-2/logger.hpp>

#include <sysfstreecollector.hpp>

#include <sstream>

#include <iostream>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <scsi/scsi.h>
#include <sys/types.h>
#include <linux/types.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>

extern "C"
{
#include <scsi/sg_cmds.h>
}

// Collection of definitions needed globally
inline static string _errmsg(string line, string file, string str)
{
	ostringstream os;
	os << RED << file << " [" << BLUE << line << RED << "]: " << GREEN << str << NC;
	return os.str( );
}

#define errmsg(str) _errmsg(TOSTRING(__LINE__), __FILE__, str)

using namespace std;

namespace lsvpd
{

	static const string SCSI_CDROM_DEFAULT ( "Other SCSI CD-ROM Drive" );

	struct scsi_template {
		string vendor;
		string devClass;
		string model;
		string format_str; //The format followed by ioctl data string
	};

	struct intStr {
		int key;
		string val;
	};

	struct strStr {
		string key;
		string val;
	};

	static vector<scsi_template*> scsi_templates;

	typedef struct my_scsi_idlun {
		int dev_id;
		int host_unique_id;
	} My_scsi_idlun;

	static int scsi_template_count = 0;

	static const struct strStr ata_device_renaming_scheme[] =
	{
		{"IBM", "IBM"},
		{"QUANTUM", "Quantum"},
		{"ST", "Seagate"},
		{"UJDA", "Matshita"},
		{"HL-DT-ST", "LG Electronics"},
		{"LG", "LG Electronics"},
		{"TOSHIBA", "Toshiba"},
		{"LTN", "Lite-On"},
		{"AOPEN", "Aopen"},
		{"RICOH", "Ricoh"},
		{"NEC", "NEC"},
		{"MAXTOR", "Maxtor"},
		{"HTS", "Hitachi"},
		{"IC25N", "Hitachi"},
		{"PIONEER", "Pioneer"},
		{"", ""}
	};

	static const struct intStr cdrom_device_types[] =
	{
		{0, SCSI_CDROM_DEFAULT},
		{1, "OEM SCSI CD-ROM Drive"},
		{2, "SCSI CD-ROM Drive"},
		{3, "SCSI Multimedia CD-ROM Drive"},
		{4, "OEM SCSI DVD-ROM Drive"},
		{5, "SCSI DVD-ROM Drive"},
		{6, "SCSI DVD-RAM Drive"},
		{7, "OEM SCSI DVD-RAM Drive"},
		{-1, ""}
	};

	/*
	 * Very vaguely taken from linux-2.5.22/drivers/scsi/scsi.[ch]
	 * There are only really 14 possible type, but filling it out to 16
	 * removes the need for error checking.
	 */
	static const struct intStr device_scsi_types_short[] =
	{
		{0x0, "disk"},
		{0x1, "tape"},
		{0x2, "printer"},
		{0x3, "processor"},
		{0x4, "worm"},
		{0x5, "cdrom"},
		{0x6, "scanner"},
		{0x7, "optical-disk"},
		{0x8, "jukebox"},
		{0x9, "communications"},
		{0xa, "graphics"},
		{0xb, "graphics"},
		{0xd, "enclosure"},
		{-1, 	""}
	};

	static const struct intStr device_scsi_types[] =
	{
		{0x0, "Disk Drive"},
		{0x1, "Tape Drive"},
		{0x2, "Printer Device"},
		{0x3, "Processor Device"},
		{0x4, "Write-once Device"},
		{0x5, "CD-ROM Drive"},
		{0x6, "Scanner Device"},
		{0x7, "Optical Memory Device"},
		{0x8, "Medium Changer Device"},
		{0x9, "Communications Device"},
		{0xa, "Graphics Arts Pre-press Device"},
		{0xb, "Graphics Arts Pre-press Device"},
		{0xd, "Enclosure Services Device"},
		{-1, ""}
	};

	struct strStr device_scsi_ds_prefixes[] = {
		{ DEVICE_TYPE_SCSI,		"SCSI"            },
		{ DEVICE_TYPE_SATA,		"ATA"             },
		{ DEVICE_TYPE_ATA,		"ATA"             },
		{ DEVICE_TYPE_ISCSI,	"iSCSI"           },
		{ DEVICE_TYPE_FIBRE,	"Fibre Channel"   },
		{ DEVICE_TYPE_RAID,		"SCSI"            },
		{ DEVICE_TYPE_SCSI_DEBUG,"SCSI Debug Fake" },
		{ DEVICE_TYPE_IBMVSCSI,	"Virtual SCSI"    },
		{ DEVICE_TYPE_USB,		"USB"             },
		{ "",                   ""              },
	};

	void printBuf(char *buf, int size)
	{
#ifdef TRACE_ON
		printf("Printing Buf @%p [%d bytes]\n", buf, size);
		printf(">>> '");
		for (int k=0; k < size; k++) {
			printf("%c", buf[k]);
		}
		printf("' <<<");
		printf("\n <<<<<<<<<<< \n");
#endif
	}

	/**
	 * make_basic_device_scsi_ds()
	 * Requires that fillMe-> devBus be filled
	 */
	string SysFSTreeCollector::make_basic_device_scsi_ds(Component *fillMe,
							     string type, int subtype)
	{
		string ds;
		int i, j;
		string prefix;
		string subtypeDS;

		if (!fillMe->devBus.getValue().empty()) {
			i = 0;
			while ((device_scsi_ds_prefixes[i].key != "") &&
			       (type != device_scsi_ds_prefixes[i].key))
				i++;

			prefix = device_scsi_ds_prefixes[i].val;
		}
		if (prefix.empty())
			prefix = "SCSI";

		i = 0;
		// Lookup device subtype
		while ((device_scsi_types[i].key != -1)
		       && (subtype != device_scsi_types[i].key))
			i++;

		if (device_scsi_types[i].key != -1)
			subtypeDS = device_scsi_types[i].val;

		j = 0;
		if ((device_scsi_types[i].key == -1)
		    && (!cdrom_device_types[j].key != -1))
			subtypeDS = cdrom_device_types[j].val;
		else if (device_scsi_types[i].key == -1)
			subtypeDS = "Unknown Device";

		/*
		 * When we get 2 special cases we will generalise this using a
		 * sparse lookup-table like the above string array, but keyed
		 * off both type and subtype.
		 */
		if ((prefix == "usb") && (subtypeDS == "Disk Drive")) {
			subtypeDS = "Mass Storage Device";
		}

		return (prefix + " " + subtypeDS);
	}

	/**
	 * make_full_scsi_ds()
	 * Requires fillMe-> devBus
	 * Note: Can only be called for evpd retrieved from page code 199
	 */
	string SysFSTreeCollector::make_full_scsi_ds(Component *fillMe,
						     string type, int subtype,
						     char * data, int dataSize)
	{
		string subtypeDS;
		int cdtype;
		string ds;
		int i = 0;

		ds = make_basic_device_scsi_ds(fillMe, type, subtype);
		/*cout << "Type = " << type << " subtype = " << subtype
		  << endl;
		  printf("data[34] =  %d\n", data[34]);*/

		// Only do the fancy stuff for real SCSI devices.
		if ((type ==  "scsi")
		    || (type ==  "raid")) {
			// Check for SCSI *Multimedia* CD-ROM Drive.
			if (subtype == 5) {
				cdtype = ((NULL != data) &&
					  (34 <= strlen(data))) ?
					(int) data[33] : 0;

				// Find CDROM type string
				while ((cdrom_device_types[i].key != -1) &&
				       (cdtype != cdrom_device_types[i].key))
					i++;

				if (!cdrom_device_types[i].key != -1)
					subtypeDS = cdrom_device_types[i].val;
				else
					subtypeDS = SCSI_CDROM_DEFAULT;

			}
			else {
				if (subtype == 0 && (81 == data[34])) {
					subtypeDS = "LVD " + ds;
					//                    label = ((0 == subtype) && (NULL != data)
					//                        && (7 == data[34])) ?
				}

			}

		}

		if (subtypeDS.empty()) {
			return ds;
		}
		else {
			return subtypeDS;
		}
	}

	/**
	 * Takes two strings and finds if the two are equivalent.
	 * s1 can contain '*', meaning zero or more of anything can be
	 * counted as a match
	 */
	bool matches(string s1, string s2)
	{
		int beg = 0, end = s1.length();
		int z;

		//strings have matched to end - base case
		if (s1 == s2)
			return true;

		//base case
		if (s1.length() == 0) {
			if (s2 == "*")
				return true;
			else
				return false;
		}

		//base case
		if (s2.length() == 0) {
			if (s1 == "*")
				return true;
			else
				return false;
		}

		//s1 has matched all the way up to a * - base case
		if (s1 == "*") {
			return true;
		}

		if (s1[0] == '*') {
			/*
			 * Need to grab a substring of s2, up to size of s1 from
			 * star to any other star
			 */
			beg = 1;
			end = s1.length();

			if ( (int) string::npos == (end = s1.find("*", beg))) {
				//No more stars - base case
				if ((int) string::npos != (z = s2.find(s1.substr(beg, end), 0))) {
					return true;
				}
			}
			else {
				if (string::npos != s2.find(s1.substr(beg, end), 0))
					matches(s1.substr(end, s1.length() - end),
						s2.substr(end, s2.length() - end));
			}
		}
		else
			end = s1.find("*", 0);

		if (s1.substr(beg, end) == s2.substr(beg, end)) {
			return matches(s1.substr(end, s1.length() - end),
				       s2.substr(end, s2.length() - end));
		}
		else {
			return false;
		}
	}

	/**
	 * Attempts to match a template specification to device discovered
	 * values.
	 */
	const scsi_template *findTemplate(string vendor,
					  string devClass, string model)
	{
		vector<scsi_template*>::iterator i, end;

		for ( i = scsi_templates.begin( ), end = scsi_templates.end( ); i != end; ++i ) {
			if (!matches((*i)->vendor, vendor))
				continue;
			if (!matches((*i)->devClass, devClass))
				continue;
			if (!matches((*i)->model, model))
				continue;

			return *i;
		}

		return NULL;
	}

	string lsvpd_hexify(const unsigned char * str,
			    size_t len)
	{
		int i;
		string tmp;

		char * ret = new char[ len*2+1 ];
		if ( ret == NULL )
			return ret;
		for (i = 0; i < (int) len ; i++) {
			snprintf(&ret[i*2], 3, "%.2X", str[i]);
		}

		tmp = ret;
		delete []ret;
		return tmp;
	}

	/**
	 * Take a binary arg and convert it to hex
	 */
	string hexify(string str_t)
	{
		int i;
		char *str = (char *) str_t.c_str();
		char *ret;

		if (str_t.length() == 0)
			return str_t;

		ret = new char[ (sizeof(char) * str_t.length() * 2) + 1 ];
		if ( ret == NULL )
			return ret;

		for (i = 0; i < (int) str_t.length(); i++) {
			snprintf(&ret[i*2], 3, "%.2X", str[i]);
		}

		str_t = ret;

		delete[] ret;

		return str_t;
	}

	/**
	 * @breif: Walk a template, returning the name of the field
	 */
	string getFieldName(string pattern)
	{
		int i, eon; //End of name field

		if (pattern.length() <= 0)
			return "";

		i = 0;
		while ((i < (int) pattern.length()) && (pattern[i] != ':'))
			i++;

		if (i >= (int) pattern.length())
			return "";

		eon = i;
		while (pattern[eon] == ',')
			eon--;

		return pattern.substr(0, eon);
	}

	/**
	 * @brief: Grab int value from template, representing the size of the
	 *	   field in bytes
	 */
	int getFieldValue(string pattern)
	{
		int i, eon, value; //End of name field
		string str;
		char **endptr = NULL;

		if ((int) pattern.length() <= 0)
			return -1;

		i = 0;
		while ((i < (int) pattern.length()) && (pattern[i] != ':'))
			i++;

		if (i >= (int) pattern.length())
			return -1;

		eon = i + 1;
		while (pattern[eon] == ',')
			eon--;

		str = pattern.substr(eon, pattern.length() - eon);
		value = strtol(str.c_str(), endptr, 0);
		//cout << "GetValue() Pattern = " << pattern << ", str = "
		//<< str << ", eon = " << eon << "Value = " << value << endl;

		return value;
	}

	/**
	 * Retrieves a specified field in a comma-delimited list
	 * @arg: num: Number of field to grab, starting with 0
	 */
	string retrieveField(char * data, int num)
	{
		int commaCount = 0;
		int beg, end, curPos;
		string str = data;

		end = beg = curPos = 0;

		//coutd << "Data: " << data << ", Len = " << strlen(data) <<  endl;

		while ((commaCount <= num) && (data[curPos] != '\0')) {
			if (str[curPos] == ',') {
				commaCount++;
				if (commaCount <= num)
					beg = curPos + 1;
				else
					end = curPos;
			}
			if (str[curPos + 1] == '\0') {
				end = curPos;
			}
			curPos++;

		}

		/*
		 * Exceptions:  [02]IBM seems to be messing up
		 *  - Note: Be as specific as possible here
		 */
		if (beg >= end)
			return "";

		while ((str[beg] == 2) && (beg < (int) strlen(data)) && (beg < end))
			beg++;

		str = str.substr(beg, end - beg);

		return str;
	}

	/**
	 * Takes a full format specifier template string, counts number of
	 *	page codes within it
	 *	Ex: ?0x0=RL:4,_:78,FN:12,EC:10,PN:12;?0x83=_:4,UM:8;
	 *		returns 2
	 */
	int numPageTemplates(string templ)
	{
		int count = 0, i = 0;

		if (templ.length() == 0)
			return 0;

		while (i < (int) templ.length())
			if (templ[i++] == '?') {
				count++;
			}

		return count;
	}

	/**
	 * Takes a full format specifier template string, and parses it for
	 * the page-specific format code specified by num.
	 * For example, suppose the template string is:
	 *		?0x0=RL:4,_:78,FN:12,EC:10,PN:12;?0x83=_:4,UM:8;
	 *	retrievePageTemplate(templ, 0) = 0x0=RL:4,_:78,FN:12,EC:10,PN:12;
	 *	retrievePageTemplate(templ, 1) = 0x83=_:4,UM:8;
	 */
	string retrievePageTemplate(string templ, int num)
	{
		int beg = -1, end = templ.length();
		int i = 0, count = 0;

		if (num >= numPageTemplates(templ))
			return "";

		while (i < (int) templ.length() && end == (int) templ.length()) {
			if (templ[i] == '?' && beg != -1) {
				end = i;
			}

			if (templ[i] == '?') {
				count++;  //PageTemplate count
				if (count == num + 1) {
					beg = i;
				}
			}

			i++;
		}

		//For case of no more page-specific strings
		if (end == 0)
			end = i;
		return templ.substr(beg, end - beg);
	}

	/**
	 * Takes a page specific template, ie '0x83=_:4,UM:8;' and parses out
	 *	page code and format string for this page of data
	 *
	 *  @return the page code '83' as 'page_code',
	 *		format string '_:4,UM:8;' as format
	 */
	int retrievePageCode(const string page_spec_template,
			     string& page_code, string& format)
	{
		int i = 0, beg, end = 0;

		/* Get page code */
		while (page_spec_template[i-1] != '0'
		       && page_spec_template[i] != 'x'
		       && i < (int) page_spec_template.length())
			i++;
		beg = i + 1;
		while (page_spec_template[i] != '=' && i < (int) page_spec_template.length())
			i++;
		end = i;
		if ((beg >= (int) page_spec_template.length())
		    || (end >= (int) page_spec_template.length()))
			return -1;
		page_code = page_spec_template.substr(beg, end - beg);

		/* Get format string */
		beg = end + 1;
		while (page_spec_template[i] != ';' && i < (int) page_spec_template.length())
			i++;
		end = i;
		format = page_spec_template.substr(beg, end - beg);return 0;
	}

	/**
	 * Takes a char * pointer, removing all spaces before and after,
	 * then copying up fieldSize bytes with Null termination added
	 */
	string strdupTrim(char *buf, int maxLen)
	{
		int beg, end;
		string result;
		char *tmp;

		beg = 0;
		while	(beg < maxLen &&
			 (buf[beg] == 32 || buf[beg] == '\n'))
			beg++;

		end = beg + maxLen;
		while	(end > beg && buf[end] == 32)
			end--;

		tmp = strndup(buf + beg, end - beg);
		if (!tmp)
			return NULL;

		result = string(tmp);
		free(tmp);
		return result;
	}

	/**
	 * Takes a template reference number along with raw data to be translated
	 * according to the template
	 * @arg : deviceType here is looked up based on subtype into the
	 * device_scsi_types_short[] table.  This is different than type!
	 */
	void SysFSTreeCollector::process_template(Component *fillMe,
						  string *deviceType,
						  char *data, int dataSize,
						  string *format,
						  int pageCode)
	{
		int eof[30]; // End of field pointers
		int fieldTotal = 0;
		int fieldNum = 0;
		int tempCurLoc, dataCurLoc;
		string fieldTemplate;
		string fieldName;
		int fieldSize;
		string dataVal;  //Data as read from data stream for single field
		string str;

		if (pageCode == 0)
			dataCurLoc = 32; // Skip ll_inquiry standard header for base inquiry
		else
			dataCurLoc = 0;

		for (int i = 0; i < (int) (*format).length(); i++)
			if ((*format)[i] == ',') {
				eof[fieldTotal] = i;
				fieldTotal++;
			}
		if (fieldTotal > 0) {
			fieldTotal++;
			eof[fieldTotal] = strlen(data);
		}

		// Walk each field in the template, grabbing data as we go
		tempCurLoc = 0; //Skip size specifier
		while (fieldNum < fieldTotal) {
			fieldTemplate = (*format).substr(tempCurLoc,
							 eof[fieldNum] - tempCurLoc);

			fieldName = getFieldName(fieldTemplate);
			fieldSize = getFieldValue(fieldTemplate);

			dataVal = strdupTrim(data + dataCurLoc, fieldSize);
			//coutd << "	fieldTemplate = " << fieldTemplate << " ,fieldName = "
			//<< fieldName << ", fieldSize = " << fieldSize
			//<< ", Dataval =  " << dataVal << endl;
			dataCurLoc += fieldSize;
			tempCurLoc = eof[fieldNum] + 1;
			fieldNum++;
			if (dataVal.length() == 0) {
				continue;
			}
			if (fieldName == "SE_VSCSI") {
				//Only called for VSCSI device - so can set mRecordType here
				fillMe->mRecordType.setValue("VSYS", 100, __FILE__, __LINE__);
				//Only want SE_VSCSI up to the first '-'
				int end = 0;
				while (dataVal[end] != '-')
					end++;
				fillMe->plantMfg.setValue(dataVal.substr(0, end), 85,
							  __FILE__, __LINE__);
			}
			else if (fieldName == "AA") {
				// The third byte of the last 4 read is the one we want.
				char slotNum = data[ dataCurLoc - 2 ];
				// We only want the last 5 bits
				slotNum &= 0x1f;
				ostringstream os;
				os << (int)slotNum;
				fillMe->mSecondLocation.setValue( os.str( ), 60, __FILE__,
								  __LINE__ );
			}
			else if (fieldName != "_") {
				/* Default behavior for most fields.  */
				//Z0 hexification handled during collection
				//Martin liked to hexify this, but I don't think it is necessary
				if (fieldName == "RL" || fieldName == "Z7") {
					dataVal = hexify(dataVal);
				}

				setVPDField( fillMe, fieldName, dataVal , __FILE__, __LINE__);
			}
		}

	}

	/**
	 * @brief: Takes a data stream obtained from sg_utils, and parses it.
	 *	Parsing is done both using a template, and through grabbing of
	 *	values from key locations in the data stream
	 * @arg fillMe: The component that is being queried by sg_utils, and which
	 *	needs to be filled
	 * @arg data: The sg_utils data stream
	 * @arg dataSize: The length in bytes of the data stream
	 * Note: Was called 'device_scsi_sg_render' under lsvpd-0.16
	 */
	int SysFSTreeCollector::interpretPage(Component *fillMe,
					      char *data,
					      int dataSize,
					      int pageCode,
					      string *pageFormat,
					      int subtype,
					      string *subtypeDS)
	{
		string ds;
		string dataTmp = string(data);
		string z0;
		Logger log;
		string tmpStr, lvd;

		if (pageCode == 199) {
			/* pagecode = 0xc7 */
			ds = make_full_scsi_ds(fillMe, "scsi", subtype, data, dataSize);

			if (!ds.empty()) {
				/* add LVD tag to DS if needed */
				if (( 0 == subtype) && (NULL != data) &&
				    (35 <= dataSize) && (7 == data[34])) {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								      + " LVD " + ds, 90, __FILE__, __LINE__);
				}
				else {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								      + " - " + ds, 90, __FILE__, __LINE__);
				}
			}

			return 0;
		}
		// If query was from page 0, the summary page, grab key data values
		if (pageCode == 0) {
			// Set Z0 - characterized as useful data
			tmpStr = lsvpd_hexify((const unsigned char *) data, 8);

			fillMe->updateDeviceSpecific("Z0",
						     "First 8 bytes of SCSI query response",
						     tmpStr, 90);
			tmpStr = string("");
			/* Final mash up adding channel width data to DS field */
			if (data[7] & 0x40) {
				tmpStr = "32 Bit";
			}
			else if (data[7] & 0x20) {
				tmpStr = "16 Bit";
			}
			fillMe->mDescription.setValue(tmpStr + fillMe->mDescription.getValue(),
						      85, __FILE__, __LINE__);
		}

		process_template(fillMe, subtypeDS, data, dataSize, pageFormat, pageCode);
		return 0;
	}

	/**
	 * Close a device that may have been opened for reading
	 */
	void device_close(int device_fd, int major, int minor, int mode)
	{
		char name[256];
		struct stat statbuf;
		if (-1 != sprintf(name, "/tmp/node-%d-%d-%d",
				  mode, major, minor)) {
			if (stat(name, &statbuf) == 0) {
				unlink(name);
			}
		}

	}

	// Open a temp file through which we can ioctl() to device for reading
	int device_open(int major, int minor, int mode)
	{
		char name[256];
		int device_fd = 0;
		int ret;
		struct stat statbuf;

		if (sprintf(name, "/tmp/node-%d-%d-%d", mode, major, minor) < 0) {
			return -ERROR_ACCESSING_DEVICE;
		}

		if (stat(name, &statbuf) == 0) {
			unlink(name);
		}

		ret = mknod(name, 0760 | mode, makedev(major, minor));
		if (ret != 0){
			return -UNABLE_TO_MKNOD_FILE;
		}

		device_fd = open(name, 0);
		if (device_fd < 0) {
			device_close(device_fd, major, minor, mode);
			return -UNABLE_TO_OPEN_FILE;
		}

		return device_fd;
	}

	/* Calculate the length of the response from SG Utils */
	static int
		device_scsi_sg_resp_len(bool evpd, char *device_sg_read_buffer,
					int bufSize)
		{
			int len = 0;

			/*
			 * SPC-3 (7.6) defines some EVPD pages with the page length in
			 * bytes 2 and 3, and some that just use byte 3 with byte 2
			 * reserved.  However, 3.3.9:
			 *	http://www.t10.org/ftp/t10/drafts/spc3/spc3r21b.pdf
			 *  says:
			 *
			 *   [...] A reserved bit, byte, word or field shall be set to
			 *   zero, or in accordance with a future extension to this
			 *   standard. Recipients are not required to check reserved
			 *   bits, bytes, words or fields for zero values. Receipt of
			 *   reserved code values in defined fields shall be reported
			 *   as an error.
			 */
			if (evpd == 0)
				len = device_sg_read_buffer[4] + 5;
			else if (evpd == 1)
				len = device_sg_read_buffer[2] * 256 + device_sg_read_buffer[3] + 4;
			/* Size field is useful for error detection, but seems to be in error
			   when evpd is enabled.  Thus, hacked to return 256 for all inquiries
			   */
			return len;
		}

	/********************************************************************/
	/* Check response from SGUtils to see if it is valid, and can be used */
	static int
		device_scsi_sg_sanity_check(bool   evpd,
					    int    page_code,
					    char *device_sg_read_buffer,
					    int bufSize)
		{
			bool ret;
			char last;
			int i;

			// No sanity checking for non-EVPD pages.
			if (!evpd)
				return 0;

			/* If we asked for a particular EVPD page and we got something
			 * else then we have failed.
			 */
			if (page_code != device_sg_read_buffer[1]) {
				coutd << "Page Code returned not same as requested!  " <<  endl;
				coutd << "page code: '" << device_sg_read_buffer[1] << "'" << endl;
				return -SGUTILS_READ_ERROR;
			}

			// That's all for EVPD page != 0.
			if (0 != page_code)
				return 0;

			/* If we asked for a particular EVPD page and we got something
			 * else then we have failed.
			 */
			if (page_code != device_sg_read_buffer[1])
				return -SGUTILS_READ_ERROR;

			// That's all for EVPD page != 0.
			if (0 != page_code)
				return 0;

			/* Check EVPD page 0 specifies a legal/useful number
			 * of page codes...
			 */
			ret = (0 < device_sg_read_buffer[3])
				&& (0 == device_sg_read_buffer[2]);

			// ...  and the list of available pages is ordered.
			i = 4;
			last = device_sg_read_buffer[i];
			i++;
			while (ret && (i < (int) device_sg_read_buffer[3])) {
				ret = (device_sg_read_buffer[i] > last);
				last = device_sg_read_buffer[i];
				i++;
			}
			if (ret == true)
				return 0;
			else
				return -1;
		}

	/**
	 * @brief: Calles into sg_ll_inquiry for vpd data
	 * @arg: int device_fd: mknod'd file id for device to be queried
	 * @arg: char *device_sg_read_buffer: Ptr to pre-allocated buffer
	 * @arg: int bufSize: Size of the buffer
	 * @arg: evpd: 1 or 0, collect full details or not
	 * @arg: page_code: memory page to retrieve from device
	 */
	int doSGQuery(int device_fd, char *device_sg_read_buffer, int bufSize,
		      int evpd, int page_code, int cmd = 0)
	{
		int len = 0 , ret_ll, ret_san;

		//coutd << "doSGQuery:  " << __LINE__ << " : Querying with evpd: "
		//<< evpd << ", page_code = " << page_code << ", cmd = " << cmd
		//<< ", bufSize = " << bufSize << endl;

		if (evpd)
			bufSize = DEVICE_SCSI_SG_DEFAULT_EVPD_LEN;
		else
			bufSize = DEVICE_SCSI_SG_DEFAULT_STD_LEN;

		//coutd << "sg_ll_inquiry Args: evpd: '" << evpd << "', page_code: "
		//<< page_code << " BufSize: " << bufSize << endl;
		ret_ll = sg_ll_inquiry(device_fd, cmd, evpd, page_code,
				       device_sg_read_buffer, bufSize, 0, 0);

		if (ret_ll == 0) { // Succeeded
			ret_san = device_scsi_sg_sanity_check(evpd, page_code,
							      device_sg_read_buffer, bufSize);

			if (ret_san < 0){
			//coutd << "doSGQuery: " << __LINE__ << ": sanity check returned: "
			//<< ret_san << endl;
				return ret_san;
			}

			len = device_scsi_sg_resp_len(evpd, device_sg_read_buffer, bufSize);


			//Redo sg_ll call with modified length
			if (len > bufSize) {
				bufSize = len;

				//coutd << "Redoing inquiry: " << endl;
				ret_ll = sg_ll_inquiry(device_fd, cmd, evpd, page_code,
						       device_sg_read_buffer, bufSize, 0, 0);
				ret_san = device_scsi_sg_sanity_check(evpd, page_code,
								      device_sg_read_buffer, bufSize);

				if (ret_san < 0){
					return ret_san;
				}
				len = device_scsi_sg_resp_len(evpd, device_sg_read_buffer,
							      bufSize);

			}

		}

		if (!ret_ll && len > MAXBUFSIZE - 1) {
			len = MAXBUFSIZE - 1;
		}

		return len;
	}

	/* load_scsi_templates
	 * @brief Loads scsi templates, used for parsing sg_utils return
	 *   data, from filesystem
	 */
	int SysFSTreeCollector::load_scsi_templates(const string& filename)
	{
		char tmp_line[512];
		string line;
		scsi_template *tmp;
		int dev_count = 0;
		string tmp_str;
		ostringstream err;
		ifstream fin(filename.c_str());

		if (fin.fail()) {
			Logger logger;
			err << "Error opening scsi template file at: "
				<< filename
				<< ";  Details: lsvpd not installed?";
			logger.log(err.str( ), LOG_ERR );
			return -ENOENT;
		}

		while(!fin.eof()) {
			tmp = new scsi_template;
			if( tmp == NULL )
				return -SCSI_FILL_TEMPLATE_LOADING;

			fin.getline(tmp_line,512);
			line = string(tmp_line);

			HelperFunctions::parseString(line, 1, tmp->vendor);
			HelperFunctions::parseString(line, 2, tmp->devClass);
			HelperFunctions::parseString(line, 3, tmp->model);
			HelperFunctions::parseString(line, 4, tmp->format_str);

			dev_count++;

			scsi_templates.push_back(tmp);
			scsi_template_count++;
		}
		return 0;
	}

	/********************************************************************
	 * @brief: High-level data collection call, using ioctl and doSGQuery
	 * 	to collect relevant data which is returned for interpretation.
	 *
	 * This took forever to figure out, but it works like this.  Page_codes
	 * are used to walk thru all data available from a device.  Page code
	 * zero gives you an overview of the device, as well as a reference to the
	 * number of total pages available.  the 'evpd' flag (Enable VPD),
	 * when set to 1, provides slightly more info, primarily consisting of
	 * some binary stuff
	 * at the front end of the return.  Below, I start with page_code 0, from
	 * which I parse the majority of the data needed.  Some other fields are
	 * required, however, so with device identification given page_code 0
	 * parsing, I can determine which other page_code's I need, and how to parse
	 * them, described by the template.
	 *
	 * @arg fillMe: Component to be queried
	 * @arg device_fd: File pointer to mknod'd device file
	 * @arg **device_sg_read_buffer: A pointer which will, upon return,
	 * 	point to a new'd region of memory.  Must be deleted by caller.
	 */

	int SysFSTreeCollector::collectVpd(Component *fillMe, int device_fd, bool limitSCSISize)
	{
		int evpd;
		int i, len = 0;
		char buffer[MAXBUFSIZE];
		string msg, pageCodeRefs;
		struct sg_scsi_id sg_dat;
		string rev, pro, ven;
		int subtype, res, num;
		const scsi_template *devTemplate = NULL; /* current dev's template */
		string pageCode, pageFormat, pageTemp, subtypeDS;
		int pageCodeInt;
		int rc;
		string str;
		char vendor[32], model[32], firmware[32];

		memset(vendor, '\0', 32);
		memset(model, '\0', 32);
		memset(firmware, '\0', 32);

		if (scsi_templates.size() == 0) {
			rc = load_scsi_templates(SCSI_TEMPLATES_FILE);
			if (rc != 0)
				return rc;

			if (scsi_template_count == 0)
				return -SCSI_FILL_TEMPLATE_LOADING;
		}

		res = ioctl(device_fd, SG_GET_SCSI_ID, &sg_dat);
		if (res < 0) {
			return -SGUTILS_IOCTL_FAILED;
		}

		memset(buffer, '\0', MAXBUFSIZE);
		if (0 == sg_ll_inquiry(device_fd, 0, 0, 0, buffer, 64, 1, 0)) {
			/* Stuff the returned buffer into a string for easier parsing */
			int j = 8;
			while (j < 40) {
				if (buffer[j] == ' ')
					break;
				j++;
			}
			memcpy(vendor, &buffer[8], &buffer[j] - &buffer[8]);

			j = 16;
			while (j < 48) {
				if (buffer[j] == ' ')
					break;
				j++;
			}
			memcpy(model, &buffer[16], &buffer[j] - &buffer[16]);
			memcpy(firmware, &buffer[32], 4);

			/* revision  is specified as revision code by sg3_utils documentation,
			 * but this may or may not correspond to firmware revision code.
			 * Am including in component vpd, as most likely this will be
			 * of value somewhere, even if not for the RM field
			 *
			 * vendor is device manufacturer
			 * product is Model
			 */
			//			string fw = hexify(string(firmware));

			fillMe->mManufacturer.setValue(vendor, 100, __FILE__, __LINE__);
			fillMe->mModel.setValue(model, 100, __FILE__, __LINE__);
			fillMe->mFirmwareVersion.setValue(firmware, 100, __FILE__, __LINE__);

			// Slight rehack for ATA devices
			if (fillMe->mManufacturer.getValue() == "ATA") {
				i = 0;
				while ((!ata_device_renaming_scheme[i].key.empty())
				       && (ata_device_renaming_scheme[i].key
					   != fillMe->mModel.getValue()))
					i++;

				if (!ata_device_renaming_scheme[i].key.empty()) {
					fillMe->mManufacturer.setValue(ata_device_renaming_scheme[i].val,
								       100, __FILE__, __LINE__);
					coutd << fillMe->mManufacturer.getValue() << endl;
				}
				//				coutd << "mManufacturer Changed: " << fillMe->mManufacturer.getValue() << endl;
			}

		}

		/* SG Utils Inquiry
		 * Can device be quieried?  Initial Query
		 */
		memset(buffer, '\0', MAXBUFSIZE);
		len = doSGQuery(device_fd, buffer, MAXBUFSIZE, 0, 0, 0);
		if (0 < len) {
			/*
			 * Validate data: if the inquiry data is short or it tells us
			 * that the "target is not capable of supporting a device on
			 * this logical unit, then don't bother proceeding.
			 */
			if ((len < 32) || (0x7F == buffer[0])) {
				return -SG_DATA_INVALID;
			}

			subtype = buffer[0] & 0x1F;

			/* Lookup this subtype in the desc table */
			i = 0;
			while ((device_scsi_types_short[i].key != -1)
			       && (subtype != device_scsi_types_short[i].key))
				i++;
			if (!device_scsi_types_short[i].key != -1)
				subtypeDS = device_scsi_types_short[i].val;
			else
				subtypeDS = "Unknown";
			if (!fillMe->mManufacturer.getValue().empty()) {
				devTemplate =  findTemplate(fillMe->mManufacturer.getValue(),
							    subtypeDS,
							    fillMe->mModel.getValue());
				/* No template - unknown device */
				if (devTemplate == NULL) {
					return -1;
				}
			}
			else {
				// Need device manufacturer to query
				return -1;
			}
			/* Loop through all pages defined by template, grabbing data
			 * as described in the template*/
			num = numPageTemplates(devTemplate->format_str);
			for (int i = 0; i < num; i++) {
				pageTemp = retrievePageTemplate(devTemplate->format_str, i);
				retrievePageCode(pageTemp, pageCode, pageFormat);

				if( pageCode == "0xDIAG" )
				{
					/*
					 * Special case to retrieve Physical locations using
					 * receive diagnostics call.
					 */
					pageCodeInt = 0x02;
					memset(buffer, '\0', MAXBUFSIZE);

					//					coutd << "Querying using evpd:  page code: " << pageCodeInt <<    endl;

					if (limitSCSISize)
						evpd = 0;
					else evpd = 1;

					len = doSGQuery( device_fd, buffer, MAXBUFSIZE, evpd,
							 pageCodeInt, RECEIVE_DIAGNOSTIC );
				}
				else
				{
					pageCodeInt = atoi(pageCode.c_str());

					if (pageCodeInt == 0)
						evpd = 0;
					else
						evpd = 1;

					if (limitSCSISize)
						evpd = 0;
					else evpd = 1;

					// Query this page
					memset(buffer, '\0', MAXBUFSIZE);
					//					coutd << "Attempting query, evpd = " << evpd << ", pageCodeInt = " << pageCodeInt <<endl;
					len = doSGQuery(device_fd, buffer, MAXBUFSIZE, evpd, pageCodeInt, 0);
				}

				if (len < 0) {
					//Query resulted in bad result
					return len;
				}

				/*
				 * Useful code for debugging
				 * cout << " ------------------------------------- " << endl;
				 cout << "Page: " << pageCodeInt << ", Buffer: " << endl;
				 for (int s = 0; s < 170; s++)
				 printf("%c", buffer[s]);
				 printf("\n");

				 cout << "Page: " << pageCodeInt << ",Buffer: " << endl;
				 for (int s = 0; s < 170; s++)
				 printf("%d:%c  ", s, buffer[s]);
				 printf("\n");*/

				//Interpret this page
				interpretPage(fillMe, buffer, len, pageCodeInt, &pageFormat,
					      subtype, &subtypeDS);
			}
		}

		return 0;
	}

	string SysFSTreeCollector::findGenericSCSIDevPath( Component *fillMe )
	{
		string link = fillMe->sysFsNode.getValue() + "/generic";
		return HelperFunctions::getSymLinkTarget(link);
	}


	/********************************************************************
	 * Get device Major:Minor device codes, as well as access mode
	 * Required for sg_utils query
	 */
	int SysFSTreeCollector::get_mm_scsi(Component *fillMe)
	{
		string str;
		int tmp, useGeneric = 0;
		int beg, end;
		string devClass, link, genericPath;
		vector<DataItem*>::const_iterator j, stop;

		if (fillMe->idNode.getValue().empty())
			return -CLASS_NODE_UNDEFINED;


		/* 
		 * Preferred 'dev' file lookup via SCSI generic link,
		 * because it provides more info than the other accesss
		 * types.
		 */
		genericPath = findGenericSCSIDevPath(fillMe);
		str = getAttrValue(genericPath, "dev");


		/* Backup 1: Look in device class dir */
		if (str.length() == 0)
			str = getAttrValue(fillMe->getClassNode(), "dev");
		else
			/* Remember we used generic link */
			useGeneric = 1;

		/* Backup 2: Look in device root dir */
		if (str.length() == 0)
			str = getAttrValue(fillMe->idNode.getValue(), "dev");

		if (str.empty())
			return -FILE_NOT_FOUND;

		beg = end = 0;
		while (end < (int) str.length() && str[end] != ':')
			end++;

		tmp = atoi(str.substr(beg, end).c_str());
		fillMe->devMajor = tmp;

		beg = end + 1;
		end = beg;
		while (end < (int) str.length() && str[end] != ':') {
			end++;
		}

		tmp = atoi(str.substr(beg, end).c_str());
		fillMe->devMinor = tmp;

		// Set Access Mode
		devClass = fillMe->getDevClass();

		/* If we used 'generic' link, use S_IFCHR mode */
		if (useGeneric == 1) fillMe->devAccessMode = S_IFCHR;
		else if (devClass == "block") fillMe->devAccessMode = S_IFBLK;
		else if (devClass.find("scsi", 0) > 0) fillMe->devAccessMode = S_IFCHR;
		else if (devClass.find("usb", 0) > 0) fillMe->devAccessMode = S_IFCHR;
		else if (devClass == "tape") fillMe->devAccessMode = S_IFCHR;
		else if (devClass == "generic") fillMe->devAccessMode = S_IFCHR;

		return 0;
	}

	/*
	 * Parse a field from the iprconfig output.
	 * The data is of the following format :
	 *  field.....:<SPACE><VALUE>
	 */
	string parseIPRField(string& output, string& field)
	{
		int start, end;

		start = output.find(field);

		if (start == (int) string::npos)
			return "";

		start = output.find(':', start);
		if (start == (int) string::npos)
			return "";

		/* Skip the space */
		start++;
		end = output.find('\n', start);
		if (end == (int) string::npos)
			end = output.length();
		return string(output.substr(start, end - start));
	}

	void SysFSTreeCollector::parseIPRData( Component *fillMe, string& output)
	{
		char ac[3];
		char *idx = &ac[1];
		string val, dev, key;

		strcpy(ac, "Z0");
		dev = "Device Specific";

		/* Find the platform location code */
		key = "Platform Location";
		val = parseIPRField( output, key );
		if (val != "")
			fillMe->mPhysicalLocation.setValue(val, 100, __FILE__, __LINE__);

		/* Fix FRU Number */
		key = "FRU Number";
		val = parseIPRField( output, key );
		if (val != "")
			fillMe->mFRU.setValue(val, 100, __FILE__, __LINE__);

		/* Fix Part Number */
		key = "Part Number";
		val = parseIPRField( output, key );
		if (val != "")
			fillMe->mPartNumber.setValue(val, 100, __FILE__, __LINE__);

		key = "Serial Number";
		val = parseIPRField( output, key );
		if (val != "")
			fillMe->mSerialNumber.setValue(val, 100, __FILE__, __LINE__);

		/* Now fill in the Device Specific record for SCSI, Z0-Z7 */
		for (*idx = '0'; *idx < '8'; (*idx)++) {
			key = dev + " (" + string((char*)ac) + ")";
			val = parseIPRField( output, key );
			if (val != "") 
				fillMe->updateDeviceSpecific(string(ac), dev, val, 100);
		}

		return;
	}

	void SysFSTreeCollector::fillIPRData( Component *fillMe )
	{
		string path = findGenericSCSIDevPath( fillMe );
		string sg, output;
		char *devSg;
		string cmd;

		if (path == "")
			return;

		devSg = strdup(path.c_str());
		if (devSg == NULL)
			return;

		sg = basename(devSg);
		if (sg == "")
			goto out;

		cmd = "/usr/sbin/iprconfig -c show-details " + sg;
		if (HelperFunctions::execCmd(cmd.c_str(), output))
			goto out;

		parseIPRData(fillMe, output);
out:
		free(devSg);
	}

	/********************************************************************
	 *
	 * @brief: Main function for querying a device for SCSI info.
	 * @arg: Device to be queried and filled
	 */
	void SysFSTreeCollector::fillSCSIComponent( Component* fillMe, bool limitSCSISize = false)
	{
		int rc;
		string data;
		int device_fd;
		ostringstream err;
		Logger logger;
		string msg;
		/* Not a SCSI device */
		if (fillMe->devBus.getValue() == "usb")
			return;
		//		coutd << " Querying with limitSCSISize set to: " << limitSCSISize << endl;

		//		coutd << "Query SCSI dev at: " << fillMe->idNode.getValue() << endl;

		/* Need major:minor codes to query device */
		if (!(rc = get_mm_scsi(fillMe))) {

			// Open Device for reading
			device_fd = device_open(fillMe->devMajor,
						fillMe->devMinor,
						fillMe->devAccessMode);
			if (device_fd < 0) {
				msg = string("vpdupdate: Failed opening device: ")
					+ fillMe->idNode.getValue();
				logger.log( msg, LOG_WARNING );
				return;
			}

			collectVpd(fillMe, device_fd, limitSCSISize);

			device_close(device_fd,
				     fillMe->devMajor,
				     fillMe->devMinor,
				     fillMe->devAccessMode);
		}

		fillIPRData( fillMe );
		return;
	}
}
