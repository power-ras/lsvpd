/***************************************************************************
 *   Copyright (C) 2012, IBM                                               *
 *                                                                         *
 *   Maintained by:                                                        *
 *   Suzuki Poulose : suzuki@in.ibm.com                                    *
 *   James Keniston : jkenisto@us.ibm.com                                  *
 *                                                                         *
 *   See 'COPYING' for License of this code.                               *
 ***************************************************************************/
#ifndef LSVPDRTASCOLLECTOR_H_
#define LSVPDRTASCOLLECTOR_H_

#include <libvpd-2/component.hpp>		/* For Component */
#include <libvpd-2/system.hpp>
#include <libvpd-2/logger.hpp>
#include <platformcollector.hpp>

#include <string>
#include <vector>
#include <errno.h>

using namespace std;

namespace lsvpd
{
	// Return codes from the RTAS call (not already handled by librtas)
	#define SUCCESS         0
	#define CONTINUE        1
	#define HARDWARE_ERROR  -1
	#define PARAMETER_ERROR -3
	#define VPD_CHANGED     -4

	// rtas
	#define PROC_FILE_RTAS_CALL "/proc/device-tree/rtas/ibm,get-vpd"
	#define RTAS_BUF_SIZE        4096
	#define RTAS_ERR_BUF_SIZE    40

	/**
	 * @struct buf_element
	 * @brief
	 *   List element for data returned by rtas_get_vpd()
	 */
	struct rtas_buf_element {
	        char buf[RTAS_BUF_SIZE]; 	/* data buffer for rtas_get_vpd() */
	        struct rtas_buf_element *next;
	        unsigned int size; 		/* amount of the buffer filled in by rtas_get_vpd() */
	};

	class RtasCollector: public PlatformCollector {

		public:

			static string rtasSystemParm(int code);

			/**
			 * Retrieve RTAS VPD if available
			 *
			 * @param yl
			 *   The location code for the device of interest (default to ""
			 * which dumps all VPD)
			 * @param data
			 *   A pointer to the address of the buffer to hold requested VPD
			 */
			static int rtasGetVPD(const string& yl, char** data);

			int addPlatformVPD(const string& yl, char** data);

	};
}

#endif /*LSVPDRTASCOLLECTOR_H_ */
