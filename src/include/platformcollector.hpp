/***************************************************************************
 *   Copyright (C) 2014, IBM                                               *
 *                                                                         *
 *   Author:                                                        *
 *   Bharani C.V : bharanve@in.ibm.com                                    *
 *                                                                         *
 *   See 'COPYING' for License of this code.                               *
 ***************************************************************************/
#ifndef LSVPDPLATFORMCOLLECTOR_H_
#define LSVPDPLATFORMCOLLECTOR_H_

#include <string>
#include <errno.h>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

#define PLATFORM_FILE	"/proc/cpuinfo"
#define DT_NODE_FSP	"/proc/device-tree/fsp"
#define DT_NODE_BMC	"/proc/device-tree/bmc"

using namespace std;

namespace lsvpd {

	enum platform {
		PF_NULL,
		PF_POWERVM_LPAR,
		PF_OPAL,
		PF_PSERIES_KVM_GUEST,
		PF_ERROR };

	/* Service processor type (FSP or BMC) */
	enum service_processor_type {
		PF_SP_NULL,
		PF_SP_FSP,
		PF_SP_BMC,
		PF_SP_ERROR
	};

	class PlatformCollector {
		private:
			static void get_sp_type();

		public:
			/**
			 * Checks if it running on pSeries or PowerNV and
			 * stores platform type.
			 */
	                static platform platform_type;
	                static service_processor_type platform_sp_type;

		        static void get_platform();
			static string get_platform_name();
			static string getFirmwareName();
			static bool isBMCBasedSystem();
			static bool isFSPBasedSystem();
	};
}

#endif /*LSVPDPLATFORMCOLLECTOR_H_*/
