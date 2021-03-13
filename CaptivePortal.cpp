/*
 * CaptivePortal.cpp
 *
 *  Created on: Feb 3, 2021
 *      Author: tsasala
 */

#include "CaptivePortal.h"

IPAddress gwip(192, 168, 4, 1);       	// GW fixed IP adress
IPAddress apip(192, 168, 4, 100);     	// AP fixed IP adress
uint8_t apChannel = 2;					// AP wifi channel
String answerLine = ""; 				// make a String to hold incoming php-data from the client
String answerList1[32];                 // store of answerList SSID
String answerList2[32];                 // store of answerList answers
int answerCounter = 0;

int wifiStatus = WL_IDLE_STATUS;
WiFiServer server(80);

// Define DNS Server
DNSServer dnsServer;


CaptivePortal::CaptivePortal()
{
	currentStatus = HC_NONE;
	statusChange = 0;
	currentMethod = HTTP_GET;
}

void CaptivePortal::setup()
{
	//Initialize serial and wait for port to open:
	LOG_INFO(F("Access Point Web Server with CaptivePortal"));

	// check for the WiFi module:
	if (WiFi.status() == WL_NO_MODULE)
	{
		LOG_ERROR(F("Communication with WiFi module failed!"));
		Helper::error();
	}

	String fv = WiFi.firmwareVersion();
	if (fv < "1.0.0")
	{
		LOG_ERROR(F("Please upgrade the firmware"));
		Helper::error();
	}

	// by default the local IP address of will be 192.168.4.1
	LOG_INFO1(F("Creating access point named: "), DEFAULT_AP_NAME );

	WiFi.disconnect();

	wifiStatus = WiFi.beginAP(DEFAULT_AP_NAME, apChannel); // setup AP
	if (wifiStatus != WL_AP_LISTENING)
	{
		// If we couldn't connect to a WIFI network, and we can't create an AP
		// we have no options - so we lock up
		LOG_ERROR(F("Creating access point failed"));
		Helper::error();
	}

	// Set up DNS server
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(DNS_PORT, F("*"), WiFi.localIP());

	server.begin();

	// you're connected now, so print out the status
	printWiFiStatus();

	currentStatus = HC_NONE;

	LOG_INFO("DNS and Server started.");
}

PortalStatusCode CaptivePortal::loop()
{
	PortalStatusCode status = PortalStatusCode::FAILURE;

	// Process DNS request
    dnsServer.processNextRequest();
    status = handleClient();
    if( status == PortalStatusCode::CONFIG_SUCCESS)
    {
    	// close AP and break
    	dnsServer.stop();
    	server.flush();
    	LOG_DEBUG1( "Disconnected: ", WiFi.disconnect());
    }

    return status;

//	// compare the previous AP status to the current status
//	int t;
//	char c;
//	if (status != WiFi.status())
//	{
//		Serial.println("Status Changed.");
//
//		// it has changed update the variable
//		status = WiFi.status();
//		if (status == WL_AP_CONNECTED)
//		{
//			// a device has connected to the AP
//			Serial.println("Device connected to AP");
//		}
//		else
//		{
//			// a device has disconnected from the AP, and we are back in listening mode
//			Serial.println("Device disconnected from AP");
//		}
//	}
//
//	// Wifi Server check
//	client = server.available();   // listen for incoming clients
//	if (client)
//	{
//		Serial.println("new client");     // print a message out the serial port
//		String currentLine = ""; // make a String to hold incoming data from the client
//		while (client.connected())
//		{
//			// loop while the client's connected
//			if (client.available())
//			{
//				String req = client.readStringUntil('\r');
//				client.readStringUntil('\n');
//
//				Serial.print("Client Request (");
//				Serial.print(req.length());
//				Serial.print("): ");
//				Serial.println( req );
//
//				while(1)
//				{
//					req = client.readStringUntil('\r');
//					client.readStringUntil('\n');
//
//					Serial.print("Client Request (");
//					Serial.print(req.length());
//					Serial.print("): ");
//					Serial.println( req );
//
//					if (req.length() == 0 )
//					{
//						Serial.println("No more headers");
//						break; //no more headers
//					}
//				}
//
//				// parse request
//				doRoot();
//				break;
//
//				// if connected, wait for close
//
//			} // end loop client available
//
//		} // end loop client connected
//
//		// close the connection:
//		client.stop();
//		Serial.println("**client disconnected**");
//	}
}

