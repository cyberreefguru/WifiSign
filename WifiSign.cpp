// Do not remove the include below
#include "WifiSign.h"

// Internal Variables
static Configuration config;
static PubSubWrapper pubsubw;
static WifiWrapper wifiw;
static MatrixWrapper matrix;
static PortalWebServer pws;

// Indicates we have a command waiting for processing
static volatile boolean commandAvailable = false;

// Indicates we have an error that should stop processing
static volatile SystemStatus systemStatus = SystemStatus::UNKNOWN;

volatile uint8_t wifiImageCount = 0;
volatile uint8_t mqttImageCount = 0;

volatile boolean initialized = false;
volatile boolean showWifiInit = false;
volatile boolean showMqttInit = false;

// Internal functions
void parseCommand();
boolean initialize();
void startupPause();
void handleConfigurationPortal(PortalStatusCode status);
void runCaptivePortal();

/**
 * Setup function
 *
 */
void setup()
{
	setSystemStatus( SystemStatus::BOOTING );

	// Configure serial port
	Serial.begin(115200);

	// Basic HW setup
	pinMode(LED_BUILTIN, OUTPUT);     // Initialize the  pin as an output

	// This pauses the start process so the user can do things like
	// attach to the serial port and look at debug information :)
	startupPause();

	LOG_INFO(F("WiFI Sign Starting Up"));

	showWifiInit = true;
	showMqttInit = true;

	// Set the configuration for the portal
	pws.setConfiguration(&config);

	// Initialize the configuration object; configs stored in Flash
	LOG_DEBUG( F("Initializing configuration...") );
    if( config.initialize(true) == ConfigStatusCode::OK)
    {
    	LOG_DEBUG( F("Configuration initialized:") );
    	LOG_DEBUG( config.toString());
    }
    else
    {
		LOG_INFO("No configuration - running captive portal")
		runCaptivePortal(); // this function never returns!
    }

	// Initialize wifi, pubsub, and LEDs
	if( !initialize() )
	{
		LOG_INFO("Unable to initialize - running captive portal")
		runCaptivePortal(); // this function never returns!
	}
	else
	{
		LOG_INFO(F("Starting configuration portal..."));
		PortalStatusCode psc = pws.setupPortal();
		if( psc == PortalStatusCode::SUCCESS )
		{
			initialized = true;
			showWifiInit = false;
			showMqttInit = false;

			// Successfully initialized - put a message in the queue showing our IP address
			IPAddress ip = wifiw.getIpAddress();
			sprintf((char *)pubsubw.getBuffer(), CMD_SHOW_IP_ADDRESS, ip[0],ip[1],ip[2],ip[3], config.getMqttServer(), config.getMqttPort() );
			Serial.println((char *)pubsubw.getBuffer());
			setSystemStatus( SystemStatus::COMMAND_AVAILABLE );
			LOG_INFO("Ready!");
		}
		else
		{
			sprintf((char *)pubsubw.getBuffer(), CMD_SHOW_PORTAL_FAIL, pws.toString(psc) );
			Serial.println((char *)pubsubw.getBuffer());
			setSystemStatus( SystemStatus::COMMAND_AVAILABLE );
		}
	}

    Serial.println(F("** Initialization Complete **"));

}

/**
 * Loop function
 *
 */
void loop()
{
	// calls yield and other must run code
	worker();

	if( getSystemStatus() == SystemStatus::WIFI_RECONNECTION )
	{
		LOG_INFO("Reconnecting to WIFI");
		showWifiInit = false;
		matrix.clear();
		matrix.print(0, 8, 0x00ff0000, F("Lost"), false);
		matrix.print(0, 17, 0x00ff0000, F("WIFI"), false);
		matrix.print(0, 26, 0x00ff0000, F("Connection"), true);
		if( wifiw.initializeWifi() == WifiStatusCode::CONNECTED)
		{
			matrix.clear();
		}
		else
		{
			matrix.print(0, 8, 0x00ff0000, F("WIFI Error"), false);
			matrix.print(0, 17, 0x00ff0000, F("Please"), false);
			matrix.print(0, 26, 0x00ff0000, F("Reboot"), true);
			Helper::error();
		}
	}
	else if( getSystemStatus() == SystemStatus::MQTT_RECONNECTION )
	{
		LOG_INFO("Reconnecting to MQTT");
		showMqttInit = false;
		matrix.clear();
		matrix.print(0, 8, 0x00ff0000, F("Lost"), false);
		matrix.print(0, 17, 0x00ff0000, F("Queue"), false);
		matrix.print(0, 26, 0x00ff0000, F("Connection"), true);

		if( pubsubw.connect() )
		{
			matrix.clear();
			matrix.print(0, 8, 0x0000ff, F("Queue"), false);
			matrix.print(0, 17, 0x0000ff, F("Reconnected"), true);
		}
		else
		{
			matrix.clear();
			matrix.print(0, 8, 0x0000ff, F("Queue"), false);
			matrix.print(0, 17, 0x000ff, F("Connection"), false);
			matrix.print(0, 26, 0x0000ff, F("FAILED"), true);
			Helper::error();
		}
	}

	// Check if command is available
	if( isCommandAvailable() )
	{
		if( getSystemStatus() == SystemStatus::COMMAND_AVAILABLE )
		{
			parseCommand();
		}
	}

	PortalStatusCode status = pws.handleClient();
	if(status == PortalStatusCode::CONFIG_SUCCESS || status == PortalStatusCode::CONFIG_FAIL)
	{
		handleConfigurationPortal(status);
	}


} // end loop

