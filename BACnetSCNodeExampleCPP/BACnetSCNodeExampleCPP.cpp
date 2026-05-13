/*
 * BACnet SC Node Example C++
 * ----------------------------------------------------------------------------
 * BACnetSCNodeExample.cpp
 *
 * In this CAS BACnet Stack example, we create a simple BACnet SC node with various
 * objects and properties from an example database.
 *
 * More information https://github.com/chipkin/BACnetSCNodeExampleCPP
 *
 * This file contains the 'main' function. Program execution begins and ends there.
 *
 * Created by: Alex Fontaine
 */

#include "CASBACnetStackAdapter.h" // Function pointer typedefs
// !!!!!! This file is part of the CAS BACnet Stack. Please contact Chipkin for more information.

#include "CASBACnetStackExampleConstants.h"
#include "CASBACnetStackExampleDatabase.h"
#include "BACnetSCWebsocketClient.h"
#include "CIBuildSettings.h"
#include "version.h"

// Standard library includes
#include <cstdio>
#include <cstring>
#include <cctype>
#include <ctime>

// Platform-specific includes and functions
#ifndef __GNUC__ // Windows
#include <windows.h>
#include <conio.h> // _kbhit
#else							 // Linux
#include <sys/ioctl.h>
#include <termios.h>
bool _kbhit()
{
	termios term;
	tcgetattr(0, &term);
	termios term2 = term;
	term2.c_lflag &= ~ICANON;
	tcsetattr(0, TCSANOW, &term2);
	int byteswaiting;
	ioctl(0, FIONREAD, &byteswaiting);
	tcsetattr(0, TCSANOW, &term);
	return byteswaiting > 0;
}
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
void Sleep(int milliseconds)
{
	usleep(milliseconds * 1000);
}

#endif // __GNUC__

// Globals
// =======================================
ExampleDatabase g_exampleDatabase;				 // The example database that stores current values.
BACnetSCWebsocketClient g_websocketClient; // Outbound WSS client connection to the BACnet/SC Hub.
bool g_initialIAmSent = false;						 // Tracks whether the I-Am has been sent after connection.
std::string g_clientKeyPassword;					 // Passphrase for the encrypted client private key.

