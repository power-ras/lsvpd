/******************************************************************************
 *   Copyright (C) 2026, IBM                                                  *
 *                                                                            *
 *   Maintained by:                                                           *
 *   Muskan Goyal : muskan@linux.ibm.com                                      *
 *                                                                            *
 *   See 'COPYING' for License of this code.                                  *
 ******************************************************************************/
#ifndef LSVPD_SPYRE_UTILS_HPP
#define LSVPD_SPYRE_UTILS_HPP

#include <string>

namespace lsvpd
{
	inline bool isSpyreDeviceId(const std::string& id)
	{
		return id == "0x06a7" || id == "0x06a8";
	}
}

#endif