/**
 * Initializes the node.  Returns true if node configured properly, false otherwise.
 *
 */
boolean initialize()
{
	boolean configured = false;

	wifiw.setRetryCount(config.getWifiTries());
	wifiw.setRetryDelay(250); // TODO - put retry delay in config file
	wifiw.setConnectionStatusCallback(wifiConnectionStatusCallback);

	pubsubw.setRetryCount(config.getMqttTries());
	pubsubw.setRetryDelay(250); // TODO - put retry delay in config file
	pubsubw.setConnectionStatusCallback(mqttConnectionStatusCallback);

	Serial.println(F("Configuring matrix..."));
	if( matrix.initialize() )
	{
		Serial.println(F("Configured matrix."));
		matrix.test();

		// Set up screen
		matrix.drawImage((char *)IMAGE_WIFI_GREY_0, 0, 0, true);
		matrix.print(2, matrix.getHeight()-1, 0x00FFFFFF, F("WIFI"), true);
		matrix.drawImage((char *)F("/resources/wifi-grey-0.bmp"), 32, 0, true);
		matrix.print(33, matrix.getHeight()-1, 0x00FFFFFF, F("MQTT"), true);

		// Initialize WIFI
		Serial.println(F("Configuring wifi..."));
		if( wifiw.initializeWifi(&config) == WifiStatusCode::CONNECTED)
		{
			wifiImageCount = 0;
			matrix.drawImage((char *)IMAGE_WIFI_GREEN_4, 0, 0, true);

			// Initialize pubsub
			Serial.println(F("Configuring queue..."));
			if( pubsubw.initialize(&config, &wifiw) )
			{
				matrix.drawImage((char *)IMAGE_WIFI_PURPLE_4, 32, 0, true);
				configured = true;

			}
			else
			{
				LOG_ERROR(F("Failed to configure queue"));
				matrix.drawImage((char *)IMAGE_WIFI_RED_4, 32, 0, true);
			}
		}
		else
		{
			LOG_ERROR(F("Failed to configure WIFI"));
			matrix.drawImage((char *)IMAGE_WIFI_RED_4, 0, 0, true);
		}
	}
	else
	{
		LOG_ERROR(F("Failed to configure matrix"));
	}

	return configured;

} // end initialize


void runCaptivePortal()
{
	setSystemStatus( SystemStatus::CONFIGURING );

	PortalStatusCode status = PortalStatusCode::FAIL;
	status = pws.runCaptivePortal();
	handleConfigurationPortal(status);
}

void handleConfigurationPortal(PortalStatusCode status)
{
	pws.stop(); // kill connections
	matrix.clear();
    if( status == PortalStatusCode::CONFIG_SUCCESS )
    {
		// Save new config file.
		ConfigStatusCode c = config.write();
		if( c == ConfigStatusCode::OK )
		{
			LOG_INFO(F("Configuration file written."));
			matrix.print(0,  8,  0x0000ff00, F("Success!"), false);
			matrix.print(0,  17,  0x000000ff, F("Please"), true);
			matrix.print(0,  26,  0x000000ff, F("Reboot"), true);
		}
		else
		{
			matrix.print(0,  8,  0x00ff0000, F("Error"), false);
			matrix.print(0,  17,  0x00ff0000, F("Saving"), true);
			LOG_ERROR(F("Error writing config file"));
		}
    }
    else
    {
    	LOG_ERROR("Unable to capture configuration");
		matrix.print(0,  8,  0x00ff0000, F("Error"), false);
		matrix.print(0,  17,  0x00ff0000, F("Getting"), true);
		matrix.print(0,  26,  0x000000ff, F("Config"), true);
    }
	Helper::error(); // never returns
}

