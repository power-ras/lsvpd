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
 *********************L******************************************************/
#ifdef TRACE_ON
	#include <libvpd-2/debug.hpp>
#endif
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _XOPEN_SOURCE 500 // For pread

#include <sysfstreecollector.hpp>

#include <libvpd-2/helper_functions.hpp>
#include <libvpd-2/debug.hpp>
#include <libvpd-2/logger.hpp>
#include <libvpd-2/lsvpd.hpp>
#include <libvpd-2/vpddbenv.hpp>

#include <linux/vfio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <libgen.h>		// for basename()
#include <stdint.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <cstring>
#include <linux/types.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <linux/hdreg.h>
#include <net/if_arp.h>
#include <net/if.h>

#include <deque>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#define MNIMI_INST 0x080280
#define ECID1 0x080588
#define MNIMI_DATA 0x0802C0

std::map<std::string, bool> g_deviceAccessible;

extern int errno;

using namespace std;
using namespace lsvpd;

extern VpdDbEnv *spyreDb;

namespace lsvpd
{
	void findDevicePaths(vector<Component*>& devs);

	SysFSTreeCollector::SysFSTreeCollector( bool limitSCSISize = false ) :
		mLimitSCSISize( limitSCSISize )
	{
		ifstream id;
		mPciTable = NULL;
		mUsbTable = NULL;

		id.open( DeviceLookup::getPciIds( ).c_str( ), ios::in );
		if( id )
		{
			mPciTable = new DeviceLookup( id );
			id.close( );
		}
		else
		{
			Logger logger;
			logger.log(
				   "pci.ids file not found, continuing but there will be a lot of missing information",
				   LOG_ERR );
		}

		id.open( DeviceLookup::getUsbIds( ).c_str( ), ios::in );
		if( id )
		{
			mUsbTable = new DeviceLookup( id );
			id.close( );
		}
		else
		{
			Logger logger;
			logger.log(
				   "usb.ids file not found, continuing but there will be a lot of missing information",
				   LOG_ERR );
		}
	}

	SysFSTreeCollector::~SysFSTreeCollector( )
	{
		if( mPciTable != NULL )
			delete mPciTable;

		if( mUsbTable != NULL )
			delete mUsbTable;
	}

	void SysFSTreeCollector::scsiGetHTBL(Component *fillMe)
	{
		string tmp = fillMe->sysFsNode.getValue();
		int loc, ploc;

		if ( (loc = tmp.rfind("/",tmp.length())) != (int) string::npos ) {
			tmp = tmp.substr(loc + 1, tmp.length());

			if ( (loc = tmp.find(":", 0)) != (int) string::npos)
				fillMe->addDeviceSpecific("XH", "SCSI Host",
							  tmp.substr(0, loc), 60);

			ploc = loc + 1;
			if ( (loc = tmp.find(":", ploc)) != (int) string::npos)
				fillMe->addDeviceSpecific("XB", "SCSI Bus",
							  tmp.substr(ploc, loc - ploc), 60);
			ploc = loc + 1;
			if ( (loc = tmp.find(":", ploc)) != (int) string::npos)
				fillMe->addDeviceSpecific("XT", "SCSI Target",
							  tmp.substr(ploc, loc - ploc), 60);
			ploc = loc + 1;
			fillMe->addDeviceSpecific("XL", "SCSI Lun",
						  tmp.substr(ploc, tmp.length() - ploc), 60);

		}
	}
	void SysFSTreeCollector::ideGetHTBL(Component *fillMe)
	{
		string tmp = fillMe->sysFsNode.getValue();
		int loc;

		if ( (loc = tmp.rfind("/",tmp.length())) != (int) string::npos )
			fillMe->addDeviceSpecific("XI", "IDE Device Identifier",
						  tmp.substr(loc + 1, tmp.length()), 60);

	}

	void SysFSTreeCollector::usbGetHTBL(Component *fillMe)
	{
		/* coutd << "USB: Cur Dev Node = " <<  fillMe->sysFsNode.getValue() << endl; */
	}

	/**
	 * @brief: Collects vpd for devices specified by bus.  This is necessary
	 *	since the bus of a device can determine the methods used to
	 *	collect vpd.
	 * @arg bus: The devices bus, as seen at /sys/bus/XXX
	 * @arg fillMe: Device component to be filled
	 */
	Component *SysFSTreeCollector::fillByBus(const string& bus,
						 Component* fillMe)
	{
		/* Component is a scsi device - fill accordingly */
		if(bus  == "scsi" )
		{
			fillMe->mManufacturer.setValue( getAttrValue(
				fillMe->sysFsLinkTarget.getValue() , "vendor" ),
							50, __FILE__, __LINE__);

			fillMe->mDescription.setValue( getAttrValue(
				fillMe->sysFsLinkTarget.getValue() , "device" ),
						       50, __FILE__, __LINE__);

			fillSCSIComponent(fillMe, mLimitSCSISize);  /* Fills SH, ST, SB, SL */

		}
		else if( bus == "usb" )
		{
			fillUSBDev( fillMe, fillMe->sysFsLinkTarget.getValue() );
			/*
			 * Since usb storage devices are treated as SCSI devices,
			 * we can get useful information from sg_utils
			 */
			fillSCSIComponent(fillMe, mLimitSCSISize);
			usbGetHTBL(fillMe);  /*Get identifiers for device */
		}
		else if( bus == "ide" )
		{
			fillIDEDev( fillMe );
			ideGetHTBL(fillMe);  /* Get identifiers for device */
		}
		else
		{
			fillPCIDev( fillMe, fillMe->sysFsLinkTarget.getValue() );
		}

		return fillMe;
	}

	/**
	 * @brief: Fill a device component by class.
	 */
	Component *SysFSTreeCollector::fillByDevClass(const string& devClass,
						      Component* fillMe)
	{
		string classLink;
		string msg;

		/* Here we need to do any of the class specific filling necessary.
		 * the device class was populated above when the Kernel name was set.
		 */

		if( devClass == "net" )
		{
			classLink = fillMe->getClassNode();
			if (classLink.length() > 0)
				fillNetClass( fillMe,  classLink);
		} else if (devClass == "nvme")
		{
			classLink = fillMe->getClassNode();
			if (classLink.length() > 0)
				fillNvmeClass(fillMe);
		}

		return fillMe;
	}

	/**
	 * This is the second biggest job for any collector.  Now that we have
	 * identified all the unique devices available on the system, the
	 * collector will attempt to fill out all the device information
	 * available from its data source for the device passed in as 'fillMe'.
	 */
	Component* SysFSTreeCollector::fillComponent( Component* fillMe )
	{
		initComponent(fillMe);

		if( fillMe->sysFsLinkTarget.getValue() == "" )
		{
			return fillMe;
		}

		fillByBus(fillMe->devBus.getValue(), fillMe);

		fillByDevClass(fillMe->getDevClass(), fillMe);

		fillFirmware( fillMe );

		ostringstream os;

		/* No /sys entry - try to build from parent */
		if( fillMe->mpParent != NULL &&
		    fillMe->mpParent->mPhysicalLocation.dataValue != "" )
		{
			if( fillMe->mpParent->mPhysicalLocation.dataValue[ 0 ] != 'U' )
				os << fillMe->mpParent->mPhysicalLocation.dataValue << "/";
			else if( fillMe->mpParent->devBusAddr.dataValue != "" )
				os << fillMe->mpParent->devBusAddr.dataValue << "/";
		}

		os << fillMe->devBusAddr.dataValue;

		fillMe->mPhysicalLocation.setValue( os.str( ), 50 , __FILE__, __LINE__);

		if( fillMe->mDescription.dataValue == "" &&
		    fillMe->mModel.dataValue != "" )
			fillMe->mDescription.setValue( fillMe->mModel.dataValue,
						       1 , __FILE__, __LINE__);

		return fillMe;
	}

	void SysFSTreeCollector::initComponent( Component* in )
	{
	}

	/**
	 * For SysFSTreeCollector, init will test to see if path_t is,
	 *  available.  If it is not, we cannot use this collector, as the
	 *  /sys file system is lacking.
	 * Note: This will substantially and negatively impact device
	 *	data collection.
	 */
	bool SysFSTreeCollector::setup(const string path_t )
	{
		struct stat statbuf;
		bool ret;

		if (stat(path_t.c_str(), &statbuf) != 0)
			ret = false;
		else {
			rootDir = path_t;
			fsw = FSWalk(rootDir);
			ret = true;
		}

		return ret;
	}

	bool SysFSTreeCollector::init()
	{
		return setup("/sys/devices");
	}