PortalStatusCode CaptivePortal::handleClient()
{
	PortalStatusCode status = PortalStatusCode::FAILURE;

	// compare the previous AP status to the current status
	if (wifiStatus != WiFi.status())
	{
		LOG_DEBUG("Status Changed.");

		// it has changed update the variable
		wifiStatus = WiFi.status();
		if (wifiStatus == WL_AP_CONNECTED)
		{
			// a device has connected to the AP
			LOG_INFO("Device connected to AP");
		}
		else
		{
			// a device has disconnected from the AP, and we are back in listening mode
			LOG_INFO("Device disconnected from AP");
		}
	}

	if (currentStatus == HC_NONE)
	{
		WiFiClient client = server.available();
		if (!client)
		{
			// No client - simply return
			return PortalStatusCode::SUCCESS;
		}

		LOG_INFO(F("New Client"));
		currentClient = client;
		currentStatus = HC_WAIT_READ;
		statusChange = millis();
	}

	if (!currentClient.connected())
	{
		LOG_ERROR("ERROR - client not connected; reseting");
		currentClient = WiFiClient();
		currentStatus = HC_NONE;
		return PortalStatusCode::SUCCESS;
	}

	// Wait for data from client to become available
	if (currentStatus == HC_WAIT_READ)
	{
		if (!currentClient.available())
		{
			if (millis() - statusChange > HTTP_MAX_DATA_WAIT)
			{
				currentClient = WiFiClient();
				currentStatus = HC_NONE;
			}
			return PortalStatusCode::SUCCESS;
		}

		LOG_DEBUG(F("Parsing request..."));
		if (!parseRequest())
		{
			LOG_ERROR(F("Error Parsing Request"));
			currentClient = WiFiClient();
			currentStatus = HC_NONE;
			return PortalStatusCode::PARSE_ERROR;
		}
		else
		{
			LOG_DEBUG("Parsed request, sending response...");
		}

		//currentClient.setTimeout(HTTP_MAX_SEND_WAIT);
		//contentLength = CONTENT_LENGTH_NOT_SET;
		status = handleRequest();
		if(status == PortalStatusCode::CONFIG_SUCCESS)
		{
			LOG_INFO(F("Config Saved."));
			currentClient = WiFiClient();
			currentStatus = HC_NONE;
			return status;
		}

		if (!currentClient.connected())
		{
			LOG_INFO(F("Connection closed"));
			currentClient = WiFiClient();
			currentStatus = HC_NONE;
			return PortalStatusCode::SUCCESS;
		}
		else
		{
			currentStatus = HC_WAIT_CLOSE;
			statusChange = millis();
			return PortalStatusCode::SUCCESS;
		}
	}

	if (currentStatus == HC_WAIT_CLOSE)
	{
		if (millis() - statusChange > HTTP_MAX_CLOSE_WAIT)
		{
			currentClient = WiFiClient();
			currentStatus = HC_NONE;
			LOG_INFO(F("HTTP_MAX_CLOSE_WAIT Timeout"));
			currentClient.stop();
			LOG_INFO(F("Client stopped."));
		}
		else
		{
			return PortalStatusCode::SUCCESS;
		}
	}

	return PortalStatusCode::SUCCESS;
}



PortalStatusCode CaptivePortal::handleRequest()
{
	PortalStatusCode status = PortalStatusCode::FAILURE;

	if(currentUri == "/" || currentUri == "/hotspot-detect.html")
	{
		LOG_INFO2("Handle Root - '", currentUri, "'");
		status = handleRoot();
	}
	else if(currentUri == "/save_config")
	{
		LOG_INFO2("Handle Config Save - '", currentUri, "'");
		status = handleConfigSave();
	}
	else
	{
		LOG_ERROR1("Unhandled request", currentUri);
		send(404, "text/html", "Unhandled request");
		currentClient.stop();
	}

	return status;
}

