/***************************************************************************
 *   Copyright (C) 2012, IBM                                               *
 *                                                                         *
 *   Maintained by:                                                        *
 *   Aravinda Prasad                                                       *
 *   aravinda@linux.vnet.ibm.com                                           *
 *                                                                         *
 * See the file 'COPYING' for the license of this file.                    *
 ***************************************************************************/

#include <libvpd-2/vpdretriever.hpp>
#include <libvpd-2/component.hpp>
#include <libvpd-2/dataitem.hpp>
#include <libvpd-2/system.hpp>
#include <libvpd-2/vpdexception.hpp>

#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdlib.h>

#include <getopt.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>

/* XXX	This is a temporary fix to help package the invscout command
 * This should basically use the version identifier defined in configure.in.
 * The fix is to define a separate invscout version variable in configure.in
 * which will be used here as well as in lsvpd.spec.in
 */
#define INVSCOUT_VERSION "3.0.0"

using namespace lsvpd;

int resourceFlag = 0;

#define INV_PATH "/var/adm/invscout/"
#define RESOURCE_LIST	1
#define RESOURCE   0

std::string loc(""), hostName(""), partitionID("");
std::string aliase(""), hmc("N"), IP("");
std::string devNm("");

void printUsage( char *argv )
{
	cout << "Usage: " << argv << endl;
	cout << "invscout [options]" << endl;
	cout << "With no option performs microcode survey" << endl;
	cout << "options: " << endl;
	cout << " -h     print this usage message" << endl;
	cout << " -v     Perform VPD survey" << endl;
	cout << " -g     print the version of invscout" << endl;
}

void printVersion( )
{
	cout << INVSCOUT_VERSION << endl;
}

/* Helper Functions */
/*
 * This function will execute the command passed into the cmd parameter and
 * stores the output in "output".
 */
int execCmd( const char *cmd, std::string &output )
{
	char buffer[32];
	FILE* fd = popen( cmd, "r" );

	if( !fd )
		return 1;

	while( !feof( fd ) )
		if( fgets( buffer, 32, fd ) != NULL )
			output += buffer;

	pclose( fd );
	return 0;
}

/*
 * Insert the number of spaces specified in "indent". This is used to indent
 * the paragraphs in the XML VPD survey file
 */
void insertSpaces( std::string &spaces, int indent )
{
	for( int i = 0; i < indent; i++ )
		spaces += "  ";
}

std::string getTime( )
{
	time_t sysTime;
	char buffer[100];

	memset( buffer, 0x0, 100 );

	snprintf( buffer, 100, "%lu", time( &sysTime ) );
	std::string timeStamp( buffer );
	return timeStamp;
}

/*
 * Generate ID to be included in the file name using Machine Model
 * and serial number
 */
std::string getID( System* root )
{
	std::string sysModel, sysSerial, ID;

	sysModel = root->getMachineType( );
	sysSerial = root->getSerial1( );

	/* Remove "IBM," from Machine Type */
	sysModel.erase( 0, 4 );

	/* Skip "IBM,xx" from the SE field */
	sysSerial.erase( 0, 4 + 2 );

	ID = sysModel + '_' + sysSerial;

	return ID;
}

/* Helper functions for generating XML file for VPD survey */

/*
 * Insert a property tag in the XML file with suitable indentation
 *
 * Ex:
 * <Property name="DS" displayName="Description">
 * DS
 * <Value type="string">System VPD</Value>
 * </Property>
 */
void insertPropertyTag( std::ofstream &xmlSurveyFile, int tabs,
		string code, string displayName, string valueName )
{
	std::string property("");
	std::string spaces("");

	insertSpaces( spaces, tabs );

	property += spaces + "<Property name=\"" + code;
	property += "\" displayName=\"" + displayName + "\">\n";
	property += spaces + "  " + code + "\n";
	property += spaces + "  <Value type=\"string\">";
	property += valueName + "</Value>\n";
	property += spaces + "</Property>\n";

	xmlSurveyFile << property.c_str( );
}

