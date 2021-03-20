// Only modify this file to include
// - function definitions (prototypes)
// - include files
// - extern variable definitions
// In the appropriate section

#pragma once

#define __DEBUG

#include <Arduino.h>

#include "Debug.h"
#include "WifiWrapper.h"
#include "PubSubWrapper.h"
#include "MatrixWrapper.h"
#include "ClientGlobal.h"
#include "Command.h"
#include "Configuration.h"
#include "Helper.h"
#include "PortalWebServer.h"
#include "SystemStatus.h"

//#define NUM_PIXELS 4
//#define HALF NUM_PIXELS/2

#define STATUS_COLOR_UNKNOWN		GREY
#define	STATUS_COLOR_NONE			BLACK
#define STATUS_COLOR_BOOTING		ORANGE
#define	STATUS_COLOR_WAITING		GREEN
#define STATUS_COLOR_PROCESSING		BLUE

#define STATUS_COLOR_CONFIGURING	YELLOW
#define STATUS_COLOR_UPLOADING		MAGENTA

// Status colors associated with the specific LEDs
#define	STATUS_COLOR_OK				GREEN
#define STATUS_COLOR_ERROR			RED

#define STATUS_CONTROLLER			NEOPIXEL
#define STATUS_COLOR_ORDER			RGB
#define STATUS_COLOR_CORRECTION		TypicalLEDStrip
#define STATUS_DEFAULT_INTENSITY	25

#define CMD_SHOW_IP_ADDRESS "{\"cmd\": 22, \"t\": \"WIFI:%d.%d.%d.%d MQTT:%s:%d\", \"x\":0, \"y\":24, \"fg\":16777215, \"bg\":0, \"sp\":10, \"r\":0, \"d\":0}"
#define CMD_SHOW_PORTAL_FAIL "{\"cmd\": 22, \"t\": \"Unable to initialize setup portal: %s\", \"x\":0, \"y\":24, \"fg\":16777215, \"bg\":0, \"sp\":10, \"r\":0, \"d\":0}"


#define IMAGE_WIFI_BLUE_0	F("/resources/wifi-blue-0.bmp")
#define IMAGE_WIFI_BLUE_1	F("/resources/wifi-blue-1.bmp")
#define IMAGE_WIFI_BLUE_2	F("/resources/wifi-blue-2.bmp")
#define IMAGE_WIFI_BLUE_3	F("/resources/wifi-blue-3.bmp")
#define IMAGE_WIFI_BLUE_4	F("/resources/wifi-blue-4.bmp")

#define IMAGE_WIFI_RED_0	F("/resources/wifi-red-0.bmp")
#define IMAGE_WIFI_RED_1	F("/resources/wifi-red-1.bmp")
#define IMAGE_WIFI_RED_2	F("/resources/wifi-red-2.bmp")
#define IMAGE_WIFI_RED_3	F("/resources/wifi-red-3.bmp")
#define IMAGE_WIFI_RED_4	F("/resources/wifi-red-4.bmp")

#define IMAGE_WIFI_PURPLE_0	F("/resources/wifi-purple-0.bmp")
#define IMAGE_WIFI_PURPLE_1	F("/resources/wifi-purple-1.bmp")
#define IMAGE_WIFI_PURPLE_2	F("/resources/wifi-purple-2.bmp")
#define IMAGE_WIFI_PURPLE_3	F("/resources/wifi-purple-3.bmp")
#define IMAGE_WIFI_PURPLE_4	F("/resources/wifi-purple-4.bmp")

#define IMAGE_WIFI_GREEN_0	F("/resources/wifi-green-0.bmp")
#define IMAGE_WIFI_GREEN_1	F("/resources/wifi-green-1.bmp")
#define IMAGE_WIFI_GREEN_2	F("/resources/wifi-green-2.bmp")
#define IMAGE_WIFI_GREEN_3	F("/resources/wifi-green-3.bmp")
#define IMAGE_WIFI_GREEN_4	F("/resources/wifi-green-4.bmp")

#define IMAGE_WIFI_GREY_0	F("/resources/wifi-grey-0.bmp")

#define IMAGE_RECORD_RED	F("/resources/record-red.bmp")
#define IMAGE_RECORD_WHITE	F("/resources/record-white.bmp")

void worker();

SystemStatus getSystemStatus();
void setSystemStatus(SystemStatus status);

boolean isCommandAvailable();
void setCommandAvailable(boolean flag);
boolean commandDelay(uint32_t time);

void pubsubCallback(char* topic, byte* payload, unsigned int length);
void wifiConnectionStatusCallback();
void mqttConnectionStatusCallback();
void runConfigurationPortal();