PortalStatusCode CaptivePortal::handleConfigSave()
{
	LOG_DEBUG("handleConfigSave");

	PortalStatusCode status = PortalStatusCode::FAILURE;

	const char* response = config_failed;
	int s=0, contentSize=0;


	// Pull parameters

	// Save configuration

	// Set response
	status = PortalStatusCode::CONFIG_SUCCESS;

	if( status == PortalStatusCode::CONFIG_SUCCESS)
	{
		response = config_success;
	}

	contentSize = strlen(response);
	LOG_DEBUG1(F("Content Size: "), contentSize );

	LOG_DEBUG(F("Sending HTTP Start"));
	currentClient.print(F("HTTP/1.1 200 OK\r\n"));
	currentClient.print(F("Content-type:text/html\r\n"));
	currentClient.print(F("Connection: closed\r\n"));
	currentClient.print(F("Content-Length: "));
	currentClient.println( contentSize );
	currentClient.print("\r\n"); // break for content

	s = currentClient.print(response);
	LOG_DEBUG1(F("Sent to Client: "), s);

	// The HTTP response ends with another blank line:
	currentClient.println(); // break = end of content

	// break out of the while loop:
	//currentClient.flush();

	currentClient.stop();

	LOG_INFO1(F("Sent Root: "), contentSize);

	return status;
}


PortalStatusCode CaptivePortal::handleRoot()
{
	LOG_DEBUG("handleRoot");

	PortalStatusCode status = PortalStatusCode::FAILURE;

	int s=0, contentSize=0;
	uint32_t bodyBufSize = strlen(root_body)+32*11+6;

	char *temp = (char *)malloc(bodyBufSize);
	if( !temp)
	{
		LOG_ERROR("ERROR - could not allocate buffer");
		currentClient.stop();
		Helper::error();
	}
	else
	{
		memset(temp,0,bodyBufSize);
	}

	s = snprintf(temp, bodyBufSize-1, CaptivePortal::root_body, "hostname", WiFi.SSID(), "wpassword", "mqServer",1883, "mqUser", "mqPass", "mych", "regch", "respch", "allch", WiFi.localIP());
	if( s <= 0)
	{
		LOG_ERROR("ERROR - could create body content");
		currentClient.stop();
		Helper::error();
	}
	else
	{
		LOG_DEBUG1(F("Body Size: "), s);
	}

	contentSize = strlen(temp) + strlen( head );
	LOG_DEBUG1(F("Content Size: "), contentSize );

	LOG_DEBUG(F("Sending HTTP Start"));
	currentClient.print(F("HTTP/1.1 200 OK\r\n"));
	currentClient.print(F("Content-type:text/html\r\n"));
	currentClient.print(F("Connection: closed\r\n"));
	currentClient.print(F("Content-Length: "));
	currentClient.println( contentSize );
	currentClient.print("\r\n"); // break for content

	s = currentClient.print(head);
	LOG_DEBUG1(F("Sent to Client: "), s);
	s = currentClient.print(temp);
	LOG_DEBUG1(F("Sent to Client: "), s);

	// The HTTP response ends with another blank line:
	currentClient.println(); // break = end of content

	free(temp);

	// break out of the while loop:
	//currentClient.flush();

	currentClient.stop();

	LOG_INFO1(F("Sent Root: "), contentSize);

	return status;
}

void CaptivePortal::doRoot()
{
	LOG_DEBUG("doRoot");

	String response = "";
	int s=0;
	uint32_t bodyBufSize = strlen(root_body)+32*11+6;

	char *temp = (char *)malloc(bodyBufSize);
	if( !temp)
	{
		LOG_ERROR("ERROR - could not allocate buffer");
		currentClient.stop();
		Helper::error();
	}
	else
	{
		memset(temp,0,bodyBufSize);
	}

	s = snprintf(temp, bodyBufSize-1, CaptivePortal::root_body, "hostname", WiFi.SSID(), "wpassword", "mqServer",1883, "mqUser", "mqPass", "mych", "regch", "respch", "allch", WiFi.localIP());
	if( s <= 0)
	{
		LOG_ERROR("ERROR - could create body content");
		currentClient.stop();
		Helper::error();
	}
	else
	{
		LOG_DEBUG1(F("Body Size: "), s);
	}

	response.concat( head );
	response.concat( temp );

//	LOG_DEBUG1("Response: ", response);

	LOG_DEBUG1("Client Ready: ", currentClient.availableForWrite() );
	s = send(200, "text/html", response);

	LOG_DEBUG1(F("Sent Root: "), s)
	free(temp);
	LOG_DEBUG("freed buffer");


//	LOG_DEBUG1(F("Content Size: "), response.length() );
//
//	LOG_DEBUG(F("Sending HTTP Start"));
//	currentClient.println(F("HTTP/1.1 200 OK"));
//	currentClient.println(F("Content-type:text/html"));
//	currentClient.print(F("Content-Length: "));
//	currentClient.println( contentSize );
//	currentClient.println(); // break for content
//
//	s = currentClient.print(head);
//	contentSize -= s;
//	LOG_DEBUG3(F("Sent to Client: "), s, F(" Content Left to Send: "), contentSize);
//
//
//	LOG_DEBUG(F("Sending HTML Body"));
//	s = currentClient.print(temp);
//	contentSize -= s;
//	LOG_DEBUG3(F("Sent to Client: "), s, F(" Content Left to Send: "), contentSize);


	// The HTTP response ends with another blank line:
//	currentClient.println(); // break = end of content

	// break out of the while loop:
	//currentClient.flush();

	currentClient.stop();

	LOG_INFO("Sent root response");
}


