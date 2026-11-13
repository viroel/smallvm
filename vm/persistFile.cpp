/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

// persistFile.cpp - Persistent file/non-volatile storage operations

#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "interp.h"
#include "persist.h"

#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP32) || defined(RP2040_PHILHOWER) || defined(ARDUINO_WEACT)

#ifdef ARDUINO_WEACT
#include "sdCardPrims.h"
static FsFile codeFile;
#else
#include "fileSys.h"
static File codeFile;
#endif

#define FILE_NAME "/ublockscode"

int fileSystemInitialized = false;

static void closeAndOpenCodeFile() {
	if (codeFile) codeFile.close();
#ifdef ARDUINO_WEACT
	// Use SD persistence
	codeFile = SD.open(FILE_NAME, O_RDWR | O_CREAT);
	codeFile.seekEnd();
#else
	codeFile = myFS.open(FILE_NAME, "a+");
	codeFile.seek(0, SeekEnd);
#endif
}

extern "C" void initFileSystem() {
	// Initialize the file system.

	if (fileSystemInitialized) return;

	#if defined(ARDUINO_ARCH_ESP32)
		myFS.begin(true);
	#elif defined(ARDUINO_WEACT)
		initSDCard(-1);
	#else
		myFS.begin();
	#endif
	#ifdef ARDUINO_WEACT
	fileSystemInitialized = sdCardInitialized();
	#else
	fileSystemInitialized = true;
	#endif
}

extern "C" int initCodeFile(uint8 *flash, int flashByteCount) {
	// Called at startup on boards that store code in the file system.
	// Initialize the file system, read the code file, and return

	initFileSystem();
	#ifdef ARDUINO_WEACT
	codeFile = SD.open(FILE_NAME, O_RDONLY);
	if (!codeFile) clearCodeFile(0);
	if (codeFile) codeFile.seekSet(0);
	// read code file into simulated Flash:
	int bytesRead = codeFile.read((char*) flash, flashByteCount);
	#else
	codeFile = myFS.open(FILE_NAME, "r");
	if (!codeFile) clearCodeFile(0);
	// read code file into simulated Flash:
	int bytesRead = codeFile.readBytes((char*) flash, flashByteCount);
	#endif
	closeAndOpenCodeFile();
	return bytesRead;
}

extern "C" void writeCodeFile(uint8 *code, int byteCount) {
	if (codeFile) codeFile.write(code, byteCount);
	closeAndOpenCodeFile();
}

extern "C" void writeCodeFileWord(int word) {
	if (codeFile) codeFile.write((uint8 *) &word, 4);
}

extern "C" void clearCodeFile(int cycleCount) {
	if (codeFile) codeFile.close();
	#ifdef ARDUINO_WEACT
	SD.remove("ublockscode"); // just in case
	codeFile = SD.open(FILE_NAME, O_RDWR | O_CREAT | O_TRUNC); // truncate file to zero length
	#else
	codeFile = myFS.open(FILE_NAME, "w"); // truncate file to zero length
	#endif
	int headerWord = ('S' << 24) | cycleCount; // Header record, version 1
	writeCodeFileWord(headerWord);
	closeAndOpenCodeFile();
}

// File operations for storing system state

extern "C" void createFile(const char *fileName) {
	#ifdef ARDUINO_WEACT
	FsFile file = SD.open(fileName, O_RDWR | O_CREAT | O_TRUNC);
	#else
	File file = myFS.open(fileName, "w");
	#endif
	file.close();
}

extern "C" void deleteFile(const char *fileName) {
	if (fileExists(fileName)) {
		#ifdef ARDUINO_WEACT
		SD.remove(fileName);
		#else
		myFS.remove(fileName);
		#endif
	}
}

extern "C" int fileExists(const char *fileName) {
	#ifdef ARDUINO_WEACT
	return SD.exists(fileName);
	#else
	File file = myFS.open(fileName, "r");
	if (!file) return false;
	file.close();
	#endif
	return true;
}

// Code snapshots

extern "C" int hasStartupSnapshot() {
	initFileSystem();
	return fileExists("/startup.ucode");
}

