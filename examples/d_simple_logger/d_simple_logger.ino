/**
 * @example{lineno} d_simple_logger.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Kevin M.Smith <SDI12@ethosengineering.org>
 * @date August 2013
 *
 * @brief Example D: Check all Addresses for Active Sensors and Log Data
 *
 * This is a simple demonstration of the SDI-12 library for Arduino.
 *
 * It discovers the address of all sensors active on a single bus and takes measurements
 * from them.
 *
 * Every SDI-12 device is different in the time it takes to take a
 * measurement, and the amount of data it returns. This sketch will not serve every
 * sensor type, but it will likely be helpful in getting you started.
 *
 * Each sensor should have a unique address already - if not, multiple sensors may
 * respond simultaneously to the same request and the output will not be readable by the
 * Arduino.
 *
 * To address a sensor, please see Example B: b_address_change.ino
 */

#include <SDI12.h>

#ifndef SDI12_DATA_PIN
#define SDI12_DATA_PIN 7
#endif
#ifndef SDI12_POWER_PIN
#define SDI12_POWER_PIN 22
#endif

/* connection information */
uint32_t serialBaud   = 115200;         /*!< The baud rate for the output serial port */
int8_t   dataPin      = SDI12_DATA_PIN; /*!< The pin of the SDI-12 data bus */
int8_t   powerPin     = SDI12_POWER_PIN; /*!< The sensor power pin (or -1) */
uint32_t wake_delay   = 0; /*!< Extra time needed for the sensor to wake (0-100ms) */
int8_t   firstAddress = 0; /* The first address in the address space to check (0='0') */
int8_t   lastAddress = 61; /* The last address in the address space to check (61='z') */
bool     printIO     = false;

/** Define the SDI-12 bus */
SDI12 mySDI12(dataPin);

// keeps track of active addresses
bool isActive[64];

uint8_t numSensors = 0;

/**
 * @brief converts allowable address characters ('0'-'9', 'a'-'z', 'A'-'Z') to a
 * decimal number between 0 and 61 (inclusive) to cover the 62 possible
 * addresses.
 */
byte charToDec(char i) {
  if ((i >= '0') && (i <= '9')) return i - '0';
  if ((i >= 'a') && (i <= 'z')) return i - 'a' + 10;
  if ((i >= 'A') && (i <= 'Z'))
    return i - 'A' + 36;
  else
    return i;
}

/**
 * @brief maps a decimal number between 0 and 61 (inclusive) to allowable
 * address characters '0'-'9', 'a'-'z', 'A'-'Z',
 *
 * THIS METHOD IS UNUSED IN THIS EXAMPLE, BUT IT MAY BE HELPFUL.
 */
char decToChar(byte i) {
  if (i < 10) return i + '0';
  if ((i >= 10) && (i < 36)) return i + 'a' - 10;
  if ((i >= 36) && (i <= 62))
    return i + 'A' - 36;
  else
    return i;
}

/**
 * @brief gets identification information from a sensor, and prints it to the serial
 * port
 *
 * @param i a character between '0'-'9', 'a'-'z', or 'A'-'Z'.
 */
void printInfo(char i) {
  String command = "";
  command += (char)i;
  command += "I!";
  mySDI12.sendCommand(command, wake_delay);
  delay(30);

  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  // allccccccccmmmmmmvvvxxx...xx<CR><LF>
  Serial.print(sdiResponse.substring(0, 1));  // address
  Serial.print(", ");
  Serial.print(sdiResponse.substring(1, 3).toFloat() / 10);  // SDI-12 version number
  Serial.print(", ");
  Serial.print(sdiResponse.substring(3, 11));  // vendor id
  Serial.print(", ");
  Serial.print(sdiResponse.substring(11, 17));  // sensor model
  Serial.print(", ");
  Serial.print(sdiResponse.substring(17, 20));  // sensor version
  Serial.print(", ");
  Serial.print(sdiResponse.substring(20));  // sensor id
  Serial.print(", ");
}

