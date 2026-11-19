/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#pragma once

#include "board/board.h"

#include <platform/CHIPDeviceLayer.h>

struct Identify;

class AppTask
{
public:
	static AppTask &Instance()
	{
		static AppTask sAppTask;
		return sAppTask;
	};

	CHIP_ERROR StartApp();

	void UpdateMeasurement();

	int16_t GetCurrentTemperature() const { return mCurrentTemperature; }
	uint16_t GetCurrentHumidity() const { return mCurrentHumidity; }

private:
	CHIP_ERROR Init();
	k_timer mTimer;

	static constexpr uint16_t kMeasurementIntervalMs = 10000; /* 10 seconds */

	static void UpdateMeasurementTimeoutCallback(k_timer *timer);

	int16_t mTemperatureSensorMaxValue = 0;
	int16_t mTemperatureSensorMinValue = 0;
	int16_t mCurrentTemperature = 0;

	uint16_t mHumiditySensorMaxValue = 0;
	uint16_t mHumiditySensorMinValue = 0;
	uint16_t mCurrentHumidity = 0;

	void UpdateTemperature();
	void UpdateHumidity();
};
