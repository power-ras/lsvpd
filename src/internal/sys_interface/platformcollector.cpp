/***************************************************************************
 *   Copyright (C) 2014, IBM                                               *
 *                                                                         *
 *   Maintained by:                                                        *
 *   Bharani C.V : bharanve@in.ibm.com                                     *
 *                                                                         *
 *   See 'COPYING' for License of this code.                               *
 ***************************************************************************/

#include <libvpd-2/logger.hpp>
#include <platformcollector.hpp>
#include <rtascollector.hpp>
#include <opalcollector.hpp>

using namespace std;

namespace lsvpd {
	platform PlatformCollector::platform_type = PF_NULL;
	void PlatformCollector::get_platform()
	{
		string pf_file = PLATFORM_FILE;
		ifstream platform_info(pf_file.c_str());
		if (platform_info.is_open()) {
			string buffer((istreambuf_iterator<char>(platform_info)), istreambuf_iterator<char>());
			if (buffer.find("PowerNV") != -1)
				platform_type = PF_POWERKVM_HOST;
			else if (buffer.find("pSeries (emulated by qemu)") != -1 )
				platform_type = PF_POWERKVM_PSERIES_GUEST;
			else if ( buffer.find("pSeries") != -1 )
				platform_type = PF_POWERVM_LPAR;
			else
				platform_type = PF_ERROR;
			platform_info.close();
		}
		else {
			Logger logger;
			logger.log("Unable to open /proc/cpuinfo file", LOG_WARNING);
		}
	}

	string PlatformCollector::get_platform_name()
	{
		get_platform();
		switch(platform_type) {
		case PF_POWERKVM_HOST:
			return "PowerKVM Host";
		case PF_POWERVM_LPAR:
			return "PowerVM pSeries LPAR";
		case PF_POWERKVM_PSERIES_GUEST:
			return "PowerKVM pSeries Guest";
		case PF_ERROR:
			return "Unknown";
		}
		return "";
	}

	/**
	 * Returns dynamic object pointing to RtasCollector if
	 * it is running on pSeries or OpalCollector if it is
	 * running on PowerNV.
	 */
	void PlatformCollector::get_collector(PlatformCollector **p)
	{
		get_platform();
		switch(platform_type) {
		case PF_POWERVM_LPAR:
		case PF_POWERKVM_PSERIES_GUEST:
			*p = new RtasCollector();
			break;
		case PF_POWERKVM_HOST:
			*p = new OpalCollector();
			break;
		case PF_ERROR:
			*p = 0;
			break;
		}
	}
}