bool getResults(char address, int resultsExpected) {
  uint8_t resultsReceived = 0;
  uint8_t cmd_number      = 0;
  uint8_t cmd_retries     = 0;

  // When requesting data, the sensor sends back up to ~80 characters at a
  // time to each data request.  If it needs to return more results than can
  // fit in the first data request (D0), we need to make additional requests
  // (D1-9).  Since this is a parent to all sensors, we're going to keep
  // requesting data until we either get as many results as we expect or no
  // more data is returned.
  while (resultsReceived < resultsExpected && cmd_number <= 9 && cmd_retries < 5) {
    bool    gotResults  = false;
    uint8_t cmd_results = 0;
    // Assemble the command based on how many commands we've already sent,
    // starting with D0 and ending with D9
    // SDI-12 command to get data [address][D][dataOption][!]
    mySDI12.clearBuffer();
    String command = "";
    command += address;
    command += "D";
    command += cmd_number;
    command += "!";
    mySDI12.sendCommand(command, wake_delay);
    delay(30);
    if (printIO) {
      Serial.print(">>>");
      Serial.println(command);
    }

    // Wait for the first few characters to arrive.  The response from a data
    // request should always have more than three characters
    uint32_t start = millis();
    while (mySDI12.available() < 3 && (millis() - start) < 1500) {}

    // read the returned address to remove it from the buffer
    char returnedAddress = mySDI12.read();
    if (printIO) {
      if (returnedAddress != address) {
        Serial.println("Wrong address returned!");
        Serial.print("Expected ");
        Serial.print(String(address));
        Serial.print(" Got ");
        Serial.println(String(returnedAddress));
      }
      Serial.print("<<<");
      Serial.write(returnedAddress);
      Serial.print(", ");
      Serial.println();
    }

    bool bad_read = false;
    // While there is any data left in the buffer
    while (mySDI12.available() && (millis() - start) < 3000) {
      char c = mySDI12.peek();
      // if there's a polarity sign, a number, or a decimal next in the
      // buffer, start reading it as a float.
      if (c == '-' || c == '+' || (c >= '0' && c <= '9') || c == '.') {
        float result = mySDI12.parseFloat();
        if (printIO) {
          Serial.print("<<<");
          Serial.println(String(result, 7));
        } else {
          Serial.print(String(result, 7));
          Serial.print(", ");
        }
        if (result != -9999) {
          gotResults = true;
          cmd_results++;
        }
        // if we get to a new line, we've made it to the end of the response
      } else if (c == '\r' || c == '\n') {
        if (printIO) { Serial.write(c); }
        mySDI12.read();
      } else {
        if (printIO) {
          Serial.print(F("<<< INVALID CHARACTER IN RESPONSE:"));
          Serial.write(c);
          Serial.println();
        }
        // Read the character to make sure it's removed from the buffer
        mySDI12.read();
        bad_read = true;
      }
      delay(10);  // 1 character ~ 7.5ms
    }
    if (!gotResults) {
      if (printIO) {
        Serial.println(F("  No results received, will not continue requests!"));
        break;  // don't do another loop if we got nothing
      }
    }
    if (gotResults && !bad_read) {
      resultsReceived = resultsReceived + cmd_results;
      if (printIO) {
        Serial.print(F("  Total Results Received: "));
        Serial.print(resultsReceived);
        Serial.print(F(", Remaining: "));
        Serial.println(resultsExpected - resultsReceived);
      }
      cmd_number++;
    } else {
      // if we got a bad charater in the response, add one to the retry
      // attempts but do not bump up the command number or transfer any
      // results because we want to retry the same data command to try get
      // a valid response
      cmd_retries++;
    }
    mySDI12.clearBuffer();
  }

  return resultsReceived == resultsExpected;
}

