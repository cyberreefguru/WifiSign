/*
 * CaptivePortal.h
 *
 *  Created on: Feb 3, 2021
 *      Author: tsasala
 */

#pragma once

#include <SPI.h>
#include <WiFiNINA.h>

#include "Debug.h"

#include "mimetable.h"
#include "DNSServer.h"

#include "WifiWrapper.h"
#include "Helper.h"
#include "Parameter.h"


enum HTTPMethod
{
  HTTP_ANY,
  HTTP_GET,
  HTTP_HEAD,
  HTTP_POST,
  HTTP_PUT,
  HTTP_PATCH,
  HTTP_DELETE,
  HTTP_OPTIONS
};

enum HTTPUploadStatus
{
  UPLOAD_FILE_START,
  UPLOAD_FILE_WRITE,
  UPLOAD_FILE_END,
  UPLOAD_FILE_ABORTED
};

enum HTTPClientStatus
{
  HC_NONE,
  HC_WAIT_READ,
  HC_WAIT_CLOSE
};

enum HTTPAuthMethod
{
  BASIC_AUTH,
  DIGEST_AUTH
};



enum class PortalStatusCode {SUCCESS, FAILURE, PARSE_ERROR, CONFIG_SUCCESS, CONFIG_FAIL, UNDEFINED_URL};


#define HTTP_MAX_DATA_WAIT    5000 //ms to wait for the client to send the request
#define HTTP_MAX_POST_WAIT    5000 //ms to wait for POST data to arrive
#define HTTP_MAX_SEND_WAIT    5000 //ms to wait for data chunk to be ACKed
#define HTTP_MAX_CLOSE_WAIT   2000 //ms to wait for the client to close the connection

#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)

#define MAX_POST_PARAMETER_SIZE 	32

class CaptivePortal
{
public:
	CaptivePortal();
	void setup();
	PortalStatusCode loop();

protected:
	void udpScan();
	void printAnswers();
	void printWiFiStatus();
	int send(int code, const char* content_type, const String& content);
	void doRoot( );
	PortalStatusCode handleRoot();
	PortalStatusCode handleConfigSave();
//	String createResponse(uint16_t statusCode, const String &body);

	PortalStatusCode handleClient();
	PortalStatusCode handleRequest();
	boolean parseRequest();
	boolean parseForm(String boundary, uint32_t len);
	String urlDecode(const String& text);

    const byte DNS_PORT = 53;

    WiFiClient	currentClient;
    HTTPMethod	currentMethod;
    String		currentUri;
    Parameter 	currentParameters[MAX_POST_PARAMETER_SIZE];
    uint8_t 	currentParameterSize = 0;

    HTTPClientStatus  currentStatus;
    unsigned long     statusChange;

