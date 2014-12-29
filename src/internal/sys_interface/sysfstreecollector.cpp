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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <libgen.h>		// for basename()
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

extern int errno;

using namespace std;

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
				fillMe->addDeviceSpecific("XH", "SCSI Host", tmp.substr(0, loc), 60);

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
/*		coutd << "USB: Cur Dev Node = " <<  fillMe->sysFsNode.getValue() << endl; */
	}

	/**
	 * @brief: Collects vpd for devices specified by bus.  This is necessary
	 * 	since the bus of a device can determine the methods used to
	 * 	collect vpd.
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
				fillMe->sysFsLinkTarget.getValue() , "vendor" ), 50, __FILE__, __LINE__);

			fillMe->mDescription.setValue( getAttrValue(
				fillMe->sysFsLinkTarget.getValue() , "device" ), 50, __FILE__, __LINE__);

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
		Logger l;
		string msg;

		/* Here we need to do any of the class specific filling necessary.
		 * the device class was populated above when the Kernel name was set.
		 */

		if( devClass == "net" )
		{
			classLink = fillMe->getClassNode();
			if (classLink.length() > 0)
				fillNetClass( fillMe,  classLink);
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
			fillMe->mDescription.setValue( fillMe->mModel.dataValue, 1 , __FILE__, __LINE__);

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
	 * 	data collection.
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
		string devNode, devName;

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
	 *		collector is aware of (ie - those discovered in the /sys tree),
	 *		and back-walks tree to determine inheritance
	 * @arg devs: An empty vector, which will be filled with one Component
	 * 	for each device on the system.
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
				char target[512];
				char *buf;
				string targetDevPath;
				Component *targetDev;
				size_t n;

				buf = strdup(link.c_str());
				n = readlink(buf, target, 512);
				if (n > 0 && n < 512) {
					target[n] = 0;
					targetDevPath = HelperFunctions::getAbsolutePath(target, buf);
					targetDev = findComponent(devs, targetDevPath);
					if (targetDev != NULL) {
						/* get the device name */
						char *tmp = strdup(devNode.c_str());
						char *name = basename(tmp);

						/* Add 'name' to the AIXName list for targetDev */
						targetDev->addAIXName(name, 90);
						free(tmp);
					}
				}
				free(buf);
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
	 * 	details can be immediately collected.  All more detailed,
	 * 	time consuming (> O(1)), and/or device intra-dependant data should be
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
				char *buf, linkTarget[512];

				buf = strdup(link.c_str());
				int len = readlink(buf, linkTarget, sizeof(
					linkTarget));
				linkTarget[len] = '\0';
				absTargetPath = HelperFunctions::getAbsolutePath(linkTarget,
					buf);
				free(buf);

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
				else if (tmp == "cpu") {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								+ " - Time Device",
								2, __FILE__, __LINE__);
				}
				*/
				else if (tmp == "mem") {
					fillMe->mDescription.setValue(fillMe->mDescription.getValue()
								+ " Memory Device",
								2, __FILE__, __LINE__);
				}
			}
		}

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
				char *buf, linkTarget[512];
				string driver;

				buf = strdup(link.c_str());
				int len = readlink(buf, linkTarget, sizeof(
					linkTarget));
				linkTarget[len] = '\0';
				absTargetPath = HelperFunctions::getAbsolutePath(linkTarget,
					buf);
				free(buf);
				/* Now grab last part of link */
				lastSlash = absTargetPath.rfind("/", absTargetPath.length()) + 1;
				driver = absTargetPath.substr(lastSlash,
							absTargetPath.length() - lastSlash);
				fillMe->devDriver.setValue(driver, INIT_PREF_LEVEL,
								 __FILE__, __LINE__);

				if (driver == "hvc_console") {
					fillMe->addAIXName(driver,90);
					fillMe->mDescription.setValue("Hypervisor Virtual Console",
									90, __FILE__, __LINE__);
				}
			}
		}

		/* Look for generic name for the scsi device pointed to by 'generic' link */
		link = string (fillMe->sysFsNode.getValue() + "/generic");
		if (FSWalk::fs_isLink(link)) {
			char linkTarget[512];
			int len, start;
			string name;

			len = readlink(link.c_str(), linkTarget, sizeof(linkTarget));
			linkTarget[len] = '\0';
			
			name = string(linkTarget);
			start = name.rfind("/", name.length()) + 1;
			fillMe->addAIXName(name.substr(start, name.length() - start), 90);
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
				INIT_PREF_LEVEL - 1, __FILE__, __LINE__);
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
					const string& parentDir, const string& devName)
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
	 * 	child devices.
	 * @param devs Partially filled vector - discovery process will add
	 * 	devices found to this vector
	 * @param parent This will be parent device of devices discovered in
	 * 	'parent' directory.  May be NULL
	 */
	void SysFSTreeCollector::findDevices(vector<Component*>& devs,
			const string& parentDir, const string& searchDir)
	{
		FSWalk fsw = FSWalk();
		string newDevDir;
		vector<string> listing;
		Component *tmpDev;
		string tmp, devName, parentDev;
		char *parent;

		parent = strdup(parentDir.c_str());
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
					devs.push_back(tmpDev);
					findDevices(devs, newDevDir, newDevDir);
				} else if(filterDevicePath(devs, parentDir, devName))
					findDevices(devs, parentDir, newDevDir);
			}
		}
	}

	/**
	 * findDevicePaths( vector<Component*>& devs ,
	 * 	Component *parent)
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
	 * 	dir
	 */
	void SysFSTreeCollector::findDevicePaths(vector<Component*>& devs)
	{
		vector<string> fullList;
		string curPath, tmpDirName, linkName, filePath, devPath;
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
		struct stat astats;
		const char *buf;
		char buf2[512];
		FILE *fi;
		int i;

	        HelperFunctions::fs_fixPath(sysPath);
		sysPath += "/devspec";

	        buf = sysPath.c_str();
		if ((lstat(buf, &astats)) != 0) {
			return string("");
		}

		// Read Results
		fi = fopen(buf , "r");
		if (fi != NULL) {
			if (fgets(buf2, 512, fi) != NULL) {
				i = 0;
				while (buf2[i] != '\0') {
					if (buf2[i] == '\n')
						buf2[i] = '\0';
					i++;
				}
			}
			fclose(fi);
		}

		// cleanup
		return string(buf2);
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
	 * 	with a ':' in the name.  This one has always been the back
	 * 	link to the /sys/class dir, where more info is gathered.
	 * Note: This may break on occassion, so a better method should be
	 * 	investigated.
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
				char* f, *p;
				f = strdup( fname.c_str( ) );
				p = strdup( sysDir.c_str( ) );
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
	 * 		link fitting the known class link format (ie ':' present)
	 * @return bool: Whether the AX name was successfully filled
	 */
	bool SysFSTreeCollector::setKernelName( Component* fillMe,
		const string& sysDir )
	{
		Logger logger;
		struct dirent* entry;
		DIR* d;
		string link;
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
	 * 	/sys/block to a device.  These are collected as AIX names
	 */
	void SysFSTreeCollector::readClassEntries( vector<Component*>& devs )
	{
		findClassEntries(devs, "/sys/class");
		findClassEntries(devs, "/sys/block");
	}

	/**
	 * findClassEntries
	 * @brief Walks through all of /sys/class and /sys/block, looking
	 * 	for links into known devices.  If found, we grab AIX names here
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
				tmp->mDevClass.setValue(searchDir, INIT_PREF_LEVEL, __FILE__, __LINE__);
				/* store AIX name */
				int lastSlash = searchDir.rfind("/", searchDir.length()) + 1;

				target = searchDir.substr(lastSlash, searchDir.length() - lastSlash);
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

		os.str(HelperFunctions::findAIXFSEntry(fillMe->getAIXNames(), "/dev"));
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
			if( subMan == UNKNOWN_ID )
			{
				if( manID != UNKNOWN_ID )
				{
					val = mPciTable->getName( manID );
					if( val != "" )
						fillMe->mManufacturer.setValue( val, 50, __FILE__, __LINE__ );
					os << hex << setw( 4 ) << setfill( '0' ) << manID;
				}
			}
			else
			{
				val = mPciTable->getName( subMan );
				if( val != "" )
					fillMe->mManufacturer.setValue( val, 50, __FILE__, __LINE__ );
				os << hex << setw( 4 ) << setfill( '0' ) << subMan;
			}

			// Fill Device Model
			if( subID == UNKNOWN_ID || mPciTable->getName( manID, devID,
				subID ) == "Unknown" )
			{
				if( manID != UNKNOWN_ID )
				{
					os << hex << setw( 4 ) << setfill( '0' )<< devID;
					val = mPciTable->getName( manID, devID );
					if( val != "" )
						fillMe->mModel.setValue( val, 80, __FILE__, __LINE__ );
				}
			}
			else
			{
				os << hex << setw( 4 ) << setfill( '0' )<< subID;
				val = mPciTable->getName( manID, devID, subID );
				if( val != "" )
					fillMe->mModel.setValue( val, 80, __FILE__, __LINE__ );
				val = mPciTable->getName( manID, devID );
				if( val != "" )
					fillMe->mDescription.setValue( val, 80, __FILE__, __LINE__ );
			}

			if( os.str( ) != "ffffffff" )
				fillMe->mCDField.setValue( os.str( ), 100, __FILE__, __LINE__ );
		}

		fillPCIDS( fillMe );

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
				fillMe->mDescription.setValue( "USB Device", 50, __FILE__, __LINE__ );
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
			strncpy( ifr.ifr_name, (*i)->getValue().c_str(), IFNAMSIZ );
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
		Logger l;
		string msg;

		string classNode = fillMe->getClassNode();
		if (classNode.length() > 0) {
			fillMe->mFirmwareVersion.setValue( getAttrValue( classNode,
				"fw_version" ), 30, __FILE__, __LINE__ );
			fillMe->mFirmwareLevel.setValue( getAttrValue( classNode,
				"fwrev" ), 30, __FILE__, __LINE__ );
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