// ============================================================
// UUID: 16-byte unique identifier for this device
// Generate once, store persistently (e.g., in EEPROM or config file)
// ============================================================
uint8_t g_uuid[16] = {
		0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
std::string g_primaryHubUri = "wss://127.0.0.1:4443/bacnet-sc";

// Function Prototypes
// =======================================
void RegisterCallbacks();
bool SetupDevice();
bool ConfigureBACnetSC();
bool SendIAm();
bool DoUserInput();
void PrintHelp();

// Callback Function Prototypes
// =======================================

// Message Functions
uint16_t CallbackReceiveMessage(uint8_t *message, const uint16_t maxMessageLength, uint8_t *sourceConnectionString, uint8_t *sourceConnectionStringLength, uint8_t *destinationConnectionString, uint8_t *destinationConnectionStringLength, const uint8_t maxConnectionStringLength, uint8_t *networkType);
uint16_t CallbackSendMessage(const uint8_t *message, const uint16_t messageLength, const uint8_t *connectionString, const uint8_t connectionStringLength, const uint8_t networkType, bool broadcast);

// System Functions
time_t CallbackGetSystemTime();

// Get Property Functions
bool CallbackGetPropertyCharString(const uint32_t deviceInstance, const uint16_t objectType, const uint32_t objectInstance, const uint32_t propertyIdentifier, char *value, uint32_t *valueElementCount, const uint32_t maxElementCount, uint8_t *encodingType, const bool useArrayIndex, const uint32_t propertyArrayIndex);
bool CallbackGetPropertyEnum(uint32_t deviceInstance, uint16_t objectType, uint32_t objectInstance, uint32_t propertyIdentifier, uint32_t *value, bool useArrayIndex, uint32_t propertyArrayIndex);
bool CallbackGetPropertyReal(uint32_t deviceInstance, uint16_t objectType, uint32_t objectInstance, uint32_t propertyIdentifier, float *value, bool useArrayIndex, uint32_t propertyArrayIndex);

// BACnetSC Callback Functions
bool CallbackInitiateWebsocket(const char *websocketUri, const uint32_t websocketUriLength);
void CallbackDisconnectWebsocket(const char *websocketUri, const uint32_t websocketUriLength);
void CallbackBACnetSCStateChange(const uint32_t deviceInstance, const uint32_t networkPortInstance, const uint8_t stateMachine, const uint8_t previousState, const uint8_t newState, const char *websocketUri, const uint32_t websocketUriLength);

int main(int argc, char **argv)
{
	// Parse command-line arguments
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--key-password") == 0 && i + 1 < argc)
		{
			g_clientKeyPassword = argv[++i];
		}
		else if (strcmp(argv[i], "--hub-uri") == 0 && i + 1 < argc)
		{
			g_primaryHubUri = argv[++i];
		}
	}

	// Print the application version information
	printf("CAS BACnet Stack SC Node Example v%s.%u\n", APPLICATION_VERSION, CIBUILDNUMBER);
	printf("BACnetSCWebsocketClient v%s\n", BACNET_SC_WEBSOCKET_CLIENT_VERSION);
	printf("https://github.com/chipkin/BACnetSCNodeExampleCPP\n\n");
	printf("Hub URI: %s\n", g_primaryHubUri.c_str());

	// 1. Load the CAS BACnet stack functions
	// ---------------------------------------------------------------------------
	printf("FYI: Loading CAS BACnet Stack functions... ");
	if (!LoadBACnetFunctions())
	{
		fprintf(stderr, "Failed to load the functions from the DLL\n");
		return 0;
	}
	printf("OK\n");
	printf("FYI: CAS BACnet Stack version: %u.%u.%u.%u\n", fpGetAPIMajorVersion(), fpGetAPIMinorVersion(), fpGetAPIPatchVersion(), fpGetAPIBuildVersion());

	// 2. Setup the callbacks
	// ---------------------------------------------------------------------------
	RegisterCallbacks();

	// 3. Setup the device and objects in the stack
	// ---------------------------------------------------------------------------
	if (!SetupDevice())
	{
		fprintf(stderr, "Failed to setup the device and objects\n");
		return 0;
	}

	// 4. Configure BACnet/SC
	// ---------------------------------------------------------------------------
	if (!ConfigureBACnetSC())
	{
		fprintf(stderr, "Failed to configure BACnet/SC settings\n");
		return 0;
	}

	// 5. Main loop (I-Am is sent once the WebSocket connection is established)
	printf("Entering main loop...\n");
	for (;;)
	{
		fpTick();
		g_websocketClient.Service();
		if (!g_initialIAmSent && g_websocketClient.IsConnected())
		{
			SendIAm();
			g_initialIAmSent = true;
		}

		if (!DoUserInput())
		{
			// User press 'q' to quit the example application.
			break;
		}

		Sleep(0);
	}

	// Clean up and exit
	g_websocketClient.Disconnect();
	fpSetBACnetSCWebSocketStatus(g_websocketClient.GetHubUri().c_str(), (uint16_t)g_websocketClient.GetHubUri().length(), 0, 0); // Inform stack of disconnection
	return 0;
}

// Helper Functions
// =======================================

void RegisterCallbacks()
{
	// Message Callback Functions
	fpRegisterCallbackReceiveMessage(CallbackReceiveMessage);
	fpRegisterCallbackSendMessage(CallbackSendMessage);

	// System Time Callback Functions
	fpRegisterCallbackGetSystemTime(CallbackGetSystemTime);

	// Get Property Callback Functions
	// fpRegisterCallbackGetPropertyBitString(CallbackGetPropertyBitString);
	// fpRegisterCallbackGetPropertyBool(CallbackGetPropertyBool);
	fpRegisterCallbackGetPropertyCharacterString(CallbackGetPropertyCharString);
	// fpRegisterCallbackGetPropertyDate(CallbackGetPropertyDate);
	// fpRegisterCallbackGetPropertyDouble(CallbackGetPropertyDouble);
	fpRegisterCallbackGetPropertyEnumerated(CallbackGetPropertyEnum);
	// fpRegisterCallbackGetPropertyOctetString(CallbackGetPropertyOctetString);
	// fpRegisterCallbackGetPropertySignedInteger(CallbackGetPropertyInt);
	fpRegisterCallbackGetPropertyReal(CallbackGetPropertyReal);
	// fpRegisterCallbackGetPropertyTime(CallbackGetPropertyTime);
	// fpRegisterCallbackGetPropertyUnsignedInteger(CallbackGetPropertyUInt);

	// BACnet SC Callback Functions
	fpRegisterCallbackInitiateWebsocket(CallbackInitiateWebsocket);
	fpRegisterCallbackDisconnectWebsocket(CallbackDisconnectWebsocket);
	fpRegisterCallbackBACnetSCStateChange(CallbackBACnetSCStateChange);
}