boolean CaptivePortal::parseRequest()
{
	LOG_DEBUG("parseRequest: BEGIN");
	boolean status = false;

	currentParameterSize = 0;

	// Read first line of HTTP request -- this tells us the type of request
	String req = currentClient.readStringUntil('\r');
	currentClient.readStringUntil('\n');

	LOG_DEBUG3("Client Request (", req.length(), "): ",req );

	// First line should look like "GET /path HTTP/1.1"
	// Retrieve the "/path" part by finding the spaces
	int addr_start = req.indexOf(' ');
	int addr_end = req.indexOf(' ', addr_start + 1);
	if (addr_start == -1 || addr_end == -1)
	{
		LOG_ERROR("parseRequest: malformed request");
		return false;
	}

	String methodStr = req.substring(0, addr_start);
	String url = req.substring(addr_start + 1, addr_end);

	String searchStr = "";
	int hasSearch = url.indexOf('?');
	if (hasSearch != -1)
	{
		searchStr = urlDecode(url.substring(hasSearch + 1));
		currentUri = url.substring(0, hasSearch);
	}
	else
	{
		currentUri = url;
	}


	currentMethod = HTTP_GET;
	if (methodStr == "HEAD")
	{
		currentMethod = HTTP_HEAD;
	}
	else if (methodStr == "POST")
	{
		currentMethod = HTTP_POST;
	}
	else if (methodStr == "DELETE")
	{
		currentMethod = HTTP_DELETE;
	}
	else if (methodStr == "OPTIONS")
	{
		currentMethod = HTTP_OPTIONS;
	}
	else if (methodStr == "PUT")
	{
		currentMethod = HTTP_PUT;
	}
	else if (methodStr == "PATCH")
	{
		currentMethod = HTTP_PATCH;
	}

	LOG_DEBUG1("Method: ", methodStr);
	LOG_DEBUG2(F("URL: '"), currentUri, F("'") );

	String formData;

	// below is needed only when POST type request
	if (currentMethod == HTTP_POST || currentMethod == HTTP_PUT || currentMethod == HTTP_PATCH || currentMethod == HTTP_DELETE)
	{
		String boundaryStr;
		String headerName;
		String headerValue;

		boolean isForm = false;
		boolean isEncoded = false;
		uint32_t contentLength = 0;

		//parse headers
		while (1)
		{
			req = currentClient.readStringUntil('\r');
			currentClient.readStringUntil('\n');

			if (req == "")
			{
				break;	//no more headers
			}

			//LOG_DEBUG2("Response: '", req, "'");
			int headerDiv = req.indexOf(':');
			if (headerDiv == -1)
			{
				LOG_ERROR("Header missing content demarc of ':'");
				break;
			}

			headerName = req.substring(0, headerDiv);
			headerValue = req.substring(headerDiv + 1);
			headerValue.trim();
			//_collectHeader(headerName.c_str(), headerValue.c_str());

			LOG_DEBUG4(F("Header: '"), headerName, F("' - '"), headerValue, F("'"));

			if (headerName.equalsIgnoreCase(F("Content-Type")))
			{
				using namespace mime;
				if (headerValue.startsWith(mimeTable[txt].mimeType))
				{
					isForm = false;
				}
				else if (headerValue.startsWith(F("application/x-www-form-urlencoded")))
				{
					isForm = false;
					isEncoded = true;
				}
				else if (headerValue.startsWith(F("multipart/")))
				{
					boundaryStr = headerValue.substring(headerValue.indexOf('=') + 1);
					boundaryStr.replace("\"", "");
					isForm = true;
				}
			}
			else if (headerName.equalsIgnoreCase(F("Content-Length")))
			{
				contentLength = headerValue.toInt();
			}


		} // end while gathering headers

		LOG_DEBUG1("Encoded: ", isEncoded );
		LOG_DEBUG1("Form: ", isForm );
		if( isEncoded )
		{
			// form is URL encoded, let's break it up
		    String headerName;
			String headerValue;

			//parse headers
			while (1)
			{
				req = currentClient.readStringUntil('\r');
				currentClient.readStringUntil('\n');

				if (req == "")
					break; //no more headers

				int headerDiv = req.indexOf(':');

				if (headerDiv == -1)
				{
					break;
				}

				headerName = req.substring(0, headerDiv);
				headerValue = req.substring(headerDiv + 2);

				LOG_DEBUG1(F("headerName: "), headerName);
				LOG_DEBUG1(F("headerValue: "), headerValue);

				// Save current parameters in args array
				currentParameters[currentParameterSize].key = headerName;
				currentParameters[currentParameterSize++].value = headerValue;

			} // end while headers

			//_parseArguments (searchStr);

		}

		if (isForm)
		{
			// Parse URL arguments here
			//parseArguments (searchStr);

			// Parse form elements here
			if (!parseForm(boundaryStr, contentLength))
			{
				return false;
			}
		}
	}
	else
	{
		// GET
		String headerName;
		String headerValue;

		//parse headers
		while (1)
		{
			req = currentClient.readStringUntil('\r');
			currentClient.readStringUntil('\n');
			if (req == "")
			{
				break;	//no more headers
			}

			int headerDiv = req.indexOf(':');
			if (headerDiv == -1)
			{
				break;
			}

			headerName = req.substring(0, headerDiv);
			headerValue = req.substring(headerDiv + 2);



			//_collectHeader(headerName.c_str(), headerValue.c_str());
			LOG_DEBUG4(F("Header: '"), headerName, F("' - '"), headerValue, F("'"));
		}
		// Parse URL arguments here
	}

	currentClient.flush();

	LOG_DEBUG1(F("Request: "), currentUri);
	//LOG_DEBUG1(F("Arguments: "), searchStr);

	status = true;

	return status;

}


