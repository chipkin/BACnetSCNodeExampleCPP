/*
 * BACnet Server Example C++
 * ----------------------------------------------------------------------------
 * CASBACnetStackExampleDatabase.h
 *
 * The CASBACnetStackExampleDatabase is a data store that contains
 * some example data used in the BACnetStackDLLExample.
 * This data is represented by BACnet objects for this server example.
 *
 * Created by: Alex Fontaine
 */

#ifndef __CASBACnetStackExampleDatabase_h__
#define __CASBACnetStackExampleDatabase_h__

#include <string>
#include <vector>
#include <stdint.h>
#include <string.h>
#include <map>

// Base class for all object types.
class ExampleDatabaseBaseObject
{
public:
	// Const
	static const uint8_t PRIORITY_ARRAY_LENGTH = 16;

	// All objects will have the following properties
	std::string objectName;
	uint32_t instance;
	ExampleDatabaseBaseObject();
};

class ExampleDatabaseAnalogInput : public ExampleDatabaseBaseObject
{
public:
	float presentValue;
	float covIncrement;
	uint32_t reliability;
	uint32_t units;
	ExampleDatabaseAnalogInput();
};

class ExampleDatabaseDevice : public ExampleDatabaseBaseObject
{
public:
	uint32_t systemStatus;
	std::string description;
	std::string applicationSoftwareVersion;
	ExampleDatabaseDevice();
};

class ExampleDatabaseNetworkPort : public ExampleDatabaseBaseObject
{
public:
	bool changesPending;
	uint8_t vmac[6];					 // Virtual MAC address for BACnet SC
	std::string primaryHubUri; // Hub WSS URI, e.g. "wss://192.168.1.100:47808"
	ExampleDatabaseNetworkPort();
};

class ExampleDatabaseFile : public ExampleDatabaseBaseObject
{
public:
	std::string description;
	std::string filePath; // Path to the file on disk (relative)
	bool isWritable;      // Whether BACnet AtomicWriteFile is permitted
	bool isReadable;      // Whether BACnet AtomicReadFile is permitted
	bool archive;         // BACnet Archive property (has been saved/archived)
	ExampleDatabaseFile();
};

class ExampleDatabase
{

public:
	ExampleDatabaseAnalogInput analogInput;
	ExampleDatabaseDevice device;
	ExampleDatabaseNetworkPort networkPort;
	ExampleDatabaseFile operationalCertFile; // instance 0 - OperationalCertificateFile
	ExampleDatabaseFile issuerCertFile1;		 // instance 1 - IssuerCertificateFiles[0]
	ExampleDatabaseFile issuerCertFile2;		 // instance 2 - IssuerCertificateFiles[1] (slot, may be empty)
	ExampleDatabaseFile csrFile;						 // instance 3 - CertificateSigningRequestFile

	// Constructor / Destructor
	ExampleDatabase();
	~ExampleDatabase();

	// Set all the objects to have a default value.
	void Setup();
};

#endif // __CASBACnetStackExampleDatabase_h__