bool SetupDevice()
{
	printf("Setting up BACnet device, instance=%u\n", g_exampleDatabase.device.instance);

	// --------------------------------------------------------
	// 1. Create the Device object
	// Every BACnet device starts here. The instance number is your
	// unique device ID on the BACnet network.
	// --------------------------------------------------------
	if (!fpAddDevice(g_exampleDatabase.device.instance))
	{
		fprintf(stderr, "ERROR: Failed to add device\n");
		return false;
	}

	// --------------------------------------------------------
	// 2. Enable BACnet services
	// Some services are already mandatory (ReadProperty, WhoIs, WhoHas).
	// You must explicitly enable any others you want to support.
	// --------------------------------------------------------
	if (!fpSetServiceEnabled(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::SERVICE_READ_PROPERTY_MULTIPLE, true))
	{
		fprintf(stderr, "ERROR: Failed to enable ReadPropertyMultiple service\n");
		return false;
	}
	if (!fpSetServiceEnabled(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::SERVICE_SUBSCRIBE_COV, true))
	{
		fprintf(stderr, "ERROR: Failed to enable SubscribeCOV service\n");
		return false;
	}

	// --------------------------------------------------------
	// 3. Add the Analog Input object (instance 1)
	// This is the only data object in this example.
	// --------------------------------------------------------
	printf("Adding AnalogInput, instance=%u\n", g_exampleDatabase.analogInput.instance);
	if (!fpAddObject(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT, g_exampleDatabase.analogInput.instance))
	{
		fprintf(stderr, "ERROR: Failed to add Analog Input\n");
		return false;
	}
	fpSetPropertyEnabled(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT, g_exampleDatabase.analogInput.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_RELIABILITY, true);
	fpSetPropertyEnabled(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT, g_exampleDatabase.analogInput.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_COV_INCREMENT, true);
	fpSetPropertyEnabled(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_DEVICE, g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_DESCRIPTION, true);
	fpSetPropertyEnabled(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_DEVICE, g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_APPLICATION_SOFTWARE_VERSION, true);

	// --------------------------------------------------------
	// 4. Add the SC Network Port object
	// This is mandatory for BACnet/SC. It represents the SC
	// transport interface and holds SC configuration properties.
	//
	// --------------------------------------------------------
	printf("Adding SC NetworkPort, instance=%u\n", g_exampleDatabase.networkPort.instance);
	if (!fpAddNetworkPortObject(
					g_exampleDatabase.device.instance,
					g_exampleDatabase.networkPort.instance,
					CASBACnetStackExampleConstants::NETWORK_TYPE_SECURE_CONNECT,				 // 11 = secureConnect
					CASBACnetStackExampleConstants::PROTOCOL_LEVEL_BACNET_APPLICATION,	 // 2
					CASBACnetStackExampleConstants::NETWORK_PORT_LOWEST_PROTOCOL_LAYER)) // 4194303
	{
		fprintf(stderr, "ERROR: Failed to add SC NetworkPort\n");
		return false;
	}

	printf("Device setup complete.\n");
	return true;
}