/**
 * This method parses an HTML form post where post attributes are
 * separated by boundaries
 */
bool CaptivePortal::parseForm(String boundary, uint32_t len)
{

	LOG_DEBUG1(F("Boundary: "), boundary);
	LOG_DEBUG1(F("Length: "), len);

	String line;
	int retry = 0;

	// Read blank lines
	do
	{
		line = currentClient.readStringUntil('\r');
		++retry;
	} while (line.length() == 0 && retry < 3);

	currentClient.readStringUntil('\n');

	//start reading the form
	if (line == ("--" + boundary))
	{
		while (1)
		{
			String argName;
			String argValue;
			String argType;
			String argFilename;

			bool argIsFile = false;

			line = currentClient.readStringUntil('\r');
			currentClient.readStringUntil('\n');

			if (line.startsWith("Content-Disposition"))
			{
				int nameStart = line.indexOf('=');
				if (nameStart != -1)
				{
					// First '=' is the first arg on the line
					// Second '=' is the file name if it exists
					argName = line.substring(nameStart + 2);
					nameStart = argName.indexOf('=');
					if (nameStart == -1)
					{
						// No file name
						argName = argName.substring(0, argName.length() - 1);
					}
					else
					{
						argFilename = argName.substring(nameStart + 2, argName.length() - 1);
						argName = argName.substring(0, argName.indexOf('"'));
						argIsFile = true;

						LOG_DEBUG1(F("PostArg FileName: "), argFilename);

						// we don't need to process files, so skip all that
					}
					LOG_DEBUG1(F("PostArg Name: "), argName);

					argType = "text/plain";
					line = currentClient.readStringUntil('\r');
					currentClient.readStringUntil('\n');

					if (line.startsWith("Content-Type"))
					{
						argType = line.substring(line.indexOf(':') + 2);
						//skip next line
						currentClient.readStringUntil('\r');
						currentClient.readStringUntil('\n');
					}

					LOG_DEBUG1(F("PostArg Type: "), argType);

					if (!argIsFile)
					{
						while (1)
						{
							line = currentClient.readStringUntil('\r');
							currentClient.readStringUntil('\n');

							if (line.startsWith("--" + boundary))
								break;

							if (argValue.length() > 0)
								argValue += "\n";

							argValue += line;
						}

						LOG_DEBUG1(F("PostArg Value: "), argValue);

						// Save current parameters in args array
						currentParameters[currentParameterSize].key = argName;
						currentParameters[currentParameterSize++].value = argValue;

						// Check if we are done with parameters
						if (line == ("--" + boundary + "--"))
						{
							LOG_DEBUG(F("Done Parsing Parameters"));
							break;
						}
					} // end not a while

				}// end startPos ! -1
				else
				{
					LOG_ERROR(F("Boundary Marker missing Content-Disposition"));
				}
			} // end starts with content-disposition

		} // end while

		return true;
	}

	LOG_DEBUG1(F("Error: line: "), line);

	return false;
}


