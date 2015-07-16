/***************************************************************************
 *   Copyright (C) 2006, IBM                                               *
 *                                                                         *
 *   Maintained By:                                                        *
 *   Eric Munson and Brad Peters                                           *
 *   munsone@us.ibm.com, bpeters@us.ibm.com                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the Lesser GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or at your option) any later version.                        *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Lesser General Public License for more details.                   *
 *                                                                         *
 *   You should have received a copy of the Lesser GNU General Public      *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <libvpd-2/lsvpd_error_codes.hpp>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cstdio>

using namespace std;

namespace lsvpd
{

	string interp_err_code(int errCode_t)
	{
		int errCode = abs(errCode_t);
		switch(errCode) {
			case FILE_NOT_FOUND:
				return string("Error: File Not Found");
			case DIRECTORY_NOT_FOUND:
				return string("Error: Directory Not Found");
			case SYSTEM_COMMAND_PROCESSOR_NOT_FOUND:
				return string("Error: System command processor not found");
			case CLASS_NODE_UNDEFINED:
				return string("Error: Device SysFS Classnode not identified.  Kernel may not have a modern Sys FS");
			case ERROR_ACCESSING_DEVICE:
				return string("Error: Unable to access device ");
			case BUFFER_EMPTY:
				return string("Error: The memory buffer check is empty.  This may represent a device access error");
			case SGUTILS_READ_ERROR:
				return string("Error: Response received from sg3_utils formatted incorrectly");
			case UNABLE_TO_OPEN_FILE:
				return string("Error: Unable to open file.  Check permissions.");
			case UNABLE_TO_MKNOD_FILE:
				return string("Error: Unable to mknod() the specified file.  Possibly a permissions issue.");
			case SG_DATA_INVALID:
				return string("Error: The data obtained through sg3_utils for this device was invalid");
			case RTAS_CALL_NOT_AVAILABLE:
				return string("The ibm,get-vpd RTAS call is not available on this system.");
			case RTAS_PARAMETER_ERROR:
				return string("An RTAS vpd parameter was in error.  Please report.");
			case RTAS_HARDWARD_ERROR:
				return string("Error detected with hardware and/or device driver.  RTAS vpd inquiry failed.");
			case RTAS_ERROR:
				return string("lsvpd and RTAS were unable to communicate with hardware for some unknown reason.");
			case SCSI_FILL_TEMPLATE_LOADING:
				return string("lsvpd was unable to load the SCSI inquiry configuration file, 'scsi_templates.conf'");

			case -1:
				//Assume errno was set if -1
				perror("mknod: ");
				return string("");
			default:
				/* Assuming this is a standard error code by default */
				return string(strerror(errCode));
		}

	}

}

