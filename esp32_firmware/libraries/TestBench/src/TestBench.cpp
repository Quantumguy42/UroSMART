#include "TestBench.h"

bool readLine(String &out) {
  Serial.flush();
  String inputBuffer = "";
  while (true) {
    if (Serial.available()){
      char c = Serial.read();
      Serial.print(c);


      if (c == '\n') {
        inputBuffer.trim();
        out = inputBuffer;
        inputBuffer = "";
        return true;   // full line received
      }

      inputBuffer += c;
    }
  }
  return false;
}


void waitForCmd(const String target) {
  String cmd;
  Serial.flush();
  while (true) {
    
    readLine(cmd);

    if (target == "") return;
    if (cmd == target) return;

    Serial.println("Invalid command, waiting for \"" + target + "\"");
    
  }
}

// Function to check if a string is a valid integer
bool isValidInteger(const String &s) {
  if (s.length() == 0) return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (i == 0 && (c == '-' || c == '+')) continue; // allow sign
    if (!isDigit(c)) return false;
  }
  return true;
}

// Function to check if a string is a valid float
bool isValidFloat(const String &s) {
  if (s.length() == 0) return false;
  bool decimalPointSeen = false;
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (i == 0 && (c == '-' || c == '+')) continue; // allow sign
    if (c == '.') {
      if (decimalPointSeen) return false; // multiple decimals not allowed
      decimalPointSeen = true;
      continue;
    }
    if (!isDigit(c)) return false;
  }
  return true;
}


// Function to read a valid integer from Serial
int readIntFromSerial(const String &prompt) {
  Serial.flush();
  while (true) {
    Serial.print(prompt);
    while (Serial.available() == 0) {} // wait for input
    String input;
    readLine(input);
    input.trim();

    if (isValidInteger(input)) {
      return input.toInt();
    } else {
      Serial.println("Invalid input. Please enter a valid integer.");
    }
  }
}

// Function to read a valid float from Serial
float readFloatFromSerial(const String &prompt) {
  while (true) {
    Serial.println(prompt);
    while (Serial.available() == 0) {}

    String input = Serial.readStringUntil('\n');
    input.trim();

    if (isValidFloat(input)) {
      return input.toFloat();
    } else {
      Serial.println("Invalid input. Please enter a valid number.");
    }
  }
}