bool ConfigureBACnetSC()
{
	printf("Configuring BACnet/SC settings...\n");
	// Set the 16-byte UUID for this device.
	// The stack includes this UUID in Connect-Request packets so the hub
	// can identify and track this device across reconnections.
	if (!fpSetBACnetSCUuid(g_uuid, 16))
	{
		fprintf(stderr, "ERROR: Failed to set BACnet/SC UUID\n");
		return false;
	}

	// Configure the Hub Connector (Node role).
	//
	// Parameters:
	//   vmac              - 6-byte VMAC for this device. For automatic VMAC
	//                       assignment, pass NULL instead of an all-zero VMAC.
	//   vmacLength        - 6 when providing a VMAC, or 0 when vmac is NULL
	//                       to request automatic assignment
	//   primaryHubUri     - the Hub's listen URI to connect to
	//   primaryHubUriLen  - length of the primary URI
	//   failoverHubUri    - a backup hub URI (optional, can be NULL)
	//   failoverHubUriLen - 0 if no failover
	//
	// When this is set, the stack will call CallbackInitiateWebsocket
	// to open the WebSocket connection. Your code must do the actual connecting.
	if (!fpSetBACnetSCHubConnector(
					g_exampleDatabase.networkPort.vmac, 6,
					g_primaryHubUri.c_str(),
					(uint16_t)g_primaryHubUri.length(),
					NULL, 0)) // no failover hub
	{
		fprintf(stderr, "ERROR: Failed to configure Hub Connector\n");
		return false;
	}

	return true;
}

bool SendIAm()
{
	// For BACnet/SC, the "connection string" for a broadcast I-Am is the
	// Hub's accept URI (for a Hub) or the Hub URI (for a Node).
	// The 'broadcast' flag tells the stack to forward to all connected peers.
	printf("Sending I-Am broadcast...\n");
	if (!fpSendIAm(
					g_exampleDatabase.device.instance,
					(const uint8_t *)g_primaryHubUri.c_str(),
					(uint16_t)g_primaryHubUri.length(),
					CASBACnetStackExampleConstants::CAS_NETWORK_TYPE_SC, // 2 = BACnet/SC
					true,																								 // broadcast (send to all peers)
					65535,																							 // hopCount (max)
					NULL, 0))																						 // no specific network/address
	{
		fprintf(stderr, "ERROR: Failed to send I-Am\n");
		return false;
	}
	printf("I-Am broadcast sent successfully.\n");
	return true;
}

bool DoUserInput()
{
	// Check to see if the user hit any key
	if (!_kbhit())
	{
		// No keys have been hit
		return true;
	}

	// Extract the letter that the user hit and convert it to lower case
	char action = tolower(getchar());

	// Handle the action
	switch (action)
	{
	// Increase Analog Input
	case 'i':
	{
		g_exampleDatabase.analogInput.presentValue += 1.1f;
		printf("Increasing Analog Input to %f\n", g_exampleDatabase.analogInput.presentValue);
		if (fpValueUpdated != NULL)
		{
			fpValueUpdated(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT, g_exampleDatabase.analogInput.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_PRESENT_VALUE);
		}
		break;
	}
	// Decrease Analog Input
	case 'd':
	{
		g_exampleDatabase.analogInput.presentValue -= 1.3f;
		printf("Decreasing Analog Input to %f\n", g_exampleDatabase.analogInput.presentValue);
		if (fpValueUpdated != NULL)
		{
			fpValueUpdated(g_exampleDatabase.device.instance, CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT, g_exampleDatabase.analogInput.instance, CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_PRESENT_VALUE);
		}
		break;
	}
	// Toggle Analog Input Reliability
	case 't':
	{
		if (g_exampleDatabase.analogInput.reliability == CASBACnetStackExampleConstants::RELIABILITY_NO_FAULT_DETECTED)
		{
			printf("Setting Analog Input reliability to UNRELIABLE_OTHER (7)\n");
			g_exampleDatabase.analogInput.reliability = CASBACnetStackExampleConstants::RELIABILITY_UNRELIABLE_OTHER; // unreliable-other (7)
		}
		else
		{
			printf("Setting Analog Input reliability to NO_FAULT_DETECTED (0)\n");
			g_exampleDatabase.analogInput.reliability = CASBACnetStackExampleConstants::RELIABILITY_NO_FAULT_DETECTED; // no-fault-detected (0)
		}
		break;
	}
	// Quit
	case 'q':
	{
		return false;
	}
	// Help
	case 'h':
	{
		PrintHelp();
		break;
	}
	default:
		break;
	};

	return true;
}

