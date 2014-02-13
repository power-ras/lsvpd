/***************************************************************************
 *   Copyright (C) 2014, IBM                                               *
 *                                                                         *
 *   Author :                                                              *
 *   Bharani C.V : bharanve@in.ibm.com                                     *
 *                                                                         *
 *   See 'COPYING' for License of this code.                               *
 ***************************************************************************/
#ifndef LSVPDOPALCOLLECTOR_H_
#define LSVPDOPALCOLLECTOR_H_

#include <libvpd-2/logger.hpp>
#include <platformcollector.hpp>

#include <string>
#include <vector>
#include <errno.h>

using namespace std;

namespace lsvpd
{

	#define VPD_FILE_SYSTEM_FILE	"/proc/device-tree/vpd/ibm,vpd"
	#define VPD_FILE_DATA_PATH	"/proc/device-tree/vpd/"
	#define VPD_FILE_FRU_TYPE	"fru-type"
	#define VPD_FILE_LOC_CODE	"ibm,loc-code"
	#define VPD_FILE_DATA_FILE	"ibm,vpd"
	#define OPAL_BUF_SIZE		4096
	#define OPAL_ERR_BUF_SIZE	40

	/**
	 * @struct buf_element
	 * @brief
	 *   List element for data parsed from ibm,vpd file
	 */
	struct opal_buf_element {
	        char buf[OPAL_BUF_SIZE]; /* data buffer for opal_get_vpd() */
	        unsigned int size;	/* amount of the buffer filled in by opal_get_vpd() */
		struct opal_buf_element *next;
	};

	class OpalCollector: public PlatformCollector {

		public:
			static vector<string> dirlist;
			static vector<string> location;
			static vector<string> desc;

			OpalCollector();
			/* Retrieve Opal VPD  */

			/**
			 * Retrieve OPAL VPD if available
			 *
			 * @param yl
			 *   The location code for the device of interest
			 *   (default to "" which dumps all VPD)
			 * @param data
			 *   A pointer to the address of the buffer to hold
			 *   requested VPD
			 */
		        int addPlatformVPD(const string& yl, char** data);
			int opalGetVPD(const string& yl, char** data);
	};
}

#endif /*LSVPDOPALCOLLECTOR_H_ */
