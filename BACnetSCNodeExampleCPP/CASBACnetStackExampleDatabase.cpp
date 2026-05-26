/*
 * BACnet Server Example C++
 * ----------------------------------------------------------------------------
 * CASBACnetStackExampleDatabase.cpp
 *
 * Sets up object names and properties in the database.
 *
 * Created by: Alex Fontaine
 */

#include "CASBACnetStackExampleDatabase.h"
#include "version.h"

ExampleDatabaseBaseObject::ExampleDatabaseBaseObject()
{
	this->objectName = "";
	this->instance = 0;
}

ExampleDatabaseAnalogInput::ExampleDatabaseAnalogInput() : ExampleDatabaseBaseObject()
{
	this->presentValue = 0.0f;
	this->covIncrement = 0.1f;
	this->reliability = 0;
	this->units = 0;
}

ExampleDatabaseDevice::ExampleDatabaseDevice() : ExampleDatabaseBaseObject()
{
	this->systemStatus = 0;
}

ExampleDatabaseNetworkPort::ExampleDatabaseNetworkPort() : ExampleDatabaseBaseObject()
{
	this->changesPending = false;
	memset(this->vmac, 0, sizeof(this->vmac));
	this->primaryHubUri = "";
}

ExampleDatabaseFile::ExampleDatabaseFile() : ExampleDatabaseBaseObject()
{
	this->description = "";
	this->filePath = "";
	this->isWritable = false;
	this->isReadable = true;
	this->archive = true;
}

ExampleDatabase::ExampleDatabase()
{
	this->Setup();
}

ExampleDatabase::~ExampleDatabase()
{
	this->Setup();
}

void ExampleDatabase::Setup()
{
	// Setup the Device Object
	this->device.objectName = "Chipkin Example SC Node";
	this->device.instance = 389990;
	this->device.systemStatus = 0; // Operational
	this->device.description = "https://github.com/chipkin/BACnetSCNodeExampleCPP";
	this->device.applicationSoftwareVersion = APPLICATION_VERSION;
	// Setup the Analog Input Object
	this->analogInput.objectName = "Example Analog Input";
	this->analogInput.instance = 1;
	this->analogInput.presentValue = 0.0f;
	this->analogInput.covIncrement = 0.1f;
	this->analogInput.reliability = 0; // No Fault Detected
	this->analogInput.units = 21;			 // Degrees Celsius

	// Setup the Network Port Object
	this->networkPort.objectName = "BACnet SC Network Port";
	this->networkPort.instance = 0;
	this->networkPort.changesPending = false;
	this->networkPort.vmac[0] = 0x00;
	this->networkPort.vmac[1] = 0x01;
	this->networkPort.vmac[2] = 0x02;
	this->networkPort.vmac[3] = 0x03;
	this->networkPort.vmac[4] = 0x04;
	this->networkPort.vmac[5] = 0x05;
	// Setup the File objects for the certificate files
	this->operationalCertFile.instance = 0;
	this->operationalCertFile.objectName = "Operational-Certificate";
	this->operationalCertFile.description = "Operational certificate";
	this->operationalCertFile.filePath = "../exampleCerts/opr-389000.pem";
	this->operationalCertFile.isWritable = true;
	this->operationalCertFile.isReadable = true;

	this->issuerCertFile1.instance = 1;
	this->issuerCertFile1.objectName = "Issuer-Certificate-1";
	this->issuerCertFile1.description = "Issuer CA certificate (primary)";
	this->issuerCertFile1.filePath = "../exampleCerts/iss-1.pem";
	this->issuerCertFile1.isWritable = true;
	this->issuerCertFile1.isReadable = true;

	this->issuerCertFile2.instance = 2;
	this->issuerCertFile2.objectName = "Issuer-Certificate-2";
	this->issuerCertFile2.description = "Issuer CA certificate (secondary, unused)";
	this->issuerCertFile2.filePath = "../exampleCerts/iss-2.pem"; // slot - file need not exist
	this->issuerCertFile2.isWritable = false; // Not writable since this is just a placeholder for a secondary issuer certificate that isn't actually used in this example
	this->issuerCertFile2.isReadable = false; // Not readable since this is just a placeholder for a secondary issuer certificate that isn't actually used in this example
	this->issuerCertFile2.archive = false; // Don't include in Archive since it's not actually used

	this->csrFile.instance = 3;
	this->csrFile.objectName = "Certificate-Signing-Request";
	this->csrFile.description = "Certificate signing request";
	this->csrFile.filePath = "../exampleCerts/csr-389000.pem";
	this->csrFile.isWritable = true;
	this->csrFile.isReadable = true;
}