extern "C" void snapshotCodeToFile(char *fileName, int fileNameBytes) {
	char filePath[64]; // local buffer for full file name
	if (fileNameBytes > 62) return; // file name is too long

	// prefix file name with "/" and add null termionator
	// Note: fileName is not null terminated.
	filePath[0] = '/';
	memcpy(&filePath[1], fileName, fileNameBytes);
	filePath[fileNameBytes + 1] = 0; // null terminator

	int codeStoreByteCount = 0;
	uint8_t *codeStoreData = getCodeStore(&codeStoreByteCount);
	if (codeStoreByteCount < 0) return; // code snapshot not supported

	initFileSystem();
	#ifdef ARDUINO_WEACT
	FsFile file = SD.open(filePath, O_RDWR | O_CREAT | O_TRUNC);
	#else
	File file = myFS.open(filePath, "w");
	#endif
	file.write(codeStoreData, codeStoreByteCount);
	file.close();
}

static int isCodeSnapshot(File *file) {
	// Check the integrity of the given code snapshot file. Return true if okay.

	uint8 header[8];

	#ifdef ARDUINO_WEACT
	int fileSize = file->fileSize();
	#else
	int fileSize = file->size();
	#endif
	if (fileSize >= codeStoreSize()) return false; // file too large
	if ((fileSize % 4) != 0) return false; // file size is not a multiple of four bytes

	#ifdef ARDUINO_WEACT
	file->seekSet(0);
	#else
	file->seek(0, SeekSet); // read from start of file
	#endif
	while (file->available()) {
		file->read(header, 8);
		int recWords = (header[7] << 24) | (header[6] << 16) | (header[5] << 8) | header[4];

		// check the record tag byte
		if ('R' != header[3]) {
			outputString("Bad code snapshot");
			return false;
		}

		// check the record type byte
		switch(header[2]) {
		case 10:
		case 11:
		case 12:
		case 19:
		case 21:
		case 29:
		case 218:
			break; // okay; these are known record types from persist.h RecordType_t
		default:
			outputString("Bad chunk type in code snapshot");
			return false;
		}

		// check the record length
		int bytesToRead = 4 * recWords;
		if (file->available() < bytesToRead) {
			outputString("Incomplete code snapshot");
			return false;
		}

		// skp to the next record
		#ifdef ARDUINO_WEACT
		file->seekCur(4 * recWords);
		#else
		file->seek(4 * recWords, SeekCur);
		#endif
	}
	return true;
}

extern "C" void loadCodeSnapshot(char *fileName) {
	#if !defined(USE_CODE_FILE)

		char fullFileName[64]; // local buffer for full file name
		if (strlen(fileName) > 62) return; // file name is too long

		// prefix fullFileName with "/" and add null termionator
		fullFileName[0] = 0;
		strncat(fullFileName, "/", sizeof(fullFileName) - 1);
		strncat(fullFileName, fileName, sizeof(fullFileName) - 1);

		initFileSystem();
		#ifdef ARDUINO_WEACT
		FsFile file = SD.open(fullFileName, O_RDONLY);
		#else
		File file = myFS.open(fullFileName, "r");
		#endif
		if (!isCodeSnapshot(&file)) {
			outputString("Bad code snapshot");
			outputString(fullFileName);
			return;
		}

		// clear code store
		clearPersistentMemory();
		#if defined(ESP8266) || defined(ARDUINO_WEACT)
			clearCodeFile(0);
		#endif

		// copy the file into the code store
		uint8 buffer[1024]; // must be a multiple of four
		#ifdef ARDUINO_WEACT
		file.seekSet(0); // read from start of file
		#else
		file.seek(0, SeekSet); // read from start of file
		#endif
		while (file.available()) {
			int byteCount = file.read(buffer, sizeof(buffer));
			appendToCodeStore(buffer, byteCount);
		}

		// install scripts and start running
		restoreScripts();
		startAll();
	#endif
}

#else // stubs for boards without file systems

extern "C" int hasStartupSnapshot() { return false; }
extern "C" void loadCodeSnapshot(char *fileName) { } // do nothing

#endif