	/**
	 * Walkes a vector of component ptr's, looking for a particular one
	 * @sysPath: The /sys/devices path of the device being sought
	 * @return the specified component, or NULL on failure
	 */
	Component *SysFSTreeCollector::findComponent(const vector<Component*> devs,
						     string sysPath )
	{
		for (int i = 0; i < (int) devs.size(); i++) {
			if (devs[i]->sysFsNode.getValue() == sysPath) {
				return devs[i];
			}
		}
		return NULL;
	}

	/* Set the parent attribute to child attribute if the former is empty */
#define mergeField(parent, child, item, force) \
	do { \
		if (force || parent->item.getValue() == "") \
		parent->item.setValue(child->item.getValue(), INIT_PREF_LEVEL, __FILE__, __LINE__); \
	} while (0)

	/**
	 * Merges attributes collected in Child node into the Parent node
	 * so that the duplicate node Child can be removed.
	 */
	void SysFSTreeCollector::mergeAttributes(Component *parent, Component *child)
	{
		mergeField(parent, child, mDevClass, 0);
		mergeField(parent, child, devBus, 0);
		mergeField(parent, child, devBusAddr, 0);
	}

	/*
	 * Remove single child device node of a parent
	 * Conditions:
	 * 1) There is only one child for this device
	 * 2) The child device is a leaf node
	 * 3) The child devNode name is in our AIXNames list.
	 */

	void SysFSTreeCollector::removeDuplicateDevices(
							vector<Component*>& devs )
	{
		vector <string> children;
		vector <Component*>::iterator parent,tmp;
		string devName;

		/* Remove duplicate devices */

		for (parent = devs.begin(); parent < devs.end(); parent++) {
			Component *childDev;
			string child;
			char *tchild;

			children = (*parent)->getChildren();

			/* Rule 1 */
			if (children.size() != 1)
				continue;

			child = children[0];
			childDev = findComponent(devs, child);
			/* Rule 2 */
			if (childDev == NULL || childDev->getChildren().size() != 0)
				continue;
			tchild = strdup(child.c_str());
			if (tchild == NULL)
				return;
			devName = basename(tchild);

			/* Rule 3 */
			if (!HelperFunctions::contains((*parent)->getAIXNames(), devName)) {
				free(tchild);
				continue;
			}
			/* Delete the child node */
			for (tmp = parent; tmp < devs.end(); tmp++)
				if ((*tmp)->sysFsNode.getValue() == child) {
					/* Remove the device from the device list */
					mergeAttributes(*parent, *tmp);
					devs.erase(tmp);
					(*parent)->removeChild(child);
					break;
				}
			free(tchild);
		}
	}




	/* Creates a vector of components, representing all devices this
	 * collector is aware of (ie - those discovered in the /sys tree),
	 * and back-walks tree to determine inheritance
	 * @arg devs: An empty vector, which will be filled with one Component
	 *	      for each device on the system.
	 */
	vector<Component*> SysFSTreeCollector::getComponents(
							     vector<Component*>& devs )
	{
		Component *dev, *parent;
		string devNode;
		int i;

		/*		devs = getComponentsVector(devs); */
		findDevicePaths(devs);

		for (i = (devs.size() - 1); i >= 0; i--) {
			dev = devs[i];
			devNode = dev->sysFsNode.getValue();
			/*
			 * Need to set up sysFsClassNode constituents - ie  mDevClass and
			 * mAIXName
			 */
			setKernelName(dev, devNode);

			if (dev->mParent.getValue().length() > 0) {
				/* Setup all dev links for discovered devices */
				parent = findComponent(devs, dev->mParent.getValue());
				if (parent != NULL) {
					parent->addChild(dev->idNode.getValue());
				}
				else {
					cout << "Error: Failed to find parent: '" << dev->mParent.getValue()
						<< "' For dev device: '" << dev->sysFsNode.getValue() << "'" << endl;
				}
			}

			if (dev->devBus.getValue() == "scsi")
				scsiGetHTBL(dev);  /* Get Host:Target:Bus:Lun data for device */
			/*
			 * If our syfsdir has a "device" link, that is our physical device.
			 * Add our name to the list of AX names for the target device.
			 */
			string link = string(devNode + "/device");
			if (FSWalk::fs_isLink(link)) {
				char linkTarget[PATH_MAX];
				Component *targetDev;

				if (realpath( link.c_str(), linkTarget ) == NULL) {
					string msg = string("realpath operation")
					 + string(" got failed on ") + link;
					Logger().log(msg, LOG_ERR);
					continue;
				}

				string targetDevPath = string(linkTarget);
				targetDev = findComponent(devs, targetDevPath);
				if (targetDev != NULL) {
					/* get the device name */
					char *tmp = strdup(devNode.c_str());
					if(tmp == NULL) {
						string msg = string("strdup")
						+ string(" failed on ")
						+ devNode;
						Logger().log(msg, LOG_ERR);
						continue;
					}

					/* Add 'name' to the AIXName list
					 * for targetDev */
					targetDev->addAIXName(basename(tmp), 90);
					free(tmp);
				}
			}

		}

		removeDuplicateDevices(devs);

		/*
		 * This case indicates that we are running on a older kernel that does
		 * not maintain the links from /sys/pci/XXX/devices/XXX into the
		 * corresponding /sys/class/XXX/XXX directory so we cannot use the
		 * above method for finding kernel names, we must fall back on
		 * the method found in scripts/scan.d/[03adapter|07device].
		 */

		/* If this kernel does not provide links to the class dir,
		 * we will try a backup approach to filling AX.
		 *
		 * This function now works to walk the entire tree of directories just once,
		 * while walking the device list N times.  This should substantially
		 * cut down on processing time as device number << directories */

		readClassEntries( devs );

		return devs;
	}

	/**
	 * getInitialDetails
	 * @brief Given just a sys/devices node, collect whatever high-level
	 *	details can be immediately collected.  All more detailed,
	 *	time consuming (> O(1)), and/or device intra-dependant data should be
	 *  collected in fillComponent.
	 * @param fillMe: Empty component
	 * @param parent: Default parent of any devices discovered in this dir
	 */
	Component * SysFSTreeCollector::getInitialDetails(const string& parentDir,
							  const string& newDevDir)
	{
		string link;
		string absTargetPath, tmp, type;
		string devName;
		Component *fillMe;
		int locBeg, locEnd;
		int lastSlash;

		fillMe = new Component();
		if ( fillMe == NULL )
			return NULL;
		fillMe->sysFsNode.setValue(newDevDir, INIT_PREF_LEVEL, __FILE__, __LINE__);
		fillMe->sysFsLinkTarget.setValue(newDevDir, INIT_PREF_LEVEL, __FILE__, __LINE__);
		fillMe->idNode.setValue(newDevDir, INIT_PREF_LEVEL, __FILE__, __LINE__);

		lastSlash = newDevDir.rfind("/", newDevDir.length()) + 1;
		devName = newDevDir.substr(lastSlash, newDevDir.length() - lastSlash);

		/* Set a basic description for standard devices */
		locBeg = 1;
		for (int i = 0; i < 2; i++)
			locBeg = fillMe->sysFsNode.getValue().find("/",
								   locBeg+1);
		locEnd = fillMe->sysFsNode.getValue().find("/",
							   locBeg+1);

		tmp = fillMe->sysFsNode.getValue().substr(locBeg + 1, locEnd - locBeg -1);
		if (tmp == "virtual") {
			fillMe->mDescription.setValue("Virtual",
						      1, __FILE__, __LINE__);
		}
		else if (fillMe->sysFsNode.getValue().find("acpi", 0) != string::npos) {
			fillMe->mDescription.setValue("Advanced Configuration and Power Interface",
						      1, __FILE__, __LINE__);
		}
		else if (tmp == "vio") {
			fillMe->mDescription.setValue("Virtual I/O Device",
						      1, __FILE__, __LINE__);
		}
		else if (tmp == "system") {
			fillMe->mDescription.setValue("System Component",
						      1, __FILE__, __LINE__);
		}
		else if (tmp == "ibmebus") {
			fillMe->mDescription.setValue("IBM EBUS Device",
						      1, __FILE__, __LINE__);
		}
		else if (tmp == "platform") {
			fillMe->mDescription.setValue("Platform",
						      1, __FILE__, __LINE__);
		}
		else if (HelperFunctions::matches("pnp*", tmp)) {
			fillMe->mDescription.setValue("Plug and Play Port - Unused",
						      1, __FILE__, __LINE__);
		}

		/* Set parentage */
		if (parentDir.length() > 0)
			fillMe->mParent.setValue(parentDir, INIT_PREF_LEVEL,
						 __FILE__, __LINE__);
		else
			fillMe->mParent.setValue("/sys/devices", INIT_PREF_LEVEL,
						 __FILE__, __LINE__);

		link = string ("");
		/* Look for either a /bus entry or, for pci, /subsystem is the same */
		if (HelperFunctions::file_exists(string (fillMe->sysFsNode.getValue() + "/bus")))
			link = string (fillMe->sysFsNode.getValue() + "/bus");
		else if (HelperFunctions::file_exists(string (fillMe->sysFsNode.getValue() + "/subsystem")))
			link = string (fillMe->sysFsNode.getValue() + "/subsystem");

		/* Get bus name if available */
		if (HelperFunctions::file_exists(link)) {
			if (FSWalk::fs_isLink(link)) {
                                char linkTarget[PATH_MAX];

				if (realpath( link.c_str(), linkTarget ) == NULL) {
					string msg = string("realpath operation")
						+ string(" got failed on ")
						+ link;
                                        Logger().log(msg, LOG_ERR);
					goto esc_subsystem_info;
                                }
                                string absTargetPath = string(linkTarget);

				/* 
				 * Grab last 2 parts of link.
				 * The link is either
				 *   /sys/bus/<bus_name>/
				 * 	OR
				 *   /sys/class/<dev_class>/
				 *
				 * Find the type and the value of the associated type.
				 * Update the values accordingly in the component.
				 */
				locBeg = absTargetPath.find("/", 2);
				locEnd = absTargetPath.find("/", locBeg+1);
				type = absTargetPath.substr(locBeg + 1, locEnd - (locBeg + 1));

				/* Now find value */
				locBeg = locEnd + 1;
				locEnd = absTargetPath.find("/", locBeg);
				tmp = absTargetPath.substr(locBeg, locEnd - locBeg -1);

				if (type == "bus")
					fillMe->devBus.setValue(tmp, INIT_PREF_LEVEL, __FILE__, __LINE__);
				else if (type == "class")
					fillMe->mDevClass.setValue(absTargetPath + "/" + devName, 
								   INIT_PREF_LEVEL, __FILE__, __LINE__);
				else {
					Logger l;
					string s = "Unknown type (" + type + ") while processing" + newDevDir;
					l.log(s, LOG_WARNING);
				}

				/* Add detail to basic descriptions */
				if (tmp == "tty") {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								      + " Terminal Device",
								      2, __FILE__, __LINE__);
				}
				else if (tmp == "graphics") {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								      + " Graphical Device",
								      2, __FILE__, __LINE__);
				}
				else if (tmp == "input") {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								      + " Input Device",
								      2, __FILE__, __LINE__);
				}
				else if (tmp == "net") {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								      + " Networking Device",
								      2, __FILE__, __LINE__);
				}
				else if (tmp == "cpu") {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								      + " - Processing Device",
								      2, __FILE__, __LINE__);
				}
				/*
				 * FIXME: This is a problem, this case will never be true, should
				 * it be removed or should the test be changed?
				 * else if (tmp == "cpu") {
				 * fillMe->mDescription.setValue(fillMe->mDescription.getValue()
				 * + " - Time Device",
				 * 2, __FILE__, __LINE__);
				 * }
				 */
				else if (tmp == "mem") {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								      + " Memory Device",
								      2, __FILE__, __LINE__);
				}
			}
		}