void PrintHelp()
{
	// Print the Help
	printf("\n\n");
	printf("CAS BACnet Stack SC Node Example v%s.%u\n", APPLICATION_VERSION, CIBUILDNUMBER);
	printf("BACnetSCWebsocketClient v%s\n", BACNET_SC_WEBSOCKET_CLIENT_VERSION);
	printf("https://github.com/chipkin/BACnetSCNodeExampleCPP\n\n");

	printf("Command-line arguments:\n");
	printf("  --hub-uri <uri>        Hub WebSocket URI (default: wss://127.0.0.1:4443/bacnet-sc)\n");
	printf("  --key-password <pass>  Private key passphrase for mutual TLS\n");
	printf("\n");
	printf("Help:\n");
	printf("i - (i)ncrease Analog Input by 1.1\n");
	printf("d - (d)ecrease Analog Input by 1.3\n");
	printf("t - (t)oggle Analog Input Reliability\n");
	printf("h - (h)elp\n");
	printf("q - (q)uit\n");
	printf("\n");
}

// Callback Functions
// =======================================

// Callback used by the BACnet Stack to check if there is a message to process
uint16_t CallbackReceiveMessage(uint8_t *message, const uint16_t maxMessageLength, uint8_t *sourceConnectionString, uint8_t *sourceConnectionStringLength, uint8_t *destinationConnectionString, uint8_t *destinationConnectionStringLength, const uint8_t maxConnectionStringLength, uint8_t *networkType)
{
	uint16_t bytesReceived = g_websocketClient.Receive(message, maxMessageLength);
	if (bytesReceived > 0)
	{
		// Tell the stack this message arrived over BACnet/SC and came from the Hub.
		*networkType = CASBACnetStackExampleConstants::CAS_NETWORK_TYPE_SC;
		const std::string &uri = g_websocketClient.GetHubUri();
		uint8_t uriLen = static_cast<uint8_t>(
				uri.size() < maxConnectionStringLength ? uri.size() : maxConnectionStringLength);
		memcpy(sourceConnectionString, uri.c_str(), uriLen);
		*sourceConnectionStringLength = uriLen;
	}
	return bytesReceived;
}

// Callback used by the BACnet Stack to send a BACnet message
uint16_t CallbackSendMessage(const uint8_t *message, const uint16_t messageLength, const uint8_t *connectionString, const uint8_t connectionStringLength, const uint8_t networkType, bool broadcast)
{
	if (g_websocketClient.Send(message, messageLength))
	{
		return messageLength;
	}
	return 0;
}

// Callback used by the BACnet Stack to get the current time
time_t CallbackGetSystemTime()
{
	return time(0);
}

// Callback used by the BACnet Stack to get Character String property values from the user
bool CallbackGetPropertyCharString(const uint32_t deviceInstance, const uint16_t objectType, const uint32_t objectInstance, const uint32_t propertyIdentifier, char *value, uint32_t *valueElementCount, const uint32_t maxElementCount, uint8_t *encodingType, const bool useArrayIndex, const uint32_t propertyArrayIndex)
{
	if (deviceInstance == g_exampleDatabase.device.instance)
	{
		if (propertyIdentifier == CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_OBJECT_NAME)
		{
			if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_DEVICE)
			{
				strncpy(value, g_exampleDatabase.device.objectName.c_str(), maxElementCount);
				*valueElementCount = (uint32_t)g_exampleDatabase.device.objectName.length();
				*encodingType = CASBACnetStackExampleConstants::ENCODING_TYPE_UTF8;
				return true;
			}
			else if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT)
			{
				strncpy(value, g_exampleDatabase.analogInput.objectName.c_str(), maxElementCount);
				*valueElementCount = (uint32_t)g_exampleDatabase.analogInput.objectName.length();
				*encodingType = CASBACnetStackExampleConstants::ENCODING_TYPE_UTF8;
				return true;
			}
			else if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_NETWORK_PORT)
			{
				strncpy(value, g_exampleDatabase.networkPort.objectName.c_str(), maxElementCount);
				*valueElementCount = (uint32_t)g_exampleDatabase.networkPort.objectName.length();
				*encodingType = CASBACnetStackExampleConstants::ENCODING_TYPE_UTF8;
				return true;
			}
		}
		else if (propertyIdentifier == CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_DESCRIPTION)
		{
			if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_DEVICE)
			{
				strncpy(value, g_exampleDatabase.device.description.c_str(), maxElementCount);
				*valueElementCount = (uint32_t)g_exampleDatabase.device.description.length();
				*encodingType = CASBACnetStackExampleConstants::ENCODING_TYPE_UTF8;
				return true;
			}
		}
		else if (propertyIdentifier == CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_APPLICATION_SOFTWARE_VERSION)
		{
			if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_DEVICE)
			{
				strncpy(value, g_exampleDatabase.device.applicationSoftwareVersion.c_str(), maxElementCount);
				*valueElementCount = (uint32_t)g_exampleDatabase.device.applicationSoftwareVersion.length();
				*encodingType = CASBACnetStackExampleConstants::ENCODING_TYPE_UTF8;
				return true;
			}
		}
	}

	return false;
}

