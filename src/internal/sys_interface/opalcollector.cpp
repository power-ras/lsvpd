/***************************************************************************
 *   Copyright (C) 2012, IBM                                               *
 *                                                                         *
 *   Authors:                                                              *
 *   Bharani C.V : bharanve@in.ibm.com                                     *
 *                                                                         *
 *   See 'COPYING' for License of this code.                               *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#define DEBUGOPAL 1

#include <libvpd-2/lsvpd_error_codes.hpp>
#include <fswalk.hpp>
#include <opalcollector.hpp>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <dirent.h>
#include <errno.h>

using namespace std;

namespace lsvpd {
	vector<string> OpalCollector::dirlist;
	vector<string> OpalCollector::location;
	vector<string> OpalCollector::desc;

#define CHECK_DATA(p, n, e)			     \
	do {					    \
		if (((p) + (n)) > (e))  {	       \
			printf("UNDERFLOW !\n");	\
			return -1;		      \
		}				       \
	} while(0)

	const char* get_fru_description(char* fru_type)
	{
		switch (fru_type[0]) {
		case 'A':
			switch (fru_type[1]) {
			case 'B':
				return "Combined AC  bulk power supply";
			case 'M':
				return "Air mover";
			case 'V':
				return "Anchor VPD";
			}
			break;
		case 'B':
			switch (fru_type[1]) {
			case 'A':
				return "Bus adapter card";
			case 'C':
				return "Battery charger";
			case 'D':
				return "Bus/Daughter card";
			case 'E':
				return "Bus expansion card";
			case 'P':
				return "Backplane";
			case 'R':
				return "Backplane riser";
			case 'X':
				return "Backplane extender";
			}
			break;
		case 'C':
			switch (fru_type[1]) {
			case 'A':
				return "Calgary bridge";
			case 'B':
				return "Connector - Infiniband";
			case 'C':
				return "Clock card";
			case 'D':
				return "Card connector";
			case 'E':
				return "Connector - Ethernet interface";
			case 'L':
				return "Calgary PHB VPD";
			case 'I':
				return "Interactive capacity card";
			case 'O':
				return "Connector - SMA interface";
			case 'P':
				return "Processor capacity card";
			case 'R':
				return "Connector - RIO interface";
			case 'S':
				return "Connector - Serial interface";
			case 'U':
				return "Connector - USB interface";
			}
			break;
		case 'D':
			switch (fru_type[1]) {
			case 'B':
				return "DASD Backplane";
			case 'C':
				return "Drawer connector card";
			case 'E':
				return "Drawer etension";
			case 'I':
				return "Drawer interposer";
			case 'L':
				return "P7 IH D-link connector";
			case 'T':
				return "Legacy PCI daughter card";
			case 'V':
				return "Media drawer LED";
			}
			break;
		case 'E':
			switch (fru_type[1]) {
			case 'I':
				return "Enclosure LED";
			case 'F':
				return "Enclosure fault LED";
			case 'S':
				return "Embedded SAS";
			case 'T':
				return "Ethernet riser card (no GX bus)";
			case 'V':
				return "Enclosure VPD";
			}
			break;
		case 'F':
			switch (fru_type[1]) {
			case 'M':
				return "Frame";
			}
			break;
		case 'H':
			switch (fru_type[1]) {
			case 'B':
				return "Host bridge RIO to PCI card";
			case 'D':
				return "High speed daughter card";
			case 'M':
				return "HMC connector";
			}
			break;
		case 'I':
			switch (fru_type[1]) {
			case 'B':
				return "IO backplane";
			case 'C':
				return "IO card";
			case 'D':
				return "IDE connector";
			case 'I':
				return "IO drawer enclosure LEDs";
			case 'P':
				return "Interplane card";
			case 'S':
				return "SMP V-Bus interconnection cable";
			case 'T':
				return "Enclosure interconnection cable";
			case 'V':
				return "IO drawer enclosure VPD";
			}
			break;
		case 'K':
			switch (fru_type[1]) {
			case 'V':
				return "Keyboard video mouse LED";
			}
			break;
		case 'L':
			switch (fru_type[1]) {
			case '2':
				return "Level 2 cache module/card";
			case '3':
				return "Level 3 cache module/card";
			case 'C':
				return "Squadrons H llight strip connector";
			case 'R':
				return "P7 IH L-link connector";
			case 'O':
				return "System locate LED";
			case 'T':
				return "Squadrons H light strip";
			}
			break;
		case 'M':
			switch (fru_type[1]) {
			case 'B':
				return "Media backplane";
			case 'E':
				return "Map extension";
			case 'M':
				return "MIP meter";
			case 'S':
				return "Main store card or DIMM";
			}
			break;
		case 'N':
			switch (fru_type[1]) {
			case 'B':
				return "NVRAM battery";
			case 'C':
				return "Service processor node controller";
			case 'D':
				return "NUMA DIMM";
			}
			break;
		case 'O':
			switch (fru_type[1]) {
			case 'D':
				return "CUoD card";
			case 'P':
				return "Operator panel";
			case 'S':
				return "Oscillator";
			}
			break;
		case 'P':
			switch (fru_type[1]) {
			case '2':
				return "IOC chip and its devices";
			case '5':
				return "IOC/IOC2 PCI bridge";
			case 'B':
				return "IO drawer main backplane";
			case 'C':
				return "Power capacitor";
			case 'D':
				return "Processor card";
			case 'F':
				return "Processor FRU";
			case 'I':
				return "IOC/IOC2 PHB";
			case 'O':
				return "SPCN";
			case 'N':
				return "SPCN connector";
			case 'R':
				return "PCI riser card";
			case 'S':
				return "Power supply";
			case 'T':
				return "Pass-through card";
			case 'X':
				return "PSC power sync card";
			case 'W':
				return "Power connector";
			}
			break;
		case 'R':
			switch (fru_type[1]) {
			case 'G':
				return "Regulator";
			case 'I':
				return "Riser";
			case 'K':
				return "Rack indicator connector";
			case 'W':
				return "RiscWatch connector";
			}
			break;
		case 'S':
			switch (fru_type[1]) {
			case 'A':
				return "System attention LED";
			case 'B':
				return "Backup system VPD";
			case 'C':
				return "SCSI connector";
			case 'D':
				return "SAS connector";
			case 'I':
				return "SCSI to IDE converter";
			case 'L':
				return "PHB slot - PHB child";
			case 'P':
				return "Service processor";
			case 'R':
				return "Service interconnection card";
			case 'S':
				return "Soft switch";
			case 'V':
				return "System VPD";
			case 'Y':
				return "Legacy system VPD FRU type";
			}
			break;
		case 'T':
			switch (fru_type[1]) {
			case 'D':
				return "Time of Day clock";
			case 'I':
				return "Torrent PCIE PHB";
			case 'L':
				return "Torrent riser PCIE PHB slot";
			case 'M':
				return "Thermal sensor";
			case 'P':
				return "TPMD adapter";
			case 'R':
				return "Torrent octant PCIE bridge";
			}
			break;
		case 'V':
			switch (fru_type[1]) {
			case 'V':
				return "Root node VPD";
			}
			break;
		case 'W':
			switch (fru_type[1]) {
			case 'D':
				return "Water device";
			}
			break;
		}
		return "Unknown";
	}

	static void add_location_code ( string locCodePath )
	{
		string fname = locCodePath + VPD_FILE_LOC_CODE;
		int fd;
		char *locCode;

		fd = open(fname.c_str(), O_RDONLY, 0);
		if (fd == -1) {
			OpalCollector::location.push_back("");
			return;
		}

		/* Location codes  -- at most 80 chars with null termination */
		locCode = (char*)calloc(80, sizeof(char));
		read(fd, locCode, 80);
		OpalCollector::location.push_back(locCode);
		free(locCode);
		close(fd);
	}

	static void add_description ( string descPath )
	{
		string fname = descPath + VPD_FILE_FRU_TYPE;
		int fd, rstat;
		struct stat st;
		char *descript;
		fd = open(fname.c_str(), O_RDONLY, 0);
		rstat = stat(fname.c_str(), &st);
		if (rstat == -1 || fd == -1) {
			OpalCollector::desc.push_back("");
			return;
		}
		descript = (char*)calloc(st.st_size + 1, sizeof(char));
		read(fd, descript, st.st_size);
		OpalCollector::desc.push_back(get_fru_description(descript));
		close(fd);
		free(descript);
	}

	void listDirectory(string baseDir, vector<string> *dirlist)
	{
		DIR *dp;
		struct dirent *dirp;
		if ((dp = opendir(baseDir.c_str()))) {
			while ((dirp = readdir(dp))) {
				if (dirp->d_name != string(".") && dirp->d_name != string("..")) {
					if (FSWalk::fs_isDir(baseDir + dirp->d_name) == true) {
							dirlist->push_back(baseDir + dirp->d_name + "/");
							listDirectory(baseDir + dirp->d_name + "/", dirlist);
					}
				}
			}
			 closedir(dp);
		}
	}

	OpalCollector::OpalCollector()
	{
		vector<string>::const_iterator it;
		dirlist.push_back(VPD_FILE_DATA_PATH);
		listDirectory(VPD_FILE_DATA_PATH, &dirlist);

		add_location_code(dirlist.at(0));
		desc.push_back(get_fru_description("SV"));

		for(it = dirlist.begin() + 1; it != dirlist.end(); it++) {
			add_location_code(*it);
			add_description(*it);
		}
	}

	static void deleteList(struct opal_buf_element *list)
	{
		struct opal_buf_element *head = list, *next;

		while (!head) {
			next = head->next;
			delete head;
			head = next;
		}
	}

	int OpalCollector::addPlatformVPD(const string& yl, char ** data)
	{
		return opalGetVPD(yl, data);
	}

	/**
	 * Parses all nodes under /proc/device-tree/vpd for ibm,vpd file, and
	 * generates a string of all available vpd data
	 * @param yl
	 *   The location code of the device to pass to OPAL, if this is an
	 *   empty string, all of the system VPD will be stored in data.
	 * @param data
	 *   char** where we will put the vpd (*data should be NULL)
	 */
	int OpalCollector::opalGetVPD(const string& yl = "",
				      char ** data = NULL)
	{
		int fd, count = 0;
		size_t size = 0;
		struct opal_buf_element *current, *list, *prev = NULL;
		int num = 0;
		char *buf;
		vector<string>::const_iterator it;
#ifdef DEBUGOPAL
		printf("Collecting OPAL info: [%d] %s\n", __LINE__, __FILE__);
#endif
		if(yl.length() > 0) {
			fd = open(VPD_FILE_SYSTEM_FILE, O_RDONLY, 0);
			if (fd == -1)
				return 0;
			current = new opal_buf_element;
			if(!current)
				return 0;
			list = current;
			count++;
			memset(current->buf, '\0', OPAL_BUF_SIZE);
			current->size = read(fd, current->buf, OPAL_BUF_SIZE);
			current->next = NULL;
		} else {
			for(it = dirlist.begin(); it != dirlist.end(); it++) {
				current = new opal_buf_element;
				if (count == 0)
					list = current;
				count++;
				memset(current->buf, '\0', OPAL_BUF_SIZE);

				fd = open((*it + VPD_FILE_DATA_FILE).c_str(), O_RDONLY, 0);
				/* In case a node does not have an ibm,vpd
				 * file, then that means the node shares
				 * same vpd data as its parent. Hence, looking
				 * for parent node's ibm,vpd file.
				 */
				if (fd == -1)
					fd = open(( *it + "../" + VPD_FILE_DATA_FILE).c_str(), O_RDONLY, 0);
				if (fd == -1) {
					unsigned char empty_vpd[] = "\0x84\000\000";
					memcpy(current->buf, empty_vpd, 3);
					size = 3;
				} else
					current->size = read(fd, current->buf, OPAL_BUF_SIZE);
				if(prev)
					prev->next = current;
				current->next = NULL;
				prev = current;
				current = current->next;
			}
		}
		current = list;
		do {
			size = size + current->size;
		} while ((current = (current->next)) != NULL);
		current = list;
		/* Count is added to total buffer size as # is added at
		 * begining of each buffer to mark the start of file.
		 *
		 * Format
		 *   #|SECTION_HEADER|DATA|SECTION_END|SECTION_HEADER|...
		 *   #|SECTION_HEADER|DATA|SECTION_END|SECTION_HEADER|DATA|
		 */
		size += count;
		*data = new char[ size ];
		if( *data == NULL )
			return -ENOMEM;
		memset( *data, '\0', size );

		buf = *data;
		do {
			/* As each ibm,vpd file might have multiple
			 * section headers, # is used as marker to
			 * denote the beginning of ibm,vpd file contents
			 * in the buffer.
			 */
			memset(buf, '#', 1);
			buf += 1;
			memcpy(buf, current->buf, current->size);
			buf += current->size;
		} while ((current = (current->next)) != NULL);

		deleteList(list);

		return size;
	}
}