/*
 * Insert a resource tag in the XML file with suitable indentation
 *
 * Ex:
 * <Resource displayName="7998.60X.100DE2A"
 * uniqueId="7998.60X.100DE2A">
 * 7998.60X.100DE2A
 */
void insertResourceTag( std::ofstream &xmlSurveyFile, int tabs, int flag,
		string displayName, string uniqueID )
{
	std::string resource("");
	std::string spaces("");

	insertSpaces( spaces, tabs );

	if( flag )
		resource += spaces + "<ResourceList";
	else
		resource += spaces + "<Resource";

	resource += " displayName=\"" +  displayName + "\"";

	if( !flag )
		resource += " uniqueId=\"" + uniqueID + "\"";

	resource += ">\n";

	if( !flag )
		resource += "  " + spaces + uniqueID + "\n";

	xmlSurveyFile << resource.c_str( );
}

void insertCloseTag( std::ofstream &xmlSurveyFile, int tabs, bool flag )
{
	std::string tag("");
	std::string spaces("");

	insertSpaces( spaces, tabs );

	if( flag )
		tag += spaces + "</ResourceList>\n";
	else
		tag += spaces + "</Resource>\n";

	xmlSurveyFile << tag.c_str( );
}
/* End of XML helper functions */

void insertPropertyAll( Component* root, std::ofstream &xmlSurveyFile, int tab )
{
	/* Insert all the properties for the given component */
	insertResourceTag( xmlSurveyFile, tab, RESOURCE, root->getDescription( ),
		root->getPhysicalLocation( ) );
	tab++;

	if( root->getDescription( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getDescriptionAC( ),
			root->getDescriptionHN( ), root->getDescription( ) );

	if( root->getRecordType( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getRecordTypeAC( ),
			root->getRecordTypeHN( ), root->getRecordType( ) );

	/*AX*/
	vector<DataItem*>::const_iterator j, stop;
	for( j = root->getAIXNames( ).begin( ),
		stop = root->getAIXNames( ).end( ); j != stop; ++j )
	{
		insertPropertyTag( xmlSurveyFile, tab,  (*j)->getAC( ),
			(*j)->getHumanName( ), (*j)->getValue( ) );
	}

	/*MF*/
	if( root->getManufacturer( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getManufacturerAC( ),
			root->getManufacturerHN( ), root->getManufacturer( ) );
	/*TM*/
	if( root->getModel( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getModelAC( ),
			root->getModelHN( ), root->getModel( ) );
	/*CD*/
	if( root->getCD( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getCDAC( ),
			root->getCDHN( ), root->getCD( ) );
	/*NA*/
	if( root->getNetAddr( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getNetAddrAC( ),
			root->getNetAddrHN( ), root->getNetAddr( ) );
	/*FN*/
	if( root->getFRU( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getFRUAC( ),
			root->getFRUHN( ), root->getFRU( ) );
	/*RM*/
	if( root->getFirmwareVersion( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getFirmwareVersionAC( ),
			root->getFirmwareVersionHN( ), root->getFirmwareVersion( ) );
	/*MN*/
	if( root->getManufacturerID( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getManufacturerIDAC( ),
			root->getManufacturerIDHN( ), root->getManufacturerID( ) );
	/*RL*/
	if( root->getFirmwareLevel( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getFirmwareLevelAC( ),
			root->getFirmwareLevel( ), root->getFirmwareLevel( ) );
	/*SN*/
	if( root->getSerialNumber( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getSerialNumberAC( ),
			root->getSerialNumberHN( ), root->getSerialNumber( ) );
	/*EC*/
	if( root->getEngChange( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getEngChangeAC( ),
			root->getEngChangeHN( ), root->getEngChange( ) );
	/*PN*/
	if( root->getPartNumber( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getPartNumberAC( ),
			root->getPartNumberHN( ), root->getPartNumber( ) );
	/*N5*/
	if( root->getn5( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getn5AC( ),
			root->getn5HN( ), root->getn5( ) );
	/*N6*/
	if( root->getn6( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getn6AC( ),
			root->getn6HN( ), root->getn6( ) );

	for( j = root->getDeviceSpecific( ).begin( ),
		stop = root->getDeviceSpecific( ).end( );
		j != stop; ++j )
	{
		insertPropertyTag( xmlSurveyFile, tab, (*j)->getAC( ),
			(*j)->getHumanName( ), (*j)->getValue( ) );
	}

	if( root->getMicroCodeImage( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getMicroCodeImageAC( ),
			root->getMicroCodeImageHN( ), root->getMicroCodeImage( ) );

	for( j = root->getUserData( ).begin( ),
		stop = root->getUserData( ).end( );
		j != stop; ++j )
	{
		insertPropertyTag( xmlSurveyFile, tab, (*j)->getAC( ),
			(*j)->getHumanName( ), (*j)->getValue( ) );
	}

	if( root->getPhysicalLocation( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getPhysicalLocationAC( ),
			root->getPhysicalLocationHN( ), root->getPhysicalLocation( ) );

	if( root->getKeywordVersion( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getKeywordVersionAC( ),
			root->getKeywordVersionHN( ), root->getKeywordVersion( ) );

	if( root->getMachineSerial( ) != "" )
		insertPropertyTag( xmlSurveyFile, tab, root->getMachineSerialAC( ),
			root->getMachineSerialHN( ), root->getMachineSerial( ) );

	tab--;
}

/*
 * Insert all the resources under the "enclosure"
 */
void insertResourceAll( std::ofstream &xmlSurveyFile, int tab, Component *cmp,
		Component* enclosure )
{
	std::string enclosurePhyLoc = enclosure->getPhysicalLocation( );
	std::string phyLoc = cmp->getPhysicalLocation( );

	/* A component "cmp" is under the enclosure "enclosure" if the physical
	 * location of the enclosure is a substring of the physical location of the
	 * component
	 */
	if ( cmp != enclosure && phyLoc.find( enclosurePhyLoc.c_str( ) ) != -1 )
	{
		if ( !resourceFlag )
		{
			insertResourceTag( xmlSurveyFile, tab, RESOURCE_LIST, "Global Resources", "" );
			resourceFlag = 1;
		}

		tab++;
		insertPropertyAll( cmp, xmlSurveyFile, tab );
		insertCloseTag( xmlSurveyFile, tab, RESOURCE );
	}

	vector<Component*> children = cmp->getLeaves( );
	vector<Component*>::const_iterator i, end;

	for( i = children.begin( ), end = children.end( ); i !=
		end; ++i )
		insertResourceAll( xmlSurveyFile, tab, *i, enclosure );
}

void insertRecord( std::ofstream &report, const string &AC, const string &val )
{
	report << "<" << AC << ">";

	if ( val != "" )
		report << val;
	else
		report << ".";

	report << "</" << AC << "> ";
}


Component* findComponentByName( Component* root, std::string &name )
{
	std::string desc = root->getDescription( );
	Component *cmp;

	if( !desc.find( name.c_str( ) ) )
		return root;

	vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;

	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		cmp = findComponentByName( *i, name );
		if ( cmp )
			return cmp;
	}
	return NULL;
}

void findEnclosures(System* root, Component* cmp, std::ofstream &xmlSurveyFile,
		int tab)
{
	vector<Component*> children = cmp->getLeaves( );
	vector<Component*>::const_iterator j, end;
	std::string phyLoc = cmp->getPhysicalLocation( );

	/* Physical Location will be always defined for the enclosure.
	 * Also an enclosure will not have "-" in its physical location
	 * and should have machine/cabinet number and model name defined
	 */
	if( phyLoc != "" && phyLoc.find ('-') == -1 && cmp->getModel( ) != ""
		&& cmp->getMachineSerial( ) != "" )
	{
		insertPropertyAll( cmp, xmlSurveyFile, tab );
		resourceFlag = 0;
		tab++;

		const vector<Component*> component = root->getLeaves( );
		vector<Component*>::const_iterator i, cmp_end;
		for( i = component.begin( ), cmp_end = component.end( );
			i != cmp_end; ++i )
		{
			/* Insert all the components under this enclosure */
			insertResourceAll( xmlSurveyFile, tab, (*i), cmp );
		}

		if ( resourceFlag )
		{
			insertCloseTag( xmlSurveyFile, tab, RESOURCE_LIST );
		}

		tab--;
		insertCloseTag( xmlSurveyFile, tab, RESOURCE );
	}

	for( j = children.begin( ), end = children.end( );
		j != end; ++j )
		findEnclosures( root, (*j), xmlSurveyFile, tab );
}

/* Insert Partition specific data in XML file */
void insertPartitionData( System *root, std::ofstream &xmlSurveyFile, int tab )
{
	std::string uniqueID("");

	if( execCmd( "/bin/hostname", hostName ) )
		cout << "Could not execute /bin/hostname\n" << endl;

	if( execCmd( "awk -F '=' '/partition_id/{ print $2}' /proc/ppc64/lparcfg",
				partitionID ) )
		cout << "Could not get partition ID\n" << endl;

	/* Remove the '\n' from the string */
	hostName.erase( hostName.size( ) - 1 );
	partitionID.erase( partitionID.size( ) - 1 );

	/* Unique ID will be locationID-partitionID */
	uniqueID = root->getLocation( ) + "-" + partitionID;

	insertResourceTag( xmlSurveyFile, tab, RESOURCE_LIST, "Partitions", "" );
	tab++;

	insertResourceTag( xmlSurveyFile, tab, RESOURCE, hostName, uniqueID );
	tab++;

	insertPropertyTag( xmlSurveyFile, tab, "HN", "HostName", hostName );
	insertPropertyTag( xmlSurveyFile, tab, root->getOSAC( ), root->getOSHN( ),
		root->getOS( ) );
	insertPropertyTag( xmlSurveyFile, tab, "PT", "Partition ID", partitionID );

	tab--;
	insertCloseTag( xmlSurveyFile, tab, RESOURCE );

	tab--;
	insertCloseTag( xmlSurveyFile, tab, RESOURCE_LIST );
}

void insertGlobalSystemResource( System *root, std::ofstream &xmlSurveyFile,
		int tab )
{
	Component* cmp;
	std::string name( "System Firmware" );

	insertResourceTag( xmlSurveyFile, tab, RESOURCE_LIST, "Global System Resources", "" );
	tab++;

	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
	{
		cmp = findComponentByName( *i, name );
		if ( cmp )
			break;
	}

	if( !cmp )
	{
		cout << "Error: Unable to find System Firmware data in VPD" << endl;
		xmlSurveyFile.close( );
		exit(1);
	}

	insertPropertyAll( cmp, xmlSurveyFile, tab );
	insertCloseTag( xmlSurveyFile, tab, RESOURCE );

	tab--;
	insertCloseTag( xmlSurveyFile, tab, RESOURCE_LIST );
}

void insertEnclosuresElements( System *root, std::ofstream &xmlSurveyFile, int tab )
{
	insertResourceTag( xmlSurveyFile, tab, RESOURCE_LIST, "Enclosures", "" );
	tab++;

	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
		findEnclosures( root, (*i), xmlSurveyFile, tab );

	tab--;
	insertCloseTag( xmlSurveyFile, tab, RESOURCE_LIST );
}

void writeData( System *root, std::ofstream &xmlSurveyFile)
{
	int tab = 2;
	loc = root->getLocation( );

	/*
	 * Remove the first character of location code to be compatible with the
	 * output of AIX invscout command
	 */
	loc.erase( 0, 1 );

	insertResourceTag( xmlSurveyFile, tab, RESOURCE, loc, loc );
	tab++;

	/* Insert "System_VPD" properties */
	insertPropertyTag( xmlSurveyFile, tab, root->getDescriptionAC( ),
		root->getDescriptionHN( ), root->getDescription( ) );

	insertPropertyTag( xmlSurveyFile, tab, root->getRecordTypeAC( ),
		root->getRecordTypeHN( ), root->getRecordType( ) );

	insertPropertyTag( xmlSurveyFile, tab, root->getFlagFieldAC( ),
		root->getFlagFieldHN( ), root->getFlagField( ) );

	insertPropertyTag( xmlSurveyFile, tab, root->getBrandAC( ),
		root->getBrandHN( ), root->getBrand( ) );

	insertPropertyTag( xmlSurveyFile, tab, root->getLocationAC( ),
		root->getLocationHN( ), root->getLocation( ) );

	insertPropertyTag( xmlSurveyFile, tab, root->getSerial2AC( ),
		root->getSerial2HN( ), root->getSerial2( ) );

	insertPropertyTag( xmlSurveyFile, tab, root->getMachineModelAC( ),
		root->getMachineModelHN( ), root->getMachineModel( ) );

	insertPropertyTag( xmlSurveyFile, tab, root->getSUIDAC( ),
		root->getSUIDHN( ), root->getSUID( ) );

	insertPropertyTag( xmlSurveyFile, tab, root->getKeywordVerAC( ),
		root->getKeywordVerHN( ), root->getKeywordVer( ) );

	vector<DataItem*>::const_iterator j, stop;
	for( j = root->getDeviceSpecific( ).begin( ),
		stop = root->getDeviceSpecific( ).end( ); j != stop; ++j )
	{
		insertPropertyTag( xmlSurveyFile, tab,  (*j)->getAC( ),
			(*j)->getHumanName( ), (*j)->getValue( ) );
	}

	/* Insert Partition related information */
	insertPartitionData( root, xmlSurveyFile, tab );

	/* Insert Global System Resource */
	insertGlobalSystemResource( root, xmlSurveyFile, tab );

	/* Insert Enclosures */
	insertEnclosuresElements( root, xmlSurveyFile, tab );

	tab--;
	insertCloseTag( xmlSurveyFile, tab, RESOURCE );
}

int writeHeader( std::ofstream &xmlSurveyFile )
{
	xmlSurveyFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << "\n";
	xmlSurveyFile << "<com.ibm.inventory version=\"1.0\">"        << "\n";
	xmlSurveyFile << "  <ResourceSet";
	xmlSurveyFile << " class=\""      << "com.ibm.inventory.Hardware" << "\"";
	xmlSurveyFile << " type=\""       << "Hardware" << "\"";
	xmlSurveyFile << " version=\""    << "1.0" << "\"";
	xmlSurveyFile << " timestamp=\""  << getTime() << "\"";

	xmlSurveyFile << ">" << std::endl;

	return 0;
}

int writeFooter( std::ofstream &xmlSurveyFile )
{
	xmlSurveyFile << "   </ResourceSet>"     << std::endl;
	xmlSurveyFile << "</com.ibm.inventory>"  << std::endl;
	return 0;
}

int generateXML( System* root )
{
	std::string xmlFile = "";
	int rc;

	cout << "\n******  Invscout Version v" << INVSCOUT_VERSION << endl;
	cout << "\nInitializing ...\n" << endl;

	xmlFile = INV_PATH;
	xmlFile += getID( root );
	xmlFile += "_VPD.xml";

	rc = mkdir( INV_PATH, 0766 );
	if( rc != 0 && errno != EEXIST )
	{
		cout << "Error creating the directory " << INV_PATH  << endl;
		cout << "Error code " << errno << endl;
		return 0;
	}

	std::ofstream xmlSurveyFile( xmlFile.c_str() );

	if( !xmlSurveyFile.is_open( ) )
	{
		cout << "Cannot open survey file " << xmlFile << endl;
		return 0;
	}

	/* Write XML Header */
	writeHeader( xmlSurveyFile );

	/* Perform survey and write survey data to XML file */
	writeData( root, xmlSurveyFile );

	/* Write XML footer */
	writeFooter( xmlSurveyFile );

	xmlSurveyFile.close( );

	cout << "VPD Survey complete\n" << endl;
	cout << "The output files can be found at:" << endl;
	cout << "Upload file: " << xmlFile << "\n\n" << endl;

	cout << "To transfer the invscout 'Upload file' for Vital Product" << endl;
	cout << "Data submission, see your service provider's web page.\n" << endl;

	return 1;
}

void writeMcSurveyHeader( std::ofstream &mcSurveyFile )
{
	std::string model(""), kernel(""), date("");

	if( execCmd( "/bin/hostname", hostName ) )
		cout << "Could not execute /bin/hostname" << endl;

	if(execCmd( "awk -F ':' '/model/{ print $2}' /proc/cpuinfo", model ) )
		cout << "Could not open /proc/cpuinfo" << endl;

	if( execCmd( "/bin/uname -r", kernel ) )
		cout << "Could not execute /bin/uname" << endl;

	if( execCmd( "/bin/date", date ) )
		cout << "Could not execute /bin/date" << endl;

	/* Remove the '\n' from the string */
	hostName.erase( hostName.size( ) - 1 );
	kernel.erase( kernel.size( ) -1 );
	date.erase( date.size( ) -1 );
	model.erase( 0, 1 );

	mcSurveyFile << "Hostname  . . . . . . : " << hostName << endl;
	mcSurveyFile << "Invscout Version  . . : " << INVSCOUT_VERSION << endl;
	mcSurveyFile << "Survey Date and Time  : " << date << endl;
	mcSurveyFile << "Kernel Level  . . . . : " << kernel << endl;
	mcSurveyFile << "Model . . . . . . . . : " << model << endl;

	mcSurveyFile << "\n\n\t------------------" << endl;
	mcSurveyFile << "\tIDENTIFIED DEVICES" << endl;
	mcSurveyFile << "\t------------------" << endl;

	mcSurveyFile << "Device\t\t\tInstalled Microcode" << endl;
	mcSurveyFile << "---------------------------------------------------------" << endl;
}

void writeMcReportHeader( std::ofstream &UploadFile, System* root )
{
	std::string date(""), os(""), version(""), hmc_name("");
	int pos;

	if( execCmd( "/bin/date +%G%m%d%H%M", date ) )
		cout << "Could not execute /bin/date" << endl;

	if( execCmd( "/bin/uname -s", os ) )
		cout << "Could not execute /bin/uname" << endl;

	if( execCmd( "/usr/bin/head -1 /etc/issue", version ) )
		cout << "Could not execute /usr/bin/head" << endl;

	if( execCmd( "serv_config -l | grep HscHostName", hmc_name ) )
		cout << "Cound not execute serv_config" << endl;

	if( execCmd( "/sbin/ifconfig | awk '/Bcast/{ print $2 }' | awk -F ':' \
			'{print $2}'", IP ) )
		cout << "Could not get IP address" << endl;

	if( execCmd( "awk -F '=' '/partition_id/{ print $2}' /proc/ppc64/lparcfg",
			partitionID ) )
		cout << "Could not execute /proc/ppc64/lparcfg\n" << endl;

	/* Remove the '\n' from the string */
	date.erase( date.size( ) -1 );
	os.erase( os.size( ) -1 );
	version.erase( version.size( ) - 1 );
	IP.erase( IP.size( ) -1 );
	partitionID.erase( partitionID.size( ) -1 );

	pos = hostName.find( "." );
	if ( pos != -1 )
		aliase = hostName.substr( 0, pos );
	else
		aliase = hostName;

	if( hmc_name.size( ) )
		hmc = "Y";

	loc = root->getLocation( );

	UploadFile << "FORM=\"INVSCOUT\" ISLVL=\"" << INVSCOUT_VERSION << "\"\n" << endl;

	UploadFile << "REC=\"HDR\" CATDATE=\"\" LICDATE=\"\" ";
	UploadFile << "VPDKW=\"MF TM CD PN FN EC RM LL LI Z2\" SURVDATE=\"";
	UploadFile << date << "\"\n" << endl;

	UploadFile << "REC=\"OS\" OSTYPE=\"" << os << "\" OSLVL=\"" << version;
	UploadFile << "\" MTMS=\"" << loc << "\"";
	UploadFile << "	PARID=\"" << partitionID << "\" SRVPAR=\"Y\"";
	UploadFile << " HOSTNAME=\"" << hostName << "\"";
	UploadFile << " IP=\"" << IP << ",  Aliases:  " << aliase << "\"";
	UploadFile << " HMCCTL=\"" << hmc << "\" ISLVL=\"" << INVSCOUT_VERSION << "\"";
	UploadFile << " CATDATE=\"\" SURVDATE=\"" << date << "\"";
	UploadFile << " STATUS=\"Surveyed\"";
}

void writeMcReportRecord( std::ofstream &report, Component *root )
{
	/* Insert standard record */
	report << "\n\nREC=\"ERR\" MTMS=\"" << loc;
	report << "\" PARID=\"" << partitionID << "\" SRVPAR=\"Y\"" ;
	report << "HOSTNAME=\"" << hostName << "\" IP=\"" << IP;
	report << ", Aliases:\"" << aliase << "\" HMCCTL=\"" << hmc << "\" ISLVL";
	report << "=\"" << INVSCOUT_VERSION << "\" CATDATE=\"\" LDB=\"N\" DEVNM=\"";
	report << devNm;

	vector<DataItem*>::const_iterator j, stop;
	for( j = root->getAIXNames( ).begin( ), stop = root->getAIXNames( ).end( );
		j != stop; ++j )
	{
		report << (*j)->getValue( ) << " ";
	}
	report << "\" ";

	/* Insert all the required tags if present */
	/*MF*/
	insertRecord( report, root->getManufacturerAC( ), root->getManufacturer( ) );

	/*TM*/
	insertRecord( report, root->getModelAC( ), root->getModel( ) );

	/*CD*/
	insertRecord( report, root->getCDAC( ), root->getCD( ) );

	/*PN*/
	insertRecord( report, root->getPartNumberAC( ), root->getPartNumber( ) );

	/*FN*/
	insertRecord( report, root->getFRUAC( ), root->getFRU( ) );

	/*EC*/
	insertRecord( report, root->getEngChangeAC( ), root->getEngChange( ) );

	/*RM*/
	insertRecord( report, root->getFirmwareVersionAC( ),
			root->getFirmwareVersion( ) );

	/*LL*/
	insertRecord( report, "LL", "." );

	/*LI*/
	insertRecord( report, "LI", "." );

	/*Z2*/
	for( j = root->getDeviceSpecific( ).begin( ),
		stop = root->getDeviceSpecific( ).end( ); j != stop; ++j )
	{
		if ( (*j)->getAC( ) == "Z2" )
		{
			insertRecord( report, (*j)->getAC( ), (*j)->getValue( ) );
			return;
		}
	}
	insertRecord( report, "Z2", "." );
}

void writeMcSurveyData( std::ofstream &mcSurveyFile,
		std::ofstream &mcSurveyReportFile, Component *root )
{
	std::string desc = root->getDescription( );

	if( !desc.find( "System Firmware" ) )
	{
		devNm = "sys0";
		mcSurveyFile << devNm << "\t\t\t" << root->getMicroCodeImage( ) << endl;
		writeMcReportRecord(mcSurveyReportFile, root);
		devNm = "";
	}

	string fwVersion = "";
	if( root->getFirmwareVersion( ) != "" )
		fwVersion = root->getFirmwareVersion( );
	else if( root->getFirmwareLevel() != "" )
		fwVersion = root->getFirmwareLevel( );

	if( fwVersion != "" )
	{
		vector<DataItem*>::const_iterator j, stop;
		for( j = root->getAIXNames( ).begin( ),
			stop = root->getAIXNames( ).end( ); j != stop; ++j )
		{
			mcSurveyFile << (*j)->getValue( ) << " ";
		}

		mcSurveyFile << "\t\t" << root->getModel( ) << "." << fwVersion << endl;
		writeMcReportRecord( mcSurveyReportFile, root );
	}

	vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;

	for( i = children.begin( ), end = children.end( ); i != end; ++i )
		writeMcSurveyData( mcSurveyFile, mcSurveyReportFile, (*i) );
}

int microCodeSurvey( System* root )
{
	std::string mcSurvey = "", mcSurveyReport = "";
	int rc;

	cout << "\n******  Invscout Version v" << INVSCOUT_VERSION << endl;
	cout << "\nInitializing ...\n" << endl;

	mcSurvey = INV_PATH;
	mcSurvey += "invs.mrp";

	rc = mkdir( INV_PATH, 0766 );
	if( rc != 0 && errno != EEXIST )
	{
		cout << "Error creating the directory " << INV_PATH  << endl;
		cout << "Error code " << errno << endl;
		return 0;
	}

	std::ofstream mcSurveyFile( mcSurvey.c_str( ) );

	if( !mcSurveyFile.is_open( ) )
	{
		cout << "Cannot open survey file " << mcSurvey << endl;
		return 0;
	}

	int pos;
	std::string hostName;

	if ( execCmd( "/bin/hostname", hostName ) )
	{
		cout << "Could not execute /bin/hostname" << endl;
		/* Report file name if hostname command fails */
		hostName += "host";
	}

	mcSurveyReport = INV_PATH;
	pos = hostName.find(".");
	if (pos != -1)
		mcSurveyReport += hostName.substr (0, pos);
	else
		mcSurveyReport += hostName;

	mcSurveyReport +=".mup";

	std::ofstream mcSurveyReportFile(mcSurveyReport.c_str());

	if( !mcSurveyReportFile.is_open() )
	{
		cout << "Cannot open survey file " << mcSurveyReport << endl;
		return 0;
	}

	/* Write Header */
	writeMcSurveyHeader( mcSurveyFile );
	writeMcReportHeader( mcSurveyReportFile, root );

	cout << "Identifying the system ..." << endl;
	cout << "Getting system microcode level(s) ..." << endl;
	cout << "Scanning for device microcode level(s) ..." << endl;

	cout << "\nWriting Microcode Survey upload file ..." << endl;

	/* Write survey data */
	const vector<Component*> children = root->getLeaves( );
	vector<Component*>::const_iterator i, end;
	for( i = children.begin( ), end = children.end( ); i != end; ++i )
		writeMcSurveyData( mcSurveyFile, mcSurveyReportFile, (*i) );

	cout << "\nMicrocode Survey complete" << endl;

	cout << "\nThe output files can be found at:" << endl;
	cout << "Upload file: " << mcSurveyReport << endl;
	cout << "Report file: " << mcSurvey << endl;

	cout << "\nTo transfer the invscout 'Upload file' for microcode" << endl;
	cout << "comparison, see your service provider's web page.\n" << endl;

	return 1;
}

int main( int argc, char** argv )
{
	System * root = NULL;
	VpdRetriever* vpd = NULL;
	int vflag = 0, cflag = 0, ch;

	if( argc > 2 )
	{
		printUsage( argv[0] );
		return -1;
	}

	if( (ch = getopt( argc, argv, "ghv") ) != -1 )
	{
		switch( ch )
		{
			case 'v':
				vflag = 1;
				break;

			case 'c':
				cflag = 1;
				break;

			case 'g':
				printVersion( );
				return 0;

			case 'h':
			default:
				printUsage( argv[0] );
				return -1;
		}
	}

	try
	{
		vpd = new VpdRetriever( );
	}
	catch( exception& e )
	{
		string prefix( DEST_DIR );
		cout << "Please run " << prefix;
		if( prefix[ prefix.length( ) - 1 ] != '/' )
		{
			cout << "/";
		}
		cout << "sbin/vpdupdate before running invscout." << endl;
		return 1;
	}

	if( vpd != NULL )
	{
		try
		{
			root = vpd->getComponentTree( );
		}
		catch( VpdException& ve )
		{
			cout << "Error reading VPD DB: " << ve.what( ) << endl;
			delete vpd;
			return 1;
		}

		delete vpd;
	}

	if( root != NULL && vflag )
	{
		/* Perform VPD survey and generate XML file */
		if( !generateXML( root ) )
		{
			cout << "Error generating XML file ";
			return 1;
		}

		delete root;
		return 0;
	}

	/* Perform microcode survey if no flags specified */
	if ( !microCodeSurvey( root ) )
	{
		cout << "Error performing microcode survey" << endl;
		return 1;
	}

	return 0;
}

