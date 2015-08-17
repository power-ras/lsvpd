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

#define PLATFORM_FILE "/proc/cpuinfo"

using namespace std;

namespace lsvpd {

	enum platform {
		PF_NULL,
		PF_POWERVM_LPAR,
		PF_OPAL,
		PF_PSERIES_KVM_GUEST,
		PF_ERROR };

	class PlatformCollector {
		public:
			/**
			 * Checks if it running on pSeries or PowerNV and
			 * stores platform type.
			 */
	                static platform platform_type;
		        static void get_platform();
			static string get_platform_name();
			static string getFirmwareName();
	};
}

#endif /*LSVPDPLATFORMCOLLECTOR_H_*/