int CaptivePortal::send(int statusCode, const char* content_type, const String& content)
{
	int sent = 0;

	LOG_DEBUG3(F("Sending HTTP Content - "), content_type, F(" - "), content.length() );
	  String response;
	  switch (statusCode)
	  {
	    case 200:
	      response = F("HTTP/1.1 200 OK\r\n");
	      break;
	    case 404:
	      response = F("HTTP/1.1 404 Not found\r\n");
	      break;
	    default:
	      response = F("HTTP/1.1 500 Internal server error\r\n");
	      break;
	  }

	  response.concat(F("Server: WiFiSign\r\nContent-Type: "));
	  response.concat(content_type);
	  response.concat(F("\r\nConnection: closed\r\nContent-Length: "));
	  response.concat(content.length());
	  response.concat(F("\r\n\r\n"));
	  response.concat(content);
	  if( currentClient )
	  {
		  LOG_DEBUG1( "Status: ", currentClient.status());

		  sent = currentClient.print(response);
	  }
	  else
	  {
		  LOG_ERROR("Client is null!")
	  }

	  LOG_DEBUG4(F("Sent "), sent, F(" bytes ("), currentClient.getWriteError(), F(")") );

	  return sent;
}



String CaptivePortal::urlDecode(const String& text)
{
  String decoded    = "";
  char temp[]       = "0x00";
  unsigned int len  = text.length();
  unsigned int i    = 0;

  while (i < len)
  {
    char decodedChar;
    char encodedChar = text.charAt(i++);

    if ((encodedChar == '%') && (i + 1 < len))
    {
      temp[2] = text.charAt(i++);
      temp[3] = text.charAt(i++);

      decodedChar = strtol(temp, NULL, 16);
    }
    else
    {
      if (encodedChar == '+')
      {
        decodedChar = ' ';
      }
      else
      {
        decodedChar = encodedChar;  // normal ascii char
      }
    }

    decoded += decodedChar;
  }

  return decoded;
}


//String CaptivePortal::createResponse(uint16_t statusCode, const String &body)
//{
//  String response;
//  switch (statusCode)
//  {
//    case 200:
//      response = "HTTP/1.1 200 OK\r\n";
//      break;
//    case 404:
//      response = "HTTP/1.1 404 Not found\r\n";
//      break;
//    default:
//      response = "HTTP/1.1 500 Internal server error\r\n";
//      break;
//  }
//
//  response +=
//    "Server: WiFiSign\r\n"
//    "Content-Type: text/html\r\n"
//    "Connection: closed\r\n"
//    "Content-Length: ";
//  response.concat(body.length());
//  response += "\r\n"
//              "\r\n";
//  response += body;
//  return response;
//}


void CaptivePortal::printWiFiStatus()
{
	LOG_INFO1(F("Nina W10 firmware: "), WiFi.firmwareVersion());

	// print the SSID of the network you're attached to:
	LOG_INFO1(F("SSID: "), WiFi.SSID());

	// print your WiFi shield's IP address:
	LOG_INFO1(F("IP Address  : "), WiFi.localIP());
	// print your WiFi shield's gateway:

	LOG_INFO1(F("IP Gateway  : "), WiFi.gatewayIP());

	// print your WiFi shield's gateway:
	LOG_INFO1(F("Subnet Mask : "), WiFi.subnetMask() );

	// print where to go in a browser:
	LOG_INFO1(F("To see this page in action, open a browser to http://"), WiFi.localIP() );
}

