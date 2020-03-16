/***************************************************************************
 *   Copyright (C) 2006, IBM                                               *
 *                                                                         *
 *   Maintained by:                                                        *
 *   Eric Munson and Brad Peters                                           *
 *   munsone@us.ibm.com, bpeters@us.ibm.com                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef LSVPDSYSFSTREECOLLECTOR_H_
#define LSVPDSYSFSTREECOLLECTOR_H_

#include <icollector.hpp>
#include <fswalk.hpp>
#include <devicelookup.hpp>

#define SCSI_TEMPLATES_FILE "/etc/lsvpd/scsi_templates.conf"
#define NVME_TEMPLATES_FILE "/etc/lsvpd/nvme_templates.conf"

#include <string>

namespace lsvpd
{
	#define DEVICE_TYPE_SCSI "scsi"
	// These types use the SCSI layer too.
	#define DEVICE_TYPE_SATA       "sata"
	#define DEVICE_TYPE_ATA        "ata"
	#define DEVICE_TYPE_ISCSI      "iscsi"
	#define DEVICE_TYPE_FIBRE      "fibre"
	#define DEVICE_TYPE_RAID       "raid"
	#define DEVICE_TYPE_SCSI_DEBUG "scsi_debug"
	#define DEVICE_TYPE_IBMVSCSI   "ibmvscsi"
	#define DEVICE_TYPE_USB        "usb"

	#define DEVICE_SCSI_SG_DEFAULT_EVPD_LEN 252
	#define DEVICE_SCSI_SG_DEFAULT_STD_LEN 36
	#define MAXBUFSIZE 4096


	/*
	 * NVME admin command opcode for getting the log page as defined in
	 * linux header file (nvme.h).
	 */
	#define NVME_ADMIN_GET_LOG_PAGE		0x02

	/* NVME device ioctl return values */
	#define NVME_RC_SUCCESS			0x0
	#define NVME_RC_NS_NOT_READY		0x82
	#define NVME_RC_INVALID_LOG_PAGE	0x109

	#define NVME_NSID_ALL			0xffffffff

	/* NVME f1h log page VPD size */
	#define NVME_VPD_INFO_SIZE		1024

	/**
	 * SysFSTreeCollector contains the logic for device discovery and VPD
	 * retrieval from /sys and sg_utils.
	 *
	 * @class SysFSTreeCollector
	 *
	 * @ingroup lsvpd
	 *
	 * @author Eric Munson <munsone@us.ibm.com>,
	 *   Brad Peters <bpeters@us.ibm.com
	 */

	class SysFSTreeCollector : public ICollector
	{
		public:
			SysFSTreeCollector( bool limitSCSISize );
			~SysFSTreeCollector( );

			bool init( );

			int initializeCompHotplugValues(Component& dev,
								const char *devpath,
								const char *subsystem,
								const char *driver);
			int getDeviceData(Component& dev);

			void scsiGetHTBL(Component *fillMe);
			void usbGetHTBL(Component *fillMe);
			void ideGetHTBL(Component *fillMe);

			Component *fillByBus(const string& bus, Component* fillMe);
			Component *fillByDevClass(const string& devClass,
				Component* fillMe);

			/**
			 * Single arg fillComp() is used to fill those devices discovered
			 * through getComponents()
			 */
			Component* fillComponent(Component* fillMe);
			void initComponent( Component *newComp );

			vector<Component*> getComponents( vector<Component*>& devs );
			int numDevicesInTree(void);
			string myName(void);
			void fillSystem( System* sys );
			void postProcess( Component* comp ) {}
			string resolveClassPath( const string& path );

		private:
			string rootDir;
			FSWalk fsw;
			DeviceLookup* mPciTable;
			DeviceLookup* mUsbTable;
			bool mLimitSCSISize;

			// nvme specific
		        int load_nvme_templates(const string& filename);
			int interpretNVMEf1hLogPage(Component *fillMe, char *data);

			// scsi specific
			int load_scsi_templates(const string&);
			int interpretPage(Component *fillMe,
							char *data,
							int dataSize,
							int pageCode,
							string *pageFormat,
							int subtype,
							string *subtypeDS);

			void process_template(Component *fillMe, string *deviceType,
									char *data, int dataSize, string *format,
									int pageCode);

			int collectVpd(Component *fillMe, int device_fd, bool limitSCSISize);
			void fillSCSIComponent( Component* fillMe, bool limitSCSISize);
			string findGenericSCSIDevPath( Component *fillMe );
			void fillIPRData( Component *fillMe );
			void parseIPRData( Component *fillMe, string& output );
			int get_mm_scsi(Component *fillMe);
			string make_full_scsi_ds(Component *fillMe, string type,
				int subtype, char * data, int len);
			string make_basic_device_scsi_ds(Component *fillMe, string type,
				int subtype);

			bool setup(const string path_t );
			Component *findComponent(const vector<Component*> devs,
				string sysPath );
			vector<Component*> getComponentsVector( vector<Component*>& devs );
			vector<Component*> getComponentsVectorDevices( vector<Component*>& devs );
			Component * getInitialDetails(const string&, const string&);
			void findDevices(vector<Component*>&, const string&, const string&);
			void findDevicePaths(vector<Component*>&);

			int isDevice(const string& devDir);
			int filterDevice(const string& devName);
			int filterDevicePath(vector<Component*>& devs,
                                        const string& parentDir, const string& devName);
			void removeDuplicateDevices(vector<Component*>& devs);

			string getDevTreePath(string sysPath);
			string getClassLink( const string& sysDir );
			string getClassLink( const Component* comp );

			void findClassEntries( vector<Component*>&, const string& );
			void readClassEntries( vector<Component*>& devs );
			void readClassDevs( vector<Component*>& devs, const string& base,
				const string& cls );
			void readClassDevice( vector<Component*>& devs, const string& base,
				const string& cls, const string& dev );
			/**
			 * Merge attributes from child to parent device
			 */
			void mergeAttributes( Component *parent, Component *child );

			/**
			 * Set the devices' kernel name and device class by attempting to
			 * read the link from /sys/bus into /sys/class.
			 *
			 * @param fillMe
			 *   The Component to fill
			 * @param sysDir
			 *   The /sys dir for this component
			 */
			bool setKernelName( Component* fillMe, const string& sysDir );

			/**
			 * Fill a generic PCI device, this is also a catch-all for devices
			 * that do not fit anywhere else.
			 *
			 * @param fillMe
			 *   The Component to fill
			 * @param sysDir
			 *   The /sys dir for this component
			 */
			void fillPCIDev( Component* fillMe, const string& sysDir );

			/**
			 * Fill the Manufacturer name and Product description if the
			 * vendor and product ids are available under /sys.
			 *
			 * @param fillMe
			 *   The Component to fill
			 */
			void fillPCIDS( Component* fillMe );

			/**
			 * Fill a USB device, look for USB specific information (e.g. the
			 * vendor and device codes are in different files than a PCI
			 * device and are compared against a different table for values.)
			 *
			 * @param fillMe
			 *   The Component to fill
			 * @param sysDir
			 *   The /sys dir for this component
			 */
			void fillUSBDev( Component* fillMe, const string& sysDir );


			/**
			 * Fill a NVMe device.
			 *
			 * @param fillMe
			 */
			void fillNvmeClass( Component* fillMe );

			/**
			 * Fill a Net device.  At the moment, all this method does is fill
			 * the NetAddr field.
			 *
			 * @param fillMe
			 *   The Component to fill
			 * @param classDir
			 *   The /sys/class dir for this component
			 */
			void fillNetClass( Component* fillMe, const string& classDir );

			/**
			 * Attempt to read firmware version information from /sys
			 *
			 * @param fillMe
			 *   The Component to fill
			 */
			void fillFirmware( Component* fillMe );

			/**
			 * Fill and IDE device, read any IDE specific information.
			 *
			 * @param fillMe
			 *   The Component to fill
			 */
			void fillIDEDev( Component* fillMe );

			/**
			 * Gather PCI device specific vpd info.
			 *
			 * @param fillMe
			 *   The Component to fill
			 */
			void fillPciDevVpd( Component* fillMe );

			/**
			 * Parse device info in two parts : key & data.
			 *
			 * @param fillMe
			 * @param buf: vpd key and data list
			 * @param size : buffer size
			 */
			unsigned int parsePciVPDBuffer( Component* fillMe,
						        const char * buf , int size );
	};

	/**
	 * device_open
	 */
	int device_open(Component* fillMe);

}
#endif

