#include "SoftwareSerial.h"

/*
 * Constants
 */
const int OL_PIN = 3;
const int IL_PIN = 8;
const int button3Pin = 7;  // Change to your specific pin number


#define RECONFIGURE_HM10 false

const unsigned long SERIAL_BAUD = 115200;
const unsigned long HM10_BAUD = 115200;  // 9600 for first HM-10 configuration

#if RECONFIGURE_HM10
const String HM10_NAME = "13-HM10-IDO";  // TODO: change HM-10 ID XX-HM10-IDO
const String HM10_SERVICE_UUID = "FF13";
// TODO: change HM-10 ID FFXX

#endif

/*
 * Variables
 */
int button3State = HIGH;             // Current state of the button
int lastButton3State = HIGH;         // Previous state of the button
unsigned long lastDebounceTime = 0;  // Last time the button state changed
unsigned long debounceDelay = 50;

// SoftwareSerial to connect to any unreserved digital ports (avoid Serial ports)
SoftwareSerial HM10Serial(4, 5);  // (HM-10 TX -> 4 RX), (HM-10 RX -> 5 TX)

// Arduino Mega 2560 has reserved HardwareSerial ports
// Arduino Uno (Keyestudio PLUS) only have Serial (UART0) shared with USB connection
// https://www.arduino.cc/reference/en/language/functions/communication/serial/
//
// Serial0 (UART0) :  0 (TX),  1 (RX) replaces connection to USB serial
// Serial1 (UART1) : 18 (TX), 19 (RX)
// Serial2 (UART2) : 16 (TX), 17 (RX)
// Serial3 (UART3) : 14 (TX), 15 (RX)
//
// #define HM10Serial Serial1 // (18 TX -> HM-10 RX), (19 RX -> HM-10 TX)

/*
 * Definitions
 */
#if RECONFIGURE_HM10
void sendATCommand(const Stream &stream, const String &command);
void reconfigureHM10();
#endif


void turnLight(bool status, char location) {

  if (location == 'o') {
    if (status) {
      digitalWrite(OL_PIN, HIGH);
    } else {
      digitalWrite(OL_PIN, LOW);
    }
  }

  if (location == 'i') {
    if (status) {
      digitalWrite(IL_PIN, HIGH);
    } else {
      digitalWrite(IL_PIN, LOW);
    }
  }
}





/*
 * Functions
 */
void setup() {
  pinMode(OL_PIN, OUTPUT);
  pinMode(IL_PIN, OUTPUT);
  pinMode(button3Pin, INPUT_PULLUP);

  Serial.begin(SERIAL_BAUD);

  while (!Serial)
    ;

  HM10Serial.begin(HM10_BAUD);
  Serial.println(F("HM10 STARTED"));

#if RECONFIGURE_HM10
  reconfigureHM10();
  Serial.println(F("HM10 RECONFIGURED"));
#endif
}

void loop() {
  int reading = digitalRead(button3Pin);

  // Check if the button state has changed
  if (reading != lastButton3State) {
    lastDebounceTime = millis();  // Reset the debounce timer
  }

  // If the time since the last state change is greater than the debounce delay
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // If the button state has changed (from HIGH to LOW or LOW to HIGH)
    if (reading != button3State) {
      button3State = reading;  // Update the button state

      // Check if the button is pressed
      if (button3State == LOW) {
        HM10Serial.println("ols");
      }
    }
  }

  lastButton3State = reading;  // Save the

  // Print data received from HM10 to Serial for debugging
  if (HM10Serial.available()) {
    String hm10Data = HM10Serial.readStringUntil('\n');

    // TODO: OK+LOSTOK+CONN

    Serial.print(F("HM-10 received : "));
    Serial.println(hm10Data);
    if (hm10Data == "olf") {
      turnLight(false, 'o');
    } else if (hm10Data == "olt") {
      turnLight(true, 'o');
    } else if (hm10Data == "ild") {
      turnLight(false, 'i');
    } else if (hm10Data == "ilu") {
      turnLight(true, 'i');
    }
  }

  // Send Serial inputs to HM10 to enable sending debug commands
  if (Serial.available()) {
    String usbData = Serial.readStringUntil('\n');
    Serial.print(F("HM-10 sending : "));
    Serial.println(usbData);
    HM10Serial.print(usbData);
  }
}

#if RECONFIGURE_HM10
void reconfigureHM10() {
  sendATCommand(HM10Serial, "AT");
  sendATCommand(HM10Serial, "AT+ADDR?");
  sendATCommand(HM10Serial, "AT+NAME?");
  sendATCommand(HM10Serial, "AT+NAME" + HM10_NAME);  // XX-HM10-IDO
  sendATCommand(HM10Serial, "AT+BAUD?");
  sendATCommand(HM10Serial, "AT+BAUD4");  // 115200 bauds, 9600 by default
  sendATCommand(HM10Serial, "AT+UUID?");
  sendATCommand(HM10Serial, "AT+UUID0x" + HM10_SERVICE_UUID);  // FFXX
}

void sendATCommand(const Stream &atStream, const String &command) {
  Serial.print(F("SEND AT COMMAND : "));
  Serial.println(command);

  atStream.print(command);

  // Wait for the response
  while (!atStream.available())
    ;

  // Print the response
  String response = atStream.readStringUntil('\n');
  Serial.println(response);
}

#endif