/**
 * calls others functions while we are not busy.
 * This is required since we disabled interrupts
 * to use the FastLED library.
 *
 */
void worker()
{
	pubsubw.work(); // process queue
	wifiw.work(); // check for OTA

	// check if we are connected - wifi first, then queue
	if( wifiw.connected() )
	{
		if( pubsubw.connected() )
		{
			//statusIndicator.setStatus(Queue, Ok);
		}
		else
		{
			LOG_WARN("Queue disconnected.");
			setSystemStatus( SystemStatus::MQTT_RECONNECTION );
		}
	}
	else
	{
		LOG_WARN("Wifi disconnected.");
		setSystemStatus( SystemStatus::WIFI_RECONNECTION );
	}
	return;
}

/**
 * Parses command buffer
 *
 */
void parseCommand()
{
	// Just a little CYA
	if(getSystemStatus() != SystemStatus::COMMAND_AVAILABLE )
	{
		LOG_WARN("parseCommand called but COMMAND_AVAILABLE not set");
		return;
	}

	Command cmd;
	ConfigStatusCode csc = ConfigStatusCode::OK;

	// Reset command available flag
	//setCommandAvailable(false);
	setSystemStatus(SystemStatus::PROCESSING);

	LOG_DEBUG1( millis(), " - parsing command...");

	if( cmd.parse( (uint8_t *)pubsubw.getBuffer() ) )
	{
#ifdef __DEBUG
		cmd.dump();
#endif
		switch(cmd.getCommand())
		{
		case Clear:
			Serial.println(F("Command: Clear"));
			matrix.clear();
			break;
		case Show:
			Serial.println(F("Command: Show"));
			matrix.show();
			break;
		case SetPixel:
			Serial.println(F("Command: SetPixel"));
			matrix.drawPixel(cmd.getX(), cmd.getY(), cmd.getForegroundColor(), cmd.getShow());
			break;
		case Text:
			Serial.println(F("Command: Text"));
			matrix.print(cmd.getX(), cmd.getY(), cmd.getForegroundColor(), (char *)cmd.getText(), cmd.getShow());
			break;
		case Line:
			Serial.println(F("Command: Line"));
			matrix.drawLine(cmd.getX(), cmd.getY(), cmd.getWidth(), cmd.getForegroundColor(), cmd.getDirection(), cmd.getShow());
			break;
		case Rectangle:
			Serial.println(F("Command: Rectangle"));
			matrix.drawRectangle(cmd.getX(), cmd.getY(), cmd.getWidth(), cmd.getHeight(), cmd.getForegroundColor(), cmd.getShow());
			break;
		case Circle:
			Serial.println(F("Command: Circle"));
			matrix.drawCircle(cmd.getX(), cmd.getY(), cmd.getRadius(), cmd.getForegroundColor(), cmd.getShow());
			break;
		case RandomFill:
			Serial.println(F("Command: RandomFill"));
			break;
		case SetIntensity:
			Serial.println(F("Command: SetIntensity"));
			break;
		case Scroll:
			Serial.println(F("Command: Scroll"));
			matrix.scrollText(cmd.getX(),  cmd.getY(),  cmd.getForegroundColor(),  cmd.getBackgroundColor(), cmd.getSpeed(), cmd.getRepeat(), cmd.getDirection(), cmd.getText() );
			break;
		case SetImage:
			Serial.println(F("Command: SetImage"));
			// TODO: use draw from file
//			matrix.drawImage((ImageEnum)cmd.getIndex(), cmd.getShow());
			break;
		case Animate:
			Serial.println(F("Command: Animate"));
			matrix.animate( (AnimateEnum)cmd.getIndex(), cmd.getOnDuration());
			break;
		case SetConfigValue:
			Serial.println(F("Command: SetConfigValue"));
			matrix.clear();
			matrix.print(0, 8, 0x0000ff, F("Config"), false);
			matrix.print(0, 17, 0x0000ff, F("Value:"), true);
			csc = config.setConfigurationValue(cmd.getKey(), cmd.getValue() );
			if( csc == ConfigStatusCode::OK )
			{
				matrix.print(0, 26, 0x00ff00, F("SUCCESS"), true);
			}
			else
			{
				matrix.print(0, 26, 0x00ff0000, F("FAILED"), true);
			}
			break;
		case Response:
			Serial.println(F("Command: Response"));
			break;
		default:
			Serial.println(F("ERROR - UNKNOWN COMMAND"));
			break;
		} // end switch

	} // end if cmd parse = true
	else
	{
		Serial.println(F("ERROR - Unable to parse command"));
	}

	Serial.print( millis() );
	Serial.println(F(" - Command Complete"));

	setSystemStatus(SystemStatus::WAITING);
}

