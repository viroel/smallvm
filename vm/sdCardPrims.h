/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2025 John Maloney, Bernat Romagosa, and Jens Mönig

// sdCardPrims.h - SD Card file system.
// John Maloney, July 2025

#ifdef ARDUINO_WEACT
#include "Arduino.h"
#include "SdFat.h"

extern SdFs SD;
void initSDCard(int chipSelectPin);
bool sdCardInitialized();
#endif

