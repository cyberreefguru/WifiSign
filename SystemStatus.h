/*
 * SystemStatus.h
 *
 *  Created on: Mar 20, 2021
 *      Author: tsasala
 */

#pragma once

enum class SystemStatus {
	UNKNOWN, OK, ERROR,
	BOOTING, WAITING, COMMAND_AVAILABLE,
	PROCESSING, CONFIGURING,
	WIFI_ERROR, WIFI_RECONNECTION,
	MQTT_ERROR, MQTT_RECONNECTION,
	MATRIX_ERROR
};