/**
 * Gets the system status variable
 */
SystemStatus getSystemStatus()
{
	return systemStatus;
}

/**
 * Sets the system status variable
 */
void setSystemStatus(SystemStatus status)
{
	systemStatus = status;
}


/**
 * Returns flag; true=new command is available
 */
boolean isCommandAvailable()
{
	// Return true if command is available or some sort of connection issue
	return (getSystemStatus() == SystemStatus::COMMAND_AVAILABLE ||
			getSystemStatus() == SystemStatus::WIFI_RECONNECTION ||
			getSystemStatus() == SystemStatus::MQTT_RECONNECTION );
//	return commandAvailable;
}

/**
 * Sets command available flag
 *
 */
void setCommandAvailable(boolean flag)
{
	if( flag )
	{
		setSystemStatus(SystemStatus::COMMAND_AVAILABLE);
	}
	else
	{
		setSystemStatus(SystemStatus::WAITING);
	}
//	commandAvailable = flag;
}


/**
 * Delay function with command check
 */
boolean commandDelay(uint32_t time)
{
	boolean cmd = isCommandAvailable();
	if( time == 0 ) return cmd;

	if( !cmd ) // if no command, pause
	{
		for (uint32_t i = 0; i < time; i++)
		{
			delay(1); // delay
			worker(); // yield to os and wifi
			cmd = isCommandAvailable();
			if (cmd)
			{
				break;
			}
		}
	}
	return cmd;
}

/**
 * Flashes LED during startup to tell user we're alive
 */
void startupPause()
{
	uint8_t i;

	Helper::setLed(OFF);
	for(i=0; i< 20; i++)
	{
		Helper::toggleLed();
		delay(250);
	}
	Helper::setLed(OFF);

}

/**
 * C function that calls class-level callback
 * Admittedly a hack, but it works
 *
 */
void pubsubCallback(char* topic, byte* payload, unsigned int length)
{
	pubsubw.callback(topic, payload, length);

}

void wifiConnectionStatusCallback()
{
	if( showWifiInit )
	{
		LOG_DEBUG1(F("WIFI Connection Status Callback: "), wifiImageCount );
		switch(wifiImageCount)
		{
			case 0:
				// Set image to 1
				matrix.drawImage((char *)IMAGE_WIFI_GREEN_1, 0, 0, true);

				// Set next image to 2
				wifiImageCount=1;
				break;
			case 1:
				// Set image to 2
				matrix.drawImage((char *)IMAGE_WIFI_GREEN_2,  0, 0, true);

				// Set next image to 3
				wifiImageCount=2;
				break;
			case 2:
				// Set image to 3
				matrix.drawImage((char *)IMAGE_WIFI_GREEN_3,  0, 0, true);

				// Set next image to 2
				wifiImageCount=3;
				break;
			case 3:
				// Set image to full
				matrix.drawImage((char *)IMAGE_WIFI_GREEN_4,  0, 0, true);

				// set next image to 1
				wifiImageCount=0;
				break;
			default:
				break;
		}
	}
	else
	{
		LOG_WARN("Initialization complete, but WIFI init called");
	}
}

void mqttConnectionStatusCallback()
{
	if( showMqttInit )
	{
		LOG_DEBUG1(F("WIFI Connection Status Callback: "), wifiImageCount );
		switch(wifiImageCount)
		{
			case 0:
				// Set image to 1
				matrix.drawImage((char *)IMAGE_WIFI_PURPLE_1, 32, 0, true);

				// Set next image to 2
				wifiImageCount=1;
				break;
			case 1:
				// Set image to 2
				matrix.drawImage((char *)IMAGE_WIFI_PURPLE_2,  32, 0, true);

				// Set next image to 3
				wifiImageCount=2;
				break;
			case 2:
				// Set image to 3
				matrix.drawImage((char *)IMAGE_WIFI_PURPLE_3,  32, 0, true);

				// Set next image to 2
				wifiImageCount=3;
				break;
			case 3:
				// Set image to full
				matrix.drawImage((char *)IMAGE_WIFI_PURPLE_4,  32, 0, true);

				// set next image to 1
				wifiImageCount=0;
				break;
			default:
				break;
		}
	}
}