esc_subsystem_info:

		if (fillMe->mDescription.getValue() == "Virtual") {
			fillMe->mDescription.setValue("Virtual Device",
						      2, __FILE__, __LINE__);
		}

		if (fillMe->mDescription.getValue() == "Platform") {
			fillMe->mDescription.setValue("Platform Device",
						      2, __FILE__, __LINE__);
		}

		/* Looking for device driver link */
		link = string (fillMe->sysFsNode.getValue() + "/driver");
		if (HelperFunctions::file_exists(link)) {
			if (FSWalk::fs_isLink(link)) {
				char linkTarget[PATH_MAX];
				string driver;

				if (realpath( link.c_str(), linkTarget ) != NULL) {
					string absTargetPath = string(linkTarget);

					/* Now grab last part of link */
					lastSlash = absTargetPath.rfind("/",
						absTargetPath.length()) + 1;
					driver = absTargetPath.substr(lastSlash,
							absTargetPath.length() - lastSlash);
					fillMe->devDriver.setValue(driver,
								   INIT_PREF_LEVEL,
								   __FILE__, __LINE__);

					if (driver == "hvc_console") {
						fillMe->addAIXName(driver,90);
						fillMe->mDescription.setValue("Hypervisor Virtual Console",
								90, __FILE__, __LINE__);
					}
				} else {
					string msg = string ("realpath operation")
						+ string(" got failed on ") + link;
					Logger().log(msg, LOG_ERR);
				}
			}
		}

		/* Look for generic name for the scsi device pointed to by 'generic' link */
		link = string (fillMe->sysFsNode.getValue() + "/generic");
		if (FSWalk::fs_isLink(link)) {
			char linkTarget[512];
			int len, start;
			string name;

			len = readlink(link.c_str(), linkTarget,
				       sizeof(linkTarget) - 1);
			if (len > 0) {
				linkTarget[len] = '\0';

				name = linkTarget;
				start = name.rfind("/", name.length()) + 1;
				fillMe->addAIXName(name.substr(start,
						name.length() - start), 90);
			}
		}


		/*
		 * VIO devices have a better name available in the 'name' than the
		 * 'numeric' sysfs node name. Use it whenever available.
		 */
		if (fillMe->devBus.getValue() == "vio" &&
		    HelperFunctions::file_exists(newDevDir + "/name")) {
			string name = getAttrValue(newDevDir, "name");
			if (name != "")
				devName = name;
		}

		fillMe->devSysName.setValue(devName, INIT_PREF_LEVEL, __FILE__, __LINE__);
		fillMe->devBusAddr.setValue(devName, INIT_PREF_LEVEL, __FILE__, __LINE__);

		/* devName seems to be a very good AIX name -
		 * adding as such, so that if no other, more preferred name is
		 * found, we can use this. */

		fillMe->addAIXName(devName, INIT_PREF_LEVEL - 1);
		/* Pointer from sysfs -> device-tree node */
		link = getDevTreePath(fillMe->sysFsNode.getValue());
		if (link.length() > 0)
		{
			fillMe->deviceTreeNode.setValue(
							"/proc/device-tree" + link,
							INIT_PREF_LEVEL - 1,
							__FILE__, __LINE__);
		}

		return fillMe;
	}

	/**
	 * isDevice
	 * @brief Check if the dir corresponds to a device by checking if it has
	 * a "uevent" file in it.
	 * @param deviceDir Path to the sysfs directory
	 * @return True if the dir represents a device, else false.
	 */
	int SysFSTreeCollector::isDevice(const string& deviceDir)
	{
		struct stat statBuf;
		string tmp = deviceDir + "/uevent";
		string part = deviceDir + "/partition";

		/* If we don't have uevent */
		if (stat(tmp.c_str(), &statBuf) != 0)
			return 0;
		/* If our device is a partition */
		if (stat(part.c_str(), &statBuf) == 0)
			return 0;
		return 1;
	}

	/**
	 * filterDevice
	 * @brief Filter the nodes which need not be listed in VPD.
	 * As of now here is the list of excluded nodes :
	 *   target[0-9]+
	 *
	 * @param devName - device name
	 * @return True if this device needs to be added.
	 * TODO: Use regex for scalability
	 */
	int SysFSTreeCollector::filterDevice(const string& devName)
	{
		int id;
		const char *s = devName.c_str();
		if (sscanf(s, "target%d", &id) == 1)
			return 0;
		return 1;
	}

	/**
	 * filterDevicePath
	 * @brief Filter out sysfs nodes known to be redundant or of no use.
	 *
	 * 1) Any scsi device will have a generic access to it provided via, sgN.
	 * We don't need this in our database, as we are interested only in the default
	 * access for the device(like, sdX, srN etc.)
	 *
	 * 2) Following dirs under /sys/devices/
	 *	virtual, system, cpu, breakpoint, tracepoint, software
	 *
	 */
	int SysFSTreeCollector::filterDevicePath(vector<Component*>& devs,
						 const string& parentDir,
						 const string& devName)
	{
		string bus;
		Component *parentDev;


		if (parentDir == "") {
			/* Dirs under /sys/devices/ */
			if (devName == "virtual" ||
			    devName == "system" ||
			    devName == "cpu" ||
			    devName == "breakpoint" ||
			    devName == "tracepoint" ||
			    devName == "software")
				return 0;
			else
				return 1;
		}

		parentDev = findComponent(devs, parentDir);

		if (parentDev == NULL) {
			Logger log;
			string msg = "Unable to find parent device corresponding to " + parentDir;
			log.log(msg, LOG_WARNING);
			return 1;
		}

		bus = parentDev->devBus.getValue();
		if (bus == "scsi") {
			/* For a scsi device, we can skip the following nodes:
			 * scsi_generic: which provides generic access.
			 * enclosure: which provides info about scsi enclosures.
			 */
			if (devName == "scsi_generic" || devName == "enclosure")
				return 0;
		}

		return 1;
	}

	/**
	 * findDevices
	 * @brief Starting at a device, recursively search for and fill out any
	 *	  child devices.
	 * @param devs Partially filled vector - discovery process will add
	 *	  devices found to this vector
	 * @param parent This will be parent device of devices discovered in
	 *	  'parent' directory.  May be NULL
	 */
	void SysFSTreeCollector::findDevices(vector<Component*>& devs,
					     const string& parentDir,
					     const string& searchDir)
	{
		FSWalk fsw = FSWalk();
		string newDevDir;
		vector<string> listing;
		Component *tmpDev;
		string devName, parentDev;
		char *parent;

		parent = strdup(parentDir.c_str());
		if ( parent == NULL )
			return;
		parentDev = string(basename(parent));
		free(parent);

		fsw.fs_getDirContents(searchDir, 'd', listing);
		while (listing.size() > 0)
		{
			devName = listing.back();
			newDevDir = searchDir + "/" + devName;
			listing.pop_back();

			/* If dir entry is a directory, it may be a device */
			if (FSWalk::fs_isDir(newDevDir)) {

				/* Last check - if dir == one of a few known to exist for
				 * each device, this is not a new device */
				if ((parentDev != devName) &&
				    isDevice(newDevDir) &&
				    filterDevice(devName)) {
					/* Found device */
					tmpDev = getInitialDetails(parentDir, newDevDir);
					if ( tmpDev != NULL )
						devs.push_back(tmpDev);
					findDevices(devs, newDevDir, newDevDir);
				} else if(filterDevicePath(devs, parentDir, devName))
					findDevices(devs, parentDir, newDevDir);
			}
		}
	}

	/**
	 * findDevicePaths( vector<Component*>& devs ,
	 *	Component *parent)
	 *
	 * @brief Recursively walks /sys/devices dirs, ID's all devices,
	 * adds path and other relevant data to the components vector.
	 * Must call fillComponent on each returned component to obtain
	 * full details
	 *
	 * @param devs Empty vector which will be filled with all devices this
	 * collector knows how to elucidate
	 *
	 * @param parent Parent component to all devices discovered in parent's
	 *	dir
	 */
	void SysFSTreeCollector::findDevicePaths(vector<Component*>& devs)
	{
		vector<string> fullList;
		string curPath, devPath;
		FSWalk fsw = FSWalk();

		/* Full list of the various categories of devices */
		fsw.fs_getDirContents("/sys/devices", 'd', fullList);
		while (fullList.size() > 0 )
		{
			devPath = fullList.back();
			fullList.pop_back();

			if (!filterDevicePath(devs, "", devPath))
				continue;
			curPath = "/sys/devices/" + devPath;
			if (FSWalk::fs_isDir(curPath)) {
				findDevices(devs, "", curPath);
			}
		}
	}

	/**
	 * Retrieves the path to the device-tree representation of sysFS device
	 * at sysPath, if it exists.  Otherwise, returns string("")
	 */
	string SysFSTreeCollector::getDevTreePath(string sysPath)
	{
		string sysFsPath, procDtPath, firmwareDtBase;
		struct stat astats;
		const char *buf;
		char buf2[PATH_MAX];
		FILE *fi;

		HelperFunctions::fs_fixPath(sysPath);
		sysFsPath = sysPath + "/devspec";
		procDtPath = sysPath + "/of_node";
		firmwareDtBase = "/sys/firmware/devicetree/base";

		/*
		 * Check for existence of of_node symlink and return the path it
		 * points to.  devspec is obsolete now, most of the devices
		 * populate of_node and devspec too. Prefer of_node over
		 * devspec, where it exist and fall back to older logic, in case
		 * of of_node not populated.
		 */
		buf = procDtPath.c_str();
		if ((lstat(buf, &astats)) == 0) {
			// of_node is symlink, follow the link
			if ( realpath( buf, buf2 ) == NULL ) {
				return string ("");
			}

			/*
			 * Trim the leading "/sys/firmware/devicetree/base" from the real
			 * path to match the assumption from devspec format, while used
			 * with /proc/devicetree.
			 */
			procDtPath = string(buf2);
			procDtPath.replace(0, firmwareDtBase.length(), "");
			return procDtPath;
		}

		buf = sysFsPath.c_str();
		if ((lstat(buf, &astats)) != 0) {
			return string("");
		}

		// Read Results
		fi = fopen(buf , "r");
		if (!fi)
			return string("");

		if (!fgets(buf2, sizeof(buf2), fi)) {
			fclose(fi);
			return string("");
		}

		if (buf2[strlen(buf2) - 1] == '\n')
			buf2[strlen(buf2) - 1] = '\0';

		fclose(fi);
		// cleanup
		if (strcmp(buf2, "(null)"))
			return string(buf2);

		return string("");
	}

	/* Collects devices on system, then returns how many were found.
	*/
	int SysFSTreeCollector::numDevicesInTree()
	{
		vector<Component*> devs;

		findDevicePaths(devs);
		return devs.size();
	}

	/* Name of this collector */
	string SysFSTreeCollector::myName(void)
	{
		return string("SysFSTreeCollector");
	}

	/**
	 * @brief: Walks thru all links in this directory, looking for one
	 *	with a ':' in the name.  This one has always been the back
	 *	link to the /sys/class dir, where more info is gathered.
	 * Note: This may break on occassion, so a better method should be
	 *	investigated.
	 * @arg: The directory in which the device was found
	 */
	string SysFSTreeCollector::getClassLink( const string& sysDir )
	{
		Logger logger;
		struct dirent* entry;
		DIR* d;
		string link;

		string ret = "";

		d = opendir( sysDir.c_str( ) );
		if( d == NULL ) {
			return ret;
		}

		while( ( entry = readdir( d ) ) != NULL )
		{
			string fname = entry->d_name;
			if( HelperFunctions::countChar( fname, ':' ) == 1 )
			{
				char *f, *p;
				f = strdup( fname.c_str( ) );
				if (f == NULL) {
					closedir(d);
					return NULL;
				}
				p = strdup( sysDir.c_str( ) );
				if ( p == NULL ) {
					free (f);
					closedir(d);
					return NULL;
				}
				link = HelperFunctions::getAbsolutePath( f, p );
				free( f );
				free( p );
				break;
			}
		}
		closedir( d );
		return ret;
	}

	/* Attempts a best effort collection of AIX name, of AX
	 * @param sysDir: Dir to start a directory walk, looking for
	 *		link fitting the known class link format (ie ':' present)
	 * @return bool: Whether the AX name was successfully filled
	 */
	bool SysFSTreeCollector::setKernelName( Component* fillMe,
						const string& sysDir )
	{
		Logger logger;
		struct dirent* entry;
		DIR* d;
		bool filled = false;

		d = opendir( sysDir.c_str( ) );
		if( d == NULL )
		{
			if (sysDir.length() > 0) {
				ostringstream os;
				os << "Unable to open requested dir '" << sysDir << "'";
				logger.log( os.str( ), LOG_WARNING );
			}
			return true;
		}

		while( ( entry = readdir( d ) ) != NULL )
		{
			string fname = entry->d_name;
			int idx;

			if( HelperFunctions::countChar( fname, ':' ) == 1 )
			{
				idx = fname.find( ':' );
				idx++;
				if( !HelperFunctions::contains( fillMe->mAIXNames,
								fname.substr( idx ) ) )
				{
					ostringstream dirName;
					dirName << "/sys/class/" << fname.substr( 0, idx - 1 ) <<
						"/" << fname.substr( idx );
					struct stat info;
					if( stat( dirName.str( ).c_str( ), &info ) == 0 )
					{
						filled = true;
						fillMe->addAIXName( fname.substr( idx ), 90 );
					}
					else if( fname.substr( 0, idx - 1 ) == "block" )
					{
						filled = true;
						fillMe->addAIXName( fname.substr( idx ), 90 );
					}
				}
			}
		}
		closedir( d );
		return filled;
	}

	/**
	 * readClassEntries
	 * @brief Top level call in to discover links from /sys/class and
	 *	/sys/block to a device.  These are collected as AIX names
	 */
	void SysFSTreeCollector::readClassEntries( vector<Component*>& devs )
	{
		findClassEntries(devs, "/sys/class");
		findClassEntries(devs, "/sys/block");
	}

	/**
	 * findClassEntries
	 * @brief Walks through all of /sys/class and /sys/block, looking
	 *	for links into known devices.  If found, we grab AIX names here
	 *  and store the class dir for the device.
	 */
	void SysFSTreeCollector::findClassEntries( vector<Component*>& devs,
						   const string& searchDir)
	{
		FSWalk fsw = FSWalk();
		string dev, devDir;
		vector<string> listing;
		Component *tmp;
		string target;

		fsw.fs_getDirContents(searchDir, 'l', listing);
		while (listing.size() > 0)
		{
			tmp = NULL;
			dev = listing.back();
			listing.pop_back();
			devDir = searchDir + "/" + dev;
			if (FSWalk::fs_isLink(devDir)
			    && ((dev == "device") || (dev == "bridge"))) {
				target = HelperFunctions::getSymLinkTarget(devDir);
				tmp = findComponent(devs, target);
				if (tmp == NULL)
					continue;

				/* Set newly discovered class dir */
				tmp->mDevClass.setValue(searchDir, INIT_PREF_LEVEL,
							__FILE__, __LINE__);
				/* store AIX name */
				int lastSlash = searchDir.rfind("/",
							searchDir.length()) + 1;

				target = searchDir.substr(lastSlash,
						searchDir.length() - lastSlash);

				tmp->addAIXName(target, INIT_PREF_LEVEL + 1);
			}
			else if ((FSWalk::fs_isDir(devDir))
				 && (!FSWalk::fs_isLink(devDir))) {
				/* recurse on all directories */
				findClassEntries(devs, devDir);
			}
		}
	}

	string findIOCTLAIXEntry(Component * fillMe)
	{
		string fin;
		int fd;
		vector<DataItem*>::const_iterator i, end;
		i = fillMe->getAIXNames().begin();
		end = fillMe->getAIXNames().end();

		while (i != end) {
			fin = string("/dev/") + (*i)->getValue();
			fd = open( fin.c_str( ), O_RDONLY | O_NONBLOCK );
			if( fd < 0 )
				i++;
			else {
				close(fd);
				return fin;
			}
		}
		return string("");
	}

	void SysFSTreeCollector::fillIDEDev( Component* fillMe )
	{
		if( fillMe->mAIXNames.empty( ) ) {
			return;
		}

		ostringstream os;
		string val;
		val = fillMe->mAIXNames[ 0 ]->dataValue;
		os << fillMe->sysFsLinkTarget.dataValue << "/modalias";
		ifstream in;
		in.open( os.str( ).c_str( ) );
		if( in ) {
			val = "";
			in >> val;
			in.close( );
			if( val.find( "cdrom" ) != string::npos )
			{
				fillMe->mDescription.setValue( "IDE Optical Drive", 80,
							       __FILE__, __LINE__);
			}
			else if( val.find( "disk" ) != string::npos )
			{
				fillMe->mDescription.setValue( "IDE Disk Drive", 80,
							       __FILE__, __LINE__);
			}
			else if( val.find( "tape" ) != string::npos )
			{
				fillMe->mDescription.setValue( "IDE Tape Drive", 80,
							       __FILE__, __LINE__) ;
			}
			else if( val.find( "floppy" ) != string::npos )
			{
				fillMe->mDescription.setValue( "IDE Floppy Drive", 80,
							       __FILE__, __LINE__) ;
			}
			else
			{
				fillMe->mDescription.setValue( "IDE Unknown Device", 80,
							       __FILE__, __LINE__) ;
			}
		}

		os.str(HelperFunctions::findAIXFSEntry(fillMe->getAIXNames(), "/dev/"));
		struct hd_driveid id;
		memset( &id, 0, sizeof( struct hd_driveid ) );
		int fd = open( os.str( ).c_str( ), O_RDONLY | O_NONBLOCK );
		if( fd < 0 )
		{
			Logger logger;
			string msg = "Failed to open " + os.str( );
			logger.log( msg, LOG_WARNING );
			return;
		}

		if( ioctl( fd, HDIO_GET_IDENTITY, &id ) != 0 )
		{
			Logger logger;
			logger.log( "SysFsTreeCollector.fillIDEDev: ioctl call failed.",
				    LOG_WARNING );
			close( fd );
			return;
		}
		close( fd );

		val = (char*)id.model;
		if( val != "" )
		{
			fillMe->mModel.setValue( val, 50, __FILE__, __LINE__ );
		}
		val = (char*)id.fw_rev;
		if( val != "" )
		{
			if( val.find( fillMe->mModel.dataValue ) != string::npos )
			{
				val = val.substr( 0,  val.find( fillMe->mModel.dataValue ) );
			}
			fillMe->mFirmwareVersion.setValue( val, 80, __FILE__, __LINE__);
		}
		val = (char*)id.serial_no;
		if( string( val ) != "" )
		{
			fillMe->mSerialNumber.setValue( val, 80, __FILE__, __LINE__ );
		}
	}

	unsigned int SysFSTreeCollector::parsePciVPDBuffer( Component* fillMe,
						    const char *buf, int size )
	{
		char key[ 3 ] = { '\0' };
		/* Each VPD field will be at most 255 bytes long */
		char val[ 256 ];
		const char *end;
		unsigned char length;
		uint16_t len;
		string field;

		/*
		 * The format of the VPD Data is a series of sections, with
		 * each section containing keyword followed by length as shown
		 * below:
		 *
		 *   _ section start here
		 *  |
		 *  v
		 *  ----------------------------------------------------------
		 * | Section ID(1) | len(2) | data(len) | section1 tag(1) |   |
		 *  ----------------------------------------------------------
		 * | len (1) | key(2) | record_len(1) | data(record_len) |    |
		 *  ----------------------------------------------------------
		 * | key(2) | ....... | section2 tag(1) | len(1) | data(len)| |
		 *  ----------------------------------------------------------
		 * |.....| sectionN tag (1) | len(1) | data(len) | end tag(1) |
		 *  ----------------------------------------------------------
		 */

		if (*buf != 0x82)
			return 0;

		end = buf + size;

		/* Increment buffer to point to offset 1 to read Product
		 * Name length
		 */
		buf++;
		if (buf >= end)
			return 0;

		/* Data length is 2 bytes (byte 1 LSB, byte 2 MSB) */
		len = *buf | (buf[1] << 8);

		/* Increment buffer to point to read Product Name */
		buf += 2;
		if (buf >= end)
			return 0;

		/* Increment buffer to point to VPD R Tag */
		buf += len;
		if (buf >= end)
			return 0;

		/* Increment buffer to point to VPD R Tag data length */
		buf++;
		if (buf >= end)
			return 0;

		/* Increment buffer to point to first VPD keyword */
		/* Increment by 2 because data length is of size 2 bytes */
		buf += 2;
		if (buf >= end)
			return 0;

		while( buf < end && *buf != 0x78 && *buf != 0x79 )
		{
			memset( key, '\0', 3 );
			memset( val, '\0', 256 );

			if( buf + 3 >= end )
			{
				goto ERROR;
			}

			key[ 0 ] = buf[ 0 ];
			key[ 1 ] = buf[ 1 ];
			length = buf[ 2 ];
			buf += 3;

			if( buf + length >= end )
			{
				goto ERROR;
			}

			memcpy( val, buf, length );
			buf += length;

			field = sanitizeVPDField(val, length);
			setVPDField( fillMe, key, field, __FILE__, __LINE__ );
		}

		return size;
ERROR:
		Logger logger;
		logger.log( "Attempting to parse corrupt VPD buffer.", LOG_NOTICE );
		return 0;
	}

	void SysFSTreeCollector::fillSpyreVpd(Component* fillMe)
	{
		string path;
		Logger l;
		char device_id[16] = {0};
		int container_fd = -1, group_fd = -1, device_fd = -1;
		int iommu_type = -1;
		char path_buf[256] = {0}, group_path[256] = {0};
		char* group_name = nullptr;
		ssize_t len = 0;
		uint64_t eeprom_data = 0;
		struct vfio_region_info reg = {0};
		void* bar0_mem = nullptr;
		unsigned char* bar0_ptr = nullptr;
		ostringstream ss;
		uint8_t eeprom_res_manu = 0;
		uint8_t boot_version = 0;
		string boot_version_str;
		string partNumber;

		/* Read device ID */
		path = fillMe->getID() + "/device";
		ifstream device_stream(path.c_str());
		if (!device_stream)
			return;

		device_stream.getline(device_id, sizeof(device_id));
		device_stream.close();

		/* Check if the device is Spyre (device ID 0x06a7 or 0x06a8) */
		if (strcmp(device_id, "0x06a7") != 0 && strcmp(device_id, "0x06a8") != 0)
			return;

		l.log("Confirmed Spyre device " + fillMe->getID() + " with ID: " + string(device_id), LOG_INFO);

		/* Open VFIO container */
		container_fd = open("/dev/vfio/vfio", O_RDWR);
		if (container_fd < 0) {
			l.log("Failed to open VFIO container /dev/vfio/vfio for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_ERR);
			return;
		}
		l.log("Successfully opened VFIO container for " + fillMe->getID(), LOG_DEBUG);

		/* Check for IOMMU Support */
		if (ioctl(container_fd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) == 1) {
			iommu_type = VFIO_TYPE1_IOMMU;
			l.log("Using VFIO_TYPE1_IOMMU for " + fillMe->getID(), LOG_DEBUG);
		} else if (ioctl(container_fd, VFIO_CHECK_EXTENSION, VFIO_SPAPR_TCE_IOMMU) == 1) {
			iommu_type = VFIO_SPAPR_TCE_IOMMU;
			l.log("Using VFIO_SPAPR_TCE_IOMMU for " + fillMe->getID(), LOG_DEBUG);
		}
		else {
			l.log("No supported IOMMU type found for " + fillMe->getID(), LOG_ERR);
			close(container_fd);
			return;
		}

		/* Get IOMMU group */
		snprintf(path_buf, sizeof(path_buf), "%s/iommu_group", fillMe->getID().c_str());
		len = readlink(path_buf, group_path, sizeof(group_path) - 1);
		if (len < 0) {
			l.log("Failed to read IOMMU group symlink " + string(path_buf) + " for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_ERR);
			close(container_fd);
			return;
		}

		group_path[len] = '\0';
		group_name = strrchr(group_path, '/');
		if (!group_name) {
			l.log("Failed to extract group name from path: " + string(group_path) + " for " + fillMe->getID(), LOG_ERR);
			close(container_fd);
			return;
		}
		group_name++;

		l.log("Found IOMMU group: " + string(group_name) + " for " + fillMe->getID(), LOG_DEBUG);

		/* Open the VFIO Group */
		snprintf(path_buf, sizeof(path_buf), "/dev/vfio/%s", group_name);
		group_fd = open(path_buf, O_RDWR);

		g_deviceAccessible[fillMe->getID()] = true;
		if (group_fd < 0) {

			Logger l;
			l.log("Failed to open VFIO group " + string(path_buf) + " for " + fillMe->getID(), LOG_ERR);
			g_deviceAccessible[fillMe->getID()] = false;

			if (spyreDb != nullptr) {
				l.log("Attempting to use cached data from spyreDb for " + fillMe->getID(), LOG_INFO);

				Component* spyreComp = spyreDb->fetch(fillMe->getID());
				if (spyreComp != nullptr) {
					l.log("Found cached component data for " + fillMe->getID(), LOG_DEBUG);

					if (!spyreComp->mManufacturer.dataValue.empty()) {
						fillMe->mManufacturer.setValue(spyreComp->mManufacturer.getValue(), 80, __FILE__, __LINE__);
					}
					if (!spyreComp->mModel.dataValue.empty()) {
						fillMe->mModel.setValue(spyreComp->mModel.getValue(), 80, __FILE__, __LINE__);
					}
					if (!spyreComp->mEngChangeLevel.dataValue.empty()) {
						fillMe->mEngChangeLevel.setValue(spyreComp->mEngChangeLevel.getValue(), 80, __FILE__, __LINE__);
					}
					if (!spyreComp->mSerialNumber.dataValue.empty()) {
						fillMe->mSerialNumber.setValue(spyreComp->mSerialNumber.getValue(), 80, __FILE__, __LINE__);
					}
					if (!spyreComp->mPartNumber.dataValue.empty()) {
						fillMe->mPartNumber.setValue(spyreComp->mPartNumber.getValue(), 80, __FILE__, __LINE__);
					}
					if (!spyreComp->mPhysicalLocation.dataValue.empty()) {
						fillMe->mPhysicalLocation.setValue(spyreComp->mPhysicalLocation.getValue(), 80, __FILE__, __LINE__);
					}
					if (!spyreComp->mDescription.dataValue.empty()) {
						fillMe->mDescription.setValue(spyreComp->mDescription.getValue(), 80, __FILE__, __LINE__);
					}
					if (!spyreComp->mCDField.dataValue.empty()) {
						fillMe->mCDField.setValue(spyreComp->mCDField.getValue(), 80, __FILE__, __LINE__);
					}
					if (!spyreComp->mFirmwareLevel.dataValue.empty()) {
						fillMe->mFirmwareLevel.setValue(spyreComp->mFirmwareLevel.getValue(), 80, __FILE__, __LINE__);
					}

				delete spyreComp;
				l.log("Successfully populated component data from cache for " + fillMe->getID(), LOG_INFO);
				} else {
					l.log("No cached component data found in spyreDb for " + fillMe->getID(), LOG_WARNING);
				}
			}

			close(group_fd);
			close(container_fd);
			return;
		}

		/* Check group status */
		struct vfio_group_status group_status = {.argsz = sizeof(group_status)};
		if (ioctl(group_fd, VFIO_GROUP_GET_STATUS, &group_status) < 0 ||
				!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
			l.log("Failed to get VFIO group status for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_ERR);
			close(group_fd);
			close(container_fd);
			return;
		}

		/* Add group to container */
		if (ioctl(group_fd, VFIO_GROUP_SET_CONTAINER, &container_fd) < 0) {
			l.log("Failed to add VFIO group to container for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_ERR);
			close(group_fd);
			close(container_fd);
			return;
		}
		l.log("Successfully added VFIO group to container for " + fillMe->getID(), LOG_DEBUG);

		/* Set IOMMU type */
		if (iommu_type != -1 && ioctl(container_fd, VFIO_SET_IOMMU, iommu_type) < 0) {
			l.log("Failed to set IOMMU type " + to_string(iommu_type) + " for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_ERR);
			close(group_fd);
			close(container_fd);
			return;
		}

		l.log("Successfully set IOMMU type for " + fillMe->getID(), LOG_DEBUG);

		device_fd = ioctl(group_fd, VFIO_GROUP_GET_DEVICE_FD,
				fillMe->getID().substr(fillMe->getID().rfind("/") + 1).c_str());

		if (device_fd < 0) {
			l.log("Failed to get device FD for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_ERR);
			close(group_fd);
			close(container_fd);
			return;
		}

		l.log("Successfully obtained device FD for " + fillMe->getID(), LOG_DEBUG);

		/* Configure BAR0 region info */
		reg.argsz = sizeof(reg);
		reg.index = VFIO_PCI_BAR0_REGION_INDEX;

		/* Get BAR0 Info */
		if (ioctl(device_fd, VFIO_DEVICE_GET_REGION_INFO, &reg) < 0) {
			l.log("Failed to get BAR0 region info for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_ERR);
			close(device_fd);
			close(group_fd);
			close(container_fd);
			return;
		}

		/* Map BAR0 */
		bar0_mem = mmap(NULL, reg.size, PROT_READ | PROT_WRITE, MAP_SHARED, device_fd, reg.offset);
		if (bar0_mem == MAP_FAILED) {
			l.log("Failed to map BAR0 memory for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_ERR);
			close(device_fd);
			close(group_fd);
			close(container_fd);
			return;
		}

		l.log("Successfully mapped BAR0 memory for " + fillMe->getID(), LOG_DEBUG);

		/* Read data from mapped memory */
		bar0_ptr = (unsigned char*)bar0_mem;

		*((uint64_t*)(bar0_ptr + MNIMI_INST)) = 0xa101f801200b1467;
		usleep(50000);

		eeprom_data = *((uint64_t*)(bar0_ptr + MNIMI_DATA));

		/* Set Manufacturer based on EEPROM data */
		eeprom_res_manu = eeprom_data & 0xFF;
		if (eeprom_res_manu == 0xff || eeprom_res_manu == 0x0) {
			fillMe->mManufacturer.setValue("Samsung", 80, __FILE__, __LINE__);
		} else if (eeprom_res_manu == 0x1) {
			fillMe->mManufacturer.setValue("Micron", 80, __FILE__, __LINE__);
		}

		/* Set Firmware Level */
		boot_version = (eeprom_data >> 56) & 0xFF;
		fillMe->mFirmwareLevel.setValue(to_string(boot_version), 80, __FILE__, __LINE__);

		string eeprom11s_sn = read11S(bar0_ptr);
		if (eeprom11s_sn.length() >= 7) {
			partNumber = eeprom11s_sn.substr(0, 7);
			fillMe->mPartNumber.setValue(partNumber, 100, __FILE__, __LINE__);
		}

		fillMe->mEngChangeLevel.setValue(eeprom11s_sn.substr(10, 1), 100, __FILE__, __LINE__);
		fillMe->mSerialNumber.setValue(eeprom11s_sn.substr(7), 100, __FILE__, __LINE__);

                /* Clean up memory mapping */
                if (munmap(bar0_mem, reg.size) < 0) {
			l.log("Warning: Failed to unmap BAR0 memory for " + fillMe->getID() +
					", errno: " + to_string(errno) + " (" + strerror(errno) + ")", LOG_WARNING);
		} else {
			l.log("Successfully unmapped BAR0 memory for " + fillMe->getID(), LOG_DEBUG);
		}

		/* Clean up file descriptors */
		close(device_fd);
		close(group_fd);
		close(container_fd);

		l.log("Successfully completed fillSpyreVpd for " + fillMe->getID(), LOG_INFO);
	}

	string SysFSTreeCollector::read11S(unsigned char* bar0_ptr)
	{
		string result;
		char buffer[8];

		*((uint64_t*)(bar0_ptr + MNIMI_INST)) = 0xa101e001200b1467;
		usleep(50000);
		uint64_t eeprom11s_0 = *((uint64_t*)(bar0_ptr + MNIMI_DATA));

		*((uint64_t*)(bar0_ptr + MNIMI_INST)) = 0xa101e801200b1467;
		usleep(50000);
		uint64_t eeprom11s_1 = *((uint64_t*)(bar0_ptr + MNIMI_DATA));

		*((uint64_t*)(bar0_ptr + MNIMI_INST)) = 0xa101f001200b1467;
		usleep(50000);
		uint64_t eeprom11s_2 = *((uint64_t*)(bar0_ptr + MNIMI_DATA)) & 0xFFFFFF0000000000;

		uint64_t values[3] = {eeprom11s_0, eeprom11s_1, eeprom11s_2};

		for (int val_idx = 0; val_idx < 3; val_idx++) {
			for (int byte_idx = 0; byte_idx < 8; byte_idx++) {
				buffer[byte_idx] = (values[val_idx] >> (56 - byte_idx * 8)) & 0xFF;
			}
			result.append(buffer, 8);
		}

		result.erase(remove(result.begin(), result.end(), '\0'), result.end());
		return result;
	}

	void SysFSTreeCollector::fillPciNvmeVpd( Component* fillMe )
	{
		int device_fd;
		struct stat myDir;
		string path;
		path = fillMe->sysFsNode.getValue() + "/nvme";
		if (stat(path.c_str(), &myDir) < 0)
			return;
		device_fd = device_open(fillMe);
		if (device_fd < 0)
			return;
		collectNvmeVpd(fillMe, device_fd);
		close(device_fd);
		return;
	}

	/* Parse VPD file */
	void SysFSTreeCollector::fillPciDevVpd( Component* fillMe )
	{
		int size;
		string path, vpdDataStr;

		path = fillMe->sysFsNode.getValue() + "/vpd";
		if (HelperFunctions::file_exists(path) != true)
			return;

		vpdDataStr = getBinaryData(path);
		if ((size = vpdDataStr.length()) == 0)
			return;

		//Use data() and not c_str() as the binary data might
		//contain '\0' characters.
		parsePciVPDBuffer( fillMe, vpdDataStr.data(), size );
	}

	void SysFSTreeCollector::fillPCIDev( Component* fillMe,
					     const string& sysDir )
	{
		string val;
		int manID = UNKNOWN_ID, devID = UNKNOWN_ID;
		int subMan = UNKNOWN_ID, subID = UNKNOWN_ID;
		setKernelName( fillMe, sysDir );

		// Get the ID numbers from sysfs.
		val = getAttrValue( sysDir, "vendor" );
		if( val != "" )
		{
			manID = strtol( val.c_str( ), NULL, 16 );
		}

		val = getAttrValue( sysDir, "device" );
		if( val != "" )
		{
			devID = strtol( val.c_str( ), NULL, 16 );
		}

		val = getAttrValue( sysDir, "subsystem_vendor" );
		if( val != "" )
		{
			subMan = strtol( val.c_str( ), NULL, 16 );
		}

		val = getAttrValue( sysDir, "subsystem_device" );
		if( val != "" )
		{
			subID = strtol( val.c_str( ), NULL, 16 );
		}

		ostringstream os;

		if( mPciTable != NULL )
		{
			// Fill Manufacturer Name
			if( subMan == UNKNOWN_ID ||
			    (mPciTable->getName( subMan ) == "Unknown") )
			{
				if( manID != UNKNOWN_ID )
				{
					val = mPciTable->getName( manID );
					if( val != "" )
						fillMe->mManufacturer.setValue( val,
							50, __FILE__, __LINE__ );

				}
			}
			else
			{
				val = mPciTable->getName( subMan );
				if( val != "" )
					fillMe->mManufacturer.setValue( val, 50,
							__FILE__, __LINE__ );

			}

			// Fill Device Model
			if( subID == UNKNOWN_ID || mPciTable->getName( manID, devID,
								       subID ) == "Unknown" )
			{
				if( manID != UNKNOWN_ID )
				{
					val = mPciTable->getName( manID, devID );
					if( val != "" )
						fillMe->mModel.setValue( val, 80,
							 __FILE__, __LINE__ );
				}
			}
			else
			{
				val = mPciTable->getName( manID, devID, subID );
				if( val != "" )
					fillMe->mModel.setValue( val, 80, __FILE__, __LINE__ );
				val = mPciTable->getName( manID, devID );
				if( val != "" )
					fillMe->mDescription.setValue( val, 80, __FILE__, __LINE__ );
			}

			os << "(" << hex << setw(4) << setfill('0') << manID << ","
				<< hex << setw(4) << setfill('0') << devID << "), ("
				<< hex << setw(4) << setfill('0') << subMan << ","
				<< hex << setw(4) << setfill('0') << subID << ")";

			if( os.str( ) != "(ffff,ffff), (ffff,ffff)" )
				fillMe->mCDField.setValue( os.str( ), 100, __FILE__, __LINE__ );
		}

		fillPCIDS( fillMe );

		/* Fill PCI device VPD info */
		fillPciDevVpd(fillMe);

		/* Fill NVME device VPD info using f1h log page */
		fillPciNvmeVpd(fillMe);

		/* Fill Spyre information */
		fillSpyreVpd(fillMe);

		// Read the pci config file for Device Specific (YC)
		os.str( "" );
		os << fillMe->sysFsNode.dataValue << "/config";
		int fd, size;
		char data = 0;
		fd = open( os.str( ).c_str( ), O_RDONLY );
		if( fd < 0 )
		{
			return;
		}

		if( pread( fd, &data, 1, 8 ) < 1 )
		{
			close( fd );
			return;
		}

		close( fd );
		os.str( "" );

		size = (int)data;
		if( size < 0 )
			size *= -1;
		os << dec << (int)size;
		fillMe->addDeviceSpecific( "YC", "Device Specific", os.str( ), 100 );
	}

	void SysFSTreeCollector::fillPCIDS( Component* fillMe )
	{
		string val = fillMe->getDevClass();

		if( val == "net" )
			fillMe->mDescription.setValue( "Ethernet PCI Adapter", 30,
						       __FILE__, __LINE__ );
		else if( val == "graphics" )
			fillMe->mDescription.setValue( "Display Adapter", 30,
						       __FILE__, __LINE__ );
		else if( val == "sound" )
			fillMe->mDescription.setValue( "Multimedia Audio Controller", 30,
						       __FILE__, __LINE__ );
		else if( val == "serial" )
			fillMe->mDescription.setValue( "Serial Adapter", 30,
						       __FILE__, __LINE__ );
		else if( val == "scsi_device" )
			fillMe->mDescription.setValue( "SCSI Device", 30,
						       __FILE__, __LINE__ );
		else if( val == "scsi_disk" )
			fillMe->mDescription.setValue( "SCSI Disk", 30,
						       __FILE__, __LINE__ );
		else if( val == "scsi_generic" )
			fillMe->mDescription.setValue( "Generic SCSI Device", 30,
						       __FILE__, __LINE__ );
		else if( val == "scsi_host" )
			fillMe->mDescription.setValue( "SCSI Adapter", 30,
						       __FILE__, __LINE__ );
		else if( val == "bluetooth" )
			fillMe->mDescription.setValue( "Bluetooth Device", 30,
						       __FILE__, __LINE__ );
		else if( val == "pcmcia_socket" )
			fillMe->mDescription.setValue( "PCMCIA Adapter", 30,
						       __FILE__, __LINE__ );
		else if( val == "usb" )
			fillMe->mDescription.setValue( "Generic USB Device", 30,
						       __FILE__, __LINE__ );
		else if( val == "usb_device" )
			fillMe->mDescription.setValue( "USB Device", 30,
						       __FILE__, __LINE__ );
		else if( val == "usb_host" )
			fillMe->mDescription.setValue( "USB Host Controller", 30,
						       __FILE__, __LINE__ );
		else if( val == "block" )
			fillMe->mDescription.setValue( "Generic Block Device", 10,
						       __FILE__, __LINE__ );
		else if( val == "tty" )
			fillMe->mDescription.setValue( "Serial Device", 20,
						       __FILE__, __LINE__ );
		else if( val == "nvme" )
			fillMe->mDescription.setValue( "NVMe Device", 20,
						       __FILE__, __LINE__ );
	}

	void SysFSTreeCollector::fillUSBDev( Component* fillMe,
					     const string& sysDir )
	{
		string val;
		int manID = UNKNOWN_ID, devID = UNKNOWN_ID;

		setKernelName( fillMe, sysDir );

		// Get the ID numbers from sysfs.
		val = getAttrValue( sysDir, "idVendor" );
		if( val != "" )
		{
			manID = strtol( val.c_str( ), NULL, 16 );
		}

		val = getAttrValue( sysDir, "idProduct" );
		if( val != "" )
		{
			devID = strtol( val.c_str( ), NULL, 16 );
		}

		if( mUsbTable != NULL )
		{
			// Fill Manufacturer Name
			if(  manID != UNKNOWN_ID )
			{
				fillMe->mManufacturer.setValue( mUsbTable->getName( manID ),
								50, __FILE__, __LINE__ );
			}

			// Fill Device Description
			if( devID != UNKNOWN_ID )
			{
				fillMe->mDescription.setValue( "USB Device", 50,
							       __FILE__, __LINE__ );

				fillMe->mModel.setValue( mUsbTable->getName( manID, devID ),
							 50, __FILE__, __LINE__ );
			}
		}

		if( manID != UNKNOWN_ID || devID != UNKNOWN_ID )
		{
			ostringstream os;
			os << hex << setw( 4 ) << setfill( '0' ) << manID;
			os << hex << setw( 4 ) << setfill( '0' ) << devID;
			fillMe->mCDField.setValue( os.str( ), 90, __FILE__, __LINE__ );
		}
	}

#ifndef SIOCETHTOOL
#define SIOCETHTOOL     0x8946
#endif

	/**
	 * Fills NVMe device info
	 */
	void SysFSTreeCollector::fillNvmeClass( Component* fillMe )
	{
		FSWalk fsw = FSWalk();
		vector<string> listing;
		string dev_syspath;
		string dev_childname;
		size_t start;
		int beg, end;
		string str;
		string newDevDir;
		bool dev_found = false;
		int device_fd;

		dev_syspath = fillMe->sysFsNode.getValue();
		fsw.fs_getDirContents(dev_syspath, 'd', listing);
		if (listing.size() <= 0) {
			Logger().log("fillNvmeClass: NVMe dev not found.",
				     LOG_WARNING);
			return;
		}

		while (listing.size() > 0) {
			dev_childname = listing.back();
			listing.pop_back();
			start = dev_childname.find("nvme");
			if (start != string::npos) {
				dev_found = true;
				break;
			}
		}

		if (dev_found != true) {
			Logger().log("fillNvmeClass: NVMe dev matching failed.",
				     LOG_WARNING);
			return;
		}

		/**
		 * Get major/minor number
		 */
		newDevDir = dev_syspath + "/" + dev_childname;
		str = getAttrValue( newDevDir, "dev" );
		if (str.empty())
			return;

		beg = end = 0;
		while (end < (int) str.length() && str[end] != ':')
			end++;

		fillMe->devMajor = atoi(str.substr(beg, end).c_str());

		beg = end + 1;
		end = beg;
		while (end < (int) str.length() && str[end] != ':') {
			end++;
		}

		fillMe->devMinor = atoi(str.substr(beg, end).c_str());
		fillMe->devAccessMode = S_IFBLK;

		device_fd = device_open(fillMe);
		if (device_fd < 0)
			return;

		collectVpd(fillMe, device_fd, false);
		close(device_fd);
		return;
	}

	void SysFSTreeCollector::fillNetClass( Component* fillMe,
					       const string& classDir )
	{
		string val = getAttrValue( classDir, "address" );
		val = val.substr( 0, 17 );
		int pos;
		while( ( pos = val.find( ':' ) ) != (int) string::npos )
		{
			val.erase( pos, 1 );
		}

		fillMe->mNetAddr.setValue( val, 90, __FILE__, __LINE__ );

		struct ethtool_drvinfo info;
		struct ifreq ifr;
		int fd;

		fd = socket( AF_INET, SOCK_DGRAM, 0 );
		if( fd < 0 )
		{
			Logger logger;
			logger.log( "SysFsTreeCollector.fillNetClass: socket call failed.",
				    LOG_WARNING );
			return;
		}

		/* Walk thru AIX names and look for correct one to query */
		vector<DataItem*>::const_iterator i, end;
		i = fillMe->getAIXNames().begin();
		end = fillMe->getAIXNames().end();
		bool done = false;

		while (i != end && !done) {
			memset( &ifr, 0, sizeof( ifr ) );
			memset( &info, 0, sizeof( ethtool_drvinfo ) );
			info.cmd = ETHTOOL_GDRVINFO;
			strncpy( ifr.ifr_name, (*i)->getValue().c_str(), IFNAMSIZ - 1 );
			ifr.ifr_name[IFNAMSIZ - 1] = '\0';
			ifr.ifr_data = (caddr_t)&info;

			if( ioctl( fd, SIOCETHTOOL, &ifr ) == -1 )
				i++;
			else {
				done = true;
			}
		}

		if (i == end && !done) {
			close( fd );
			Logger logger;
			logger.log( "SysFsTreeCollector.fillNetClass: ioctl call failed.",
				    LOG_WARNING );
			return;
		}

		close( fd );

		val = info.fw_version;
		if( val != "" && val != "N/A" )
			fillMe->mFirmwareVersion.setValue( val, 80, __FILE__, __LINE__ );
	}

	void SysFSTreeCollector::fillSystem( System* sys )
	{
		struct utsname info;

		sys->mDescription.setValue( string( "System VPD" ), 100,
					    __FILE__, __LINE__ );
		sys->mRecordType.setValue( string( "VSYS" ), 100,
					   __FILE__, __LINE__ );

		if( uname( &info ) != 0 )
		{
			string message =
				"SysFSTreeCollector.fillSystem: Call to uname failed.";
			Logger logger;
			logger.log( message, LOG_WARNING );
		}
		else
		{
			ostringstream os;
			os << info.sysname << " " << info.release;
			sys->mOS.setValue( os.str( ), 100, __FILE__, __LINE__ );
			sys->mNodeName.setValue( string( info.nodename ), 50
						 , __FILE__, __LINE__ );
			sys->mArch.dataValue = info.machine;
		}
	}

	void SysFSTreeCollector::fillFirmware( Component* fillMe )
	{
		string result = "";
		string path = fillMe->getID();
		const string firmwareAttributes[] = {"fw_version", "firmware_rev"};
		const size_t firmwareAttributesSize = sizeof(firmwareAttributes) / sizeof(firmwareAttributes[0]);
		string classNode = fillMe->getClassNode();
		if (classNode.length() > 0) {
			fillMe->mFirmwareVersion.setValue( getAttrValue( classNode,
					"fw_version" ), 30, __FILE__, __LINE__ );

			fillMe->mFirmwareLevel.setValue( getAttrValue( classNode,
					"fwrev" ), 30, __FILE__, __LINE__ );

			fillMe->mFirmwareVersion.setValue( getAttrValue( classNode,
					"firmware_rev" ), 30, __FILE__, __LINE__ );
		}

		if (fillMe->mFirmwareLevel.dataValue.empty()) {
			result = searchFile(path, "fwrev");
			if (!result.empty()) {
				fillMe->mFirmwareLevel.setValue( getAttrValue( result,
							"fwrev" ), 30, __FILE__, __LINE__ );
			}
		}

		for (size_t i = 0; i < firmwareAttributesSize && fillMe->mFirmwareVersion.dataValue.empty(); i++) {
			result = searchFile(path, firmwareAttributes[i]);
			if (!result.empty()) {
				fillMe->mFirmwareVersion.setValue( getAttrValue( result,
							firmwareAttributes[i]), 30, __FILE__, __LINE__ );
				break;
			}
		}
	}

	string SysFSTreeCollector::resolveClassPath( const string& path )
	{
		string devPath = "/sys" + path;
		string device = devPath.substr( devPath.rfind( '/' ) + 1 );
		devPath += "/bus";
		char rel[ 1024 ] = { 0 };
		if( readlink( devPath.c_str( ), rel, 1023 ) < 0 )
		{
			return "";
		}

		ostringstream resPath;
		resPath << "/sys/bus/";
		string bus = rel;

		resPath << bus.substr( bus.rfind( '/' ) + 1 ) << "/devices/" << device;

		return resPath.str( );
	}
}
