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
	service_processor_type PlatformCollector::platform_sp_type = PF_SP_NULL;

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

	void PlatformCollector::get_sp_type()
	{
		int rc;
		struct stat sbuf;

		/* Check for BMC node */
		rc = stat(DT_NODE_BMC, &sbuf);
		if (rc == 0) {
			platform_sp_type = PF_SP_BMC;
			return;
		}

		/* Check for FSP node */
		rc = stat(DT_NODE_FSP, &sbuf);
		if (rc == 0) {
			platform_sp_type = PF_SP_FSP;
			return;
		}

		platform_sp_type = PF_SP_ERROR;
	}

	void PlatformCollector::get_platform()
	{
		string buf;
		ifstream ifs(PLATFORM_FILE);
		Logger log;

		if (!ifs.is_open()) {
			log.log("Unable to open file /proc/cpuinfo", LOG_WARNING);
			platform_type = PF_ERROR;
			return;
		}

		buf[0] = '\0';

		while (getline(ifs, buf)) {
			if (strstr(buf.c_str(), "PowerNV")) {
				platform_type = PF_OPAL;
				break;
			} else if (strstr(buf.c_str(), "pSeries (emulated by qemu)")) {
				platform_type = PF_PSERIES_KVM_GUEST;
				break;
			} else if (strstr(buf.c_str(), "pSeries")) {
				platform_type = PF_POWERVM_LPAR;
				/* catch model for PowerNV guest */
				continue;
			}
		}

		if (platform_type == PF_NULL)
			platform_type = PF_ERROR;

		ifs.close();

		/* Get Service processor type */
		get_sp_type();
	}

	string PlatformCollector::get_platform_name()
	{
		get_platform();
		switch(platform_type) {
		case PF_OPAL:
			return "OPAL";
		case PF_POWERVM_LPAR:
			return "PowerVM pSeries LPAR";
		case PF_PSERIES_KVM_GUEST:
			return "pSeries KVM Guest";
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

	bool PlatformCollector::isBMCBasedSystem()
	{
		if (platform_sp_type == PF_SP_NULL)
			get_sp_type();

		if (platform_sp_type == PF_SP_BMC)
			return true;

		return false;
	}

	bool PlatformCollector::isFSPBasedSystem()
	{
		if (platform_sp_type == PF_SP_NULL)
			get_sp_type();

		if (platform_sp_type == PF_SP_FSP)
			return true;

		return false;
	}
}
