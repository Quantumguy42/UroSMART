#ifndef TESTBENCH_H
#define TESTBENCH_H

#include <Arduino.h>

bool readLine(String &out);

void waitForCmd(const String target = "");

// Function to check if a string is a valid integer
bool isValidInteger(const String &s);

// Function to check if a string is a valid float
bool isValidFloat(const String &s);

// Function to read a valid integer from Serial
int readIntFromSerial(const String &prompt = "Enter an integer: ");

// Function to read a valid float from Serial
float readFloatFromSerial(const String &prompt = "Enter a number:");


#endif 