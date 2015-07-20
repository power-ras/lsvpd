/***************************************************************************
 *   Copyright (C) 2012, IBM                                               *
 *                                                                         *
 *   Maintained by:                                                        *
 *   Suzuki Poulose : suzuki@in.ibm.com                                    *
 *   James Keniston : jkenisto@us.ibm.com                                  *
 *                                                                         *
 *   See 'COPYING' for License of this code.                               *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


// #define DEBUGRTAS 1

#include <libvpd-2/lsvpd_error_codes.hpp>
#include <rtascollector.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#ifdef HAVE_LIBRTAS
#include <librtas.h>
#include <fstream>
#endif

using namespace std;
namespace lsvpd {

#ifdef HAVE_LIBRTAS

	/**
	 * Based on rtas_ibm_get_vpd.c by
	 * @author Michael Strosaker <strosake@us.ibm.com>
	 * and @author Martin Schwenke <schwenke@au1.ibm.com>
	 *
	 * librtas_error
	 * @brief check for any librtas specific return codes
	 *
	 * This will check the error code provided to see if it matches any of
	 * the librtas specific return codes and add any text explaining the error
	 * to the buffer if a match is found.
	 *
	 * @param error return code from librtas call
	 * @param buf buffer to fill with librtas error message
	 * @param size size of buffer
	 */
	static string librtas_error(int error)
	{
		switch (error) {
		case RTAS_KERNEL_INT:
			return string("No kernel interface to firmware");
		case RTAS_KERNEL_IMP:
			return string("No kernel implementation of function");
		case RTAS_PERM:
			return string("Non-root caller");
		case RTAS_NO_MEM:
			return string("Out of heap memory");
		case RTAS_NO_LOWMEM:
			return string("Kernel out of low memory");
		case RTAS_FREE_ERR:
			return string("Attempt to free nonexistent RMO buffer");
		case RTAS_TIMEOUT:
			return string("RTAS delay exceeded specified timeout");
		case RTAS_IO_ASSERT:
			return string( "Unexpected I/O error");
		case RTAS_UNKNOWN_OP:
			return string( "No firmware implementation of function");
		default:
			string tmp = "Unknown librtas error: " + error;
			return tmp;
		}

	}

	string RtasCollector::rtasSystemParm(int code)
	{
		char buf[RTAS_BUF_SIZE];
		int ret = -1;

#ifdef DEBUGRTAS
		printf("Collecting RTAS info: [%d] %s\n", __LINE__, __FILE__);
#endif
		ret = rtas_get_sysparm(code, RTAS_BUF_SIZE, buf);

		if (ret == 0) {
			/*
			 * The first two bytes of the buffer contain the
			 * length of the data returned, including the
			 * terminating null.
			 */
			return string(buf+2);
		} else {
			return string();
		}
	}

	static void deleteList(struct rtas_buf_element *list)
	{
		struct rtas_buf_element *head = list, *next;

		while (head) {
			next = head->next;
			delete head;
			head = next;
		}
	}

	/**
	 * Calls into librtas, available only on Power64 systems, and generates
	 * a string of all available vpd data
	 * @param yl
	 *   The location code of the device to pass to RTAS, if this is an empty
	 * string, all of the system VPD will be stored in data.
	 * @param data
	 *   char** where we will put the vpd (*data should be NULL)
	 */
	int RtasCollector::rtasGetVPD(const string& yl = "",
				      char ** data = NULL)
	{
		int size = 0;
		struct rtas_buf_element *current, *list;
		unsigned int seq = 1, nextSeq;
		int rc;
		int vpd_changed = 0;
		char *locCode, *buf;

		list = new rtas_buf_element;

		if (!list)
			return -ENOMEM;

		memset(list->buf, '\0', RTAS_BUF_SIZE);

		list->size = 0;
		list->next = NULL;
		current = list;

		locCode = strdup(yl.c_str( ));

		if(locCode == NULL) {
			deleteList(list);
			return -ENOMEM;
		}

#ifdef DEBUGRTAS
		printf("Collecting RTAS info: [%d] %s\n", __LINE__, __FILE__);
#endif
		do {
			rc = rtas_get_vpd(locCode, current->buf, RTAS_BUF_SIZE,
					  seq, &nextSeq, &(current->size));

			switch (rc) {
			case CONTINUE:
				vpd_changed = 0;
				seq = nextSeq;
				current->next = new rtas_buf_element;

				if (!current->next) {
					deleteList(list);
					return -ENOMEM;
				}

				current = current->next;
				current->size = 0;
				current->next = NULL;
				/* fall through */
			case SUCCESS:
				break;
			case VPD_CHANGED:
				deleteList(list);
				vpd_changed ++;
				/*
				 * If we keep getting the VPD_CHANGED rc
				 * for more than a threshold, we quit, than
				 * looping forever.
				 */
				if (vpd_changed > VPD_CHANGED_THRESHOLD)
					return -RTAS_ERROR;
				seq = 1;
				list = new rtas_buf_element;
				if (!list)
					return -ENOMEM;
				list->size = 0;
				list->next = NULL;
				current = list;
				break;
			case PARAMETER_ERROR:
				deleteList(list);
				free(locCode);
				return -RTAS_PARAMETER_ERROR;
			case HARDWARE_ERROR:
				deleteList(list);
				free(locCode);
				return -RTAS_HARDWARD_ERROR;
			default:
				deleteList(list);
				free(locCode);
				librtas_error(rc);
				return -RTAS_ERROR;
			}

		} while(rc != SUCCESS);

#ifdef DEBUGRTAS
		printf("Done. [%d] %s\n", __LINE__, __FILE__);
#endif
		free(locCode);

		current = list;
		do {
			size += current->size;
		} while ((current = (current->next)) != NULL);

		current = list;
		*data = new char[ size ];
		if( *data == NULL ) {
			deleteList(list);
			return -ENOMEM;
		}
		memset( *data, '\0', size );

		buf = *data;
		do {
			memcpy(buf, current->buf, current->size);
			buf += current->size;
		} while ((current = (current->next)) != NULL);

		deleteList(list);

		return size;
	}

#else
	string RtasCollector::rtasSystemParm(int code)
	{
		return string();
	}

	int RtasCollector::rtasGetVPD(const string& yl, char **data)
	{
		return -1;
	}

#endif
}