bool takeMeasurement(char i, String meas_type = "") {
  mySDI12.clearBuffer();
  String command = "";
  command += i;
  command += "M";
  command += meas_type;
  command += "!";  // SDI-12 measurement command format  [address]['M'][!]
  mySDI12.sendCommand(command, wake_delay);
  delay(30);

  if (printIO) {
    Serial.print(">>>");
    Serial.println(command);
  }

  // wait for acknowledgement with format [address][ttt (3 char, seconds)][number of
  // measurements available, 0-9]
  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  if (printIO) {
    Serial.print("<<<");
    Serial.println(sdiResponse);
  }

  String returnedAddress = sdiResponse.substring(0, 1);
  Serial.print(returnedAddress);
  Serial.print(", ");

  // find out how long we have to wait (in seconds).
  uint8_t meas_time_s = sdiResponse.substring(1, 4).toInt();
  Serial.print(meas_time_s);
  Serial.print(", ");

  // Set up the number of results to expect
  int numResults = sdiResponse.substring(4).toInt();
  Serial.print(numResults);
  Serial.print(", ");

  if (printIO) { Serial.println(); }

  unsigned long timerStart = millis();
  while ((millis() - timerStart) < (1000UL * (meas_time_s + 1))) {
    if (mySDI12.available())  // sensor can interrupt us to let us know it is done early
    {
      if (printIO) {
        Serial.print("<<<");
        Serial.write(mySDI12.read());
        Serial.print(" (");
      }
      Serial.print(millis() - timerStart);
      if (printIO) {
        Serial.println(")");
      } else {
        Serial.print(", ");
      }
      mySDI12.clearBuffer();
      break;
    }
  }
  // Wait for anything else and clear it out
  delay(30);
  mySDI12.clearBuffer();

  if (numResults > 0) { return getResults(i, numResults); }

  return true;
}

// this checks for activity at a particular address
// expects a char, '0'-'9', 'a'-'z', or 'A'-'Z'
bool checkActive(char i) {
  String myCommand = "";
  myCommand        = "";
  myCommand += (char)i;  // sends basic 'acknowledge' command [address][!]
  myCommand += "!";

  for (int j = 0; j < 3; j++) {  // goes through three rapid contact attempts
    mySDI12.sendCommand(myCommand, wake_delay);
    if (printIO) {
      Serial.print(">>>");
      Serial.println(myCommand);
    }
    delay(30);
    if (mySDI12.available()) {  // If we hear anything, assume we have an active sensor
      if (printIO) {
        Serial.print("<<<");
        while (mySDI12.available()) { Serial.write(mySDI12.read()); }
      } else {
        mySDI12.clearBuffer();
      }
      return true;
    }
  }
  mySDI12.clearBuffer();
  return false;
}

void setup() {
  Serial.begin(serialBaud);
  while (!Serial && millis() < 10000L);

  Serial.println("Opening SDI-12 bus...");
  mySDI12.begin();
  delay(500);  // allow things to settle

  Serial.println("Timeout value: ");
  Serial.println(mySDI12.TIMEOUT);

  // Power the sensors;
  if (powerPin >= 0) {
    Serial.println("Powering up sensors, wait...");
    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    delay(5000L);
  }

  // Quickly scan the address space
  Serial.println("Scanning all addresses, please wait...");
  Serial.println("Sensor Address, Protocol Version, Sensor Vendor, Sensor Model, "
                 "Sensor Version, Sensor ID");

  for (int8_t i = firstAddress; i <= lastAddress; i++) {
    char addr = decToChar(i);
    if (checkActive(addr)) {
      numSensors++;
      isActive[i] = 1;
      printInfo(addr);
      Serial.println();
    }
  }
  Serial.print("Total number of sensors found:  ");
  Serial.println(numSensors);

  if (numSensors == 0) {
    Serial.println(
      "No sensors found, please check connections and restart the Arduino.");
    while (true) { delay(10); }  // do nothing forever
  }

  Serial.println();
  Serial.println("Time Elapsed (s), Measurement Type, Sensor Address, Est Measurement "
                 "Time (s), Number Measurements, "
                 "Real Measurement Time (ms), Measurement 1, Measurement 2, ... etc.");
  Serial.println("-------------------------------------------------------------------"
                 "------------");
}

void loop() {
  String commands[] = {"", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
  for (uint8_t a = 0; a < 1; a++) {
    // measure one at a time
    for (int8_t i = firstAddress; i <= lastAddress; i++) {
      char addr = decToChar(i);
      if (isActive[i]) {
        if (printIO) {
          Serial.print(addr);
          Serial.print(" - millis: ");
          Serial.print(millis());
          Serial.println();
        } else {
          Serial.print(millis());
          Serial.print(", ");
        }
        takeMeasurement(addr, commands[a]);
        Serial.println();
      }
    }
  }

  delay(10000L);  // wait ten seconds between measurement attempts.
}