bool CallbackGetPropertyEnum(uint32_t deviceInstance, uint16_t objectType, uint32_t objectInstance, uint32_t propertyIdentifier, uint32_t *value, bool useArrayIndex, uint32_t propertyArrayIndex)
{
	if (deviceInstance == g_exampleDatabase.device.instance)
	{
		if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT && objectInstance == g_exampleDatabase.analogInput.instance)
		{
			if (propertyIdentifier == CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_RELIABILITY)
			{
				*value = g_exampleDatabase.analogInput.reliability;
				return true;
			}
		}
	}
	return false;
}

// Callback used by the BACnet Stack to get Real property values from the user
bool CallbackGetPropertyReal(uint32_t deviceInstance, uint16_t objectType, uint32_t objectInstance, uint32_t propertyIdentifier, float *value, bool useArrayIndex, uint32_t propertyArrayIndex)
{
	if (deviceInstance == g_exampleDatabase.device.instance)
	{
		if (objectType == CASBACnetStackExampleConstants::OBJECT_TYPE_ANALOG_INPUT && objectInstance == g_exampleDatabase.analogInput.instance)
		{
			if (propertyIdentifier == CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_PRESENT_VALUE)
			{
				*value = g_exampleDatabase.analogInput.presentValue;
				return true;
			}
			else if (propertyIdentifier == CASBACnetStackExampleConstants::PROPERTY_IDENTIFIER_COV_INCREMENT)
			{
				*value = g_exampleDatabase.analogInput.covIncrement;
				return true;
			}
		}
	}

	return false;
}

// Callback gets called when the CAS BACnet Stack needs to start listening for inbound BACnet / SC websocket connections
bool CallbackInitiateWebsocket(const char *websocketUri, const uint32_t websocketUriLength)
{
	g_websocketClient.Configure(
			std::string(websocketUri, websocketUriLength),
			g_exampleDatabase.networkPort.caCertPath,
			g_exampleDatabase.networkPort.clientCertPath,
			g_exampleDatabase.networkPort.clientKeyPath,
			g_clientKeyPassword);
	g_websocketClient.SetStatusCallback(fpSetBACnetSCWebSocketStatus);
	return g_websocketClient.Connect();
}

// Callback gets called when the CAS BACnet Stack needs to stop listening for inbound BACnet / SC websocket connections
void CallbackDisconnectWebsocket(const char *websocketUri, const uint32_t websocketUriLength)
{
	g_websocketClient.Disconnect();
}

// Callback gets called when the CAS BACnet Stack changes an observable BACnet / SC state machine value (debug purposes)
void CallbackBACnetSCStateChange(const uint32_t deviceInstance, const uint32_t networkPortInstance, const uint8_t stateMachine, const uint8_t previousState, const uint8_t newState, const char *websocketUri, const uint32_t websocketUriLength)
{
	printf("BACnet/SC State Change: deviceInstance=%u, networkPortInstance=%u, stateMachine=%u, previousState=%u, newState=%u, websocketUri=%.*s\n",
				 deviceInstance, networkPortInstance, stateMachine, previousState, newState, websocketUriLength, websocketUri);
	return;
}