    const char *head="<html> <head> <title>WiFi Sign Setup</title> <meta name='viewport' content='width=device-width, initial-scale=1.0'> <style> * { box-sizing: border-box; } [class*='col-'] { float: left; padding: 15px; } [class*='col-'] { /* For mobile phones: */ width: 100%; } @media only screen and (min-width: 600px) { /* For tablets: */ .col-s-25 { width: 25%; } .col-s-50 { width: 50%; } .col-s-75 { width: 75%; } .col-s-100 { width: 100%; } } @media only screen and (min-width: 768px) { /* For desktop: */ .col-25 { width: 25%; } .col-50 { width: 50%; } .col-75 { width: 75%; } .col-100 { width: 100%; } } body { color: white; background-color: #333333; font-family: verdana; } .header { /* background-color: #9933cc; */ /* padding: 10px 0px 0px 0px; */ font-family: verdana; color: GoldenRod; font-size: 110%; font-weight: bolder; text-align: center; } .footer { background-color: #6622cc; color: #ffffff; text-align: center; font-size: 12px; padding: 1px 0px 1px 0px; } .t_row { margin: 0px 10px 0px 10px } .t_row::after { content: ''; clear: both; display: table; } .f_row { display: flex; flex-direction: row; } .l_row { display: flex; flex-direction: row; } .f_input { font-size: 1em; padding: 2px 0px 2px 0px; margin: 0px 0px 10px 0px; } .f_label { font-weight: bold; padding: 0px 0px 5px 0px; } .f_note { font-style: italic; font-size: small; margin: 2.5px 0px 0px 5px; } .f_but { border: 0; border-radius: 0.3rem; background-color: #1fa3ec; color: #fff; line-height: 2.4rem; font-size: 1.2rem; } input:invalid { border: 3px solid red; } input[type=checkbox] { width: 20px; } </style> <script> function togglePass() { toggleType(document.getElementById('wifiPass')); toggleType(document.getElementById('mqttPass')); } function toggleType(field) { if (field.type === 'password') { field.type = 'text'; } else { field.type = 'password'; } } </script> </head>";
    const char *root_body="<body> <div class='t_row'> <div class='col-25 col-s-25'> </div> <div class='col-50 col-s-75'> <div class='header'>WiFi Sign Setup</div> </div> <div class='col-25 col-s-100'> </div> </div> <form method=POST action='/save_config'> <div class='t_row'> <div class='col-25 col-s-25'> </div> <div class='col-50 col-s-75'> <div class='f_row'> <label class='f_label' for='hostName'>Host Name</label> <div class='f_note'>(required. lowercase letters, numbers, and _ only)</div> </div> <div class='f_row'> <input class='f_input' id='hostName' required name='hostName' maxlength=15 placeholder='WiFiSign' pattern='[a-z0-9_]*' value='%s'> </div> <div class='f_row'> <label class='f_label' for='wifiSSID'>WIFI SSID</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='wifiSSID' required name='wifiSSID' maxlength=31 placeholder='SSID' value='%s'> </div> <div class='f_row'> <label class='f_label' for='wifiPass'>WIFI Password</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='wifiPass' required name='wifiPass' type='password' maxlength=31 placeholder='password' value='%s'> </div> <div class='f_row'> <label class='f_label' for='mqttServer'>MQTT Server</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='mqttServer' required name='mqttServer' maxlength=31 placeholder='server' value='%s'> </div> <div class='f_row'> <label class='f_label' for='mqttPort'>MQTT Port</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='mqttPort' required name='mqttPort' type='number' maxlength=5 placeholder='1883' value='%d'> </div> <div class='f_row'> <label class='f_label' for='mqttUser'>MQTT User</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='mqttUser' name='mqttUser' maxlength=31 placeholder='user' value='%s'> </div> <div class='f_row'> <label class='f_label' for='mqttPass'>MQTT Password</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='mqttPass' name='mqttPass' type='password' maxlength=31 placeholder='password' value='%s'> </div> <div class='f_row'> <label class='f_label' for='mychannel'>MQTT Channel</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='mychannel' name='mychannel' maxlength=31 placeholder='My Channel' value='%s'> </div> <div class='f_row'> <label class='f_label' for='regchannel'>MQTT Registration Channel</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='regchannel' name='regchannel' maxlength=31 placeholder='Reg Channel' value='%s'> </div> <div class='f_row'> <label class='f_label' for='respchannel'>MQTT Response Channel</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='respchannel' name='respchannel' maxlength=31 placeholder='Resp Channel' value='%s'> </div> <div class='f_row'> <label class='f_label' for='allchannel'>MQTT All Channel</label> <div class='f_note'>(required)</div> </div> <div class='f_row'> <input class='f_input' id='allchannel' name='allchannel' maxlength=31 placeholder='All Channel' value='%s'> </div> <div class='f_row'> <input class='f_input' type='checkbox' onclick='togglePass()'>Show Password </div> <div class='f_row'><button class='f_but' type='submit'>save settings</button></div> <form method='get' action='reboot'> <div class='f_row'> <button class='f_but' type='submit'>reboot device</button> </div> </form> </div> <div class='col-25 col-s-100'> </div> </div> </form> <div class='footer'> <p>Copyright CyberReefGuru 2021</p> </div> </body> </html>";
    const char *config_success="<body> <div class='t_row'> <div class='col-25 col-s-25'> </div> <div class='col-50 col-s-75'> <div class='header'>WiFi Sign Setup - Success</div> </div> <div class='col-25 col-s-100'> </div> </div> <form method=POST action='/save_config'> <div class='t_row'> <div class='col-25 col-s-25'> </div> <div class='col-50 col-s-75'> <div class='l_row'> Configuration saved! </div> </div> <div class='col-25 col-s-100'> </div> </div> </form> <div class='footer'> <p>Copyright CyberReefGuru 2021</p> </div> </body>";
    const char *config_failed="<body> <div class='t_row'> <div class='col-25 col-s-25'> </div> <div class='col-50 col-s-75'> <div class='header'>WiFi Sign Setup - Fail</div> </div> <div class='col-25 col-s-100'> </div> </div> <form method=POST action='/save_config'> <div class='t_row'> <div class='col-25 col-s-25'> </div> <div class='col-50 col-s-75'> <div class='l_row'> Configuration save failed! </div> </div> <div class='col-25 col-s-100'> </div> </div> </form> <div class='footer'> <p>Copyright CyberReefGuru 2021</p> </div> </body>";

};
