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
	this->caCertPath = "";
	this->clientCertPath = "";
	this->clientKeyPath = "";
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
	this->device.objectName = "Example BACnet SC Node";
	this->device.instance = 389999;
	this->device.systemStatus = 0; // Operational
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
	this->networkPort.caCertPath = "../exampleCerts/iss-1.pem";
	this->networkPort.clientCertPath = "../exampleCerts/opr-389000.pem";
	this->networkPort.clientKeyPath = "../exampleCerts/key-389000.pem";
}
