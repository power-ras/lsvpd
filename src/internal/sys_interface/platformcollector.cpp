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

using namespace std;

namespace lsvpd {
	platform PlatformCollector::platform_type = PF_NULL;

	static string extractTagValue(char *buf)
	{
		buf = strchr(buf, ':');
		if (!buf)
			return string();
		/* skip : */
		buf++;
		/* skip spaces */
		while (isspace(*buf)) buf++;
		return string(buf);
	}

	static string getCpuInfoTag(const char *tag)
	{
		char buf[1024];
		int len = strlen(tag);
		string value = string();

		ifstream ifs(PLATFORM_FILE);
		Logger log;

		if (!ifs.is_open()) {
			log.log("Unable to open file /proc/cpuinfo", LOG_WARNING);
			goto error;
		}

		buf[0] = '\0';

		do  {
			ifs.getline(buf, 1024);
			/* Found the tag */
			if (!strncmp(buf, tag, len)) {
				value = extractTagValue(buf);
				break;
			}
		} while (!ifs.eof());
		ifs.close();
error:
		return value;
	}

	void PlatformCollector::get_platform()
	{
		string platform = getCpuInfoTag("platform");

		if ( platform == "PowerNV" )
			platform_type = PF_POWERKVM_HOST;
		else if ( platform == "pSeries (emulated by qemu)" )
			platform_type = PF_POWERKVM_PSERIES_GUEST;
		else if ( platform == "pSeries" )
			platform_type = PF_POWERVM_LPAR;
		else
			platform_type = PF_ERROR;
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
		case PF_NULL:
		case PF_ERROR:
			return "Unknown";
		}
		return "";
	}

	string PlatformCollector::getFirmwareName()
	{
		return getCpuInfoTag("firmware");
	}
}
