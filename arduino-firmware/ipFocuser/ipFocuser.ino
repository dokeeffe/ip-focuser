/**
 * Arduino firmware for a motorised telescope focuser which provides an HTTP interface. 
 * HTTP requests to this device are blocking and will not respond until motor movement is complete so take care with your client side timeouts.
 * 
 * Based on the ethercard library by Jean-Claude Wippler (https://github.com/jcw/ethercard) You will need to install this in your arduino IDE to flash this firmware. See instrunctions in the ethercard github project page.
 *
 * PARTS:
 * 1 x Arduino nano (http://www.dx.com/p/arduino-nano-v3-0-81877#.ViKE2RCrRE4)
 * 1 x PCB ENC28J60 Ethernet Module HanRun HR911105A (14/10) (http://www.dx.com/p/pcb-arduino-enc28j60-ethernet-module-blue-140971#.ViKE_xCrRE4)
 * 1 x TB6560 3A Single-Axis Controller Stepper Motor Driver(http://www.dx.com/p/tb6560-3a-single-axis-controller-stepper-motor-driver-board-green-black-red-217142#.ViKE8hCrRE4)
 * 1 x Old uniplar stepper motor that I found in the shed.
 *
 * Wiring:
 *     Arduino -- Ethernet module:
 *      Pin 8 -- CS
 *      Pin 11 -- ST
 *      Pin 12 -- SO
 *      Pin 13 -- SCK
 *      5v -- 5v
 *      GND -- GND
 *     Arduino -- Stepper Driver:
 *      Pin 6 -- CW+
 *      GND -- CW-
 *      Pin 7 -- CLK+
 *      GND -- CLK-
 *     Stepper Driver -- 12V supply:
 *      12v+ -- 12v PSU +
 *      gnd -- 12v PSU -
 *     Stepper Driver -- Motor:
 *      A+ --  coil a
 *      A- --  coil a
 *      B+ --  coil b
 *      B- --  coil b
 *      12v+ -- NA (common from motor not connected, was running very hot when stopped for some reason when connected)
 *
 *
 *  Example curl requests:
 *    Query state: curl 'http://192.168.1.203/focuser'
 *    Move to a position:  curl 'http://192.168.1.203/focuser?absolutePosition=20000'
 *    Change the speed config:  curl 'http://192.168.1.203/focuser?speed=100'
 *    Change backlashSteps config: curl 'http://192.168.1.203/focuser?backlashSteps=11'
 *  October 2015 Derek OKeeffe
 *
 **/
#include <EtherCard.h>
#include <Stepper.h>

#define BUFFERSIZE 350

//HTTP responses
const char FOCUS_RESPONSE[] PROGMEM = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nPragma: no-cache\r\n\r\n{\"uptime\":\"$D$D:$D$D:$D$D\",\"speed\":$D,\"temperature\":null,\"temperatureCompensationOn\":false,\"backlashSteps\":$D,\"absolutePosition\":$D}";
const char BADREQUEST_RESPONSE[] PROGMEM = "HTTP/1.0 400 Bad Request";
const char NOTFOUND_RESPONSE[] PROGMEM = "HTTP/1.0 404 Not Found";

//Focuser defaults and constants
//The default starting position when powered on.
const int DEFAULT_ABS_POSN = 30000;
//Backlash compensation steps.
const int DEFAULT_BACKLASHSTEPS = 100;
//Max speed of the stepper. Depends on the motor.
const int MAX_SPEED = 280;
const int STEPS_PER_REVOLUTION = 195; //steps per rev of the motor. One I used is,,,wierd.
const int GEARBOX_MULTIPLIER = 10; //if the stepper is attached to a gearbox. In my case it is. All steps (backlash and movements) will be multiplied by this.

//ALWAYS_APPROACH_CCW_BACKLASH_COMPENSATION is useful when attaching to SCT mirror shift focuser knobs.
//The final focus turn should always be CCW on most SCTs to avoid mirror movement during long exposures. This strategy also reduces image shift when using auto focus software.
//This setting works in conjunction with backlash steps. If CW motion, then it will go beyond the requested position then back CCW to the requested position.
//If set to false then backlash steps are only applied on direction changes.
//If using zero backlash crawford focuser, set this to false and backlash steps to 0.
const boolean ALWAYS_APPROACH_CCW_BACKLASH_COMPENSATION = true;



// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74, 0x69, 0x69, 0x2D, 0x30, 0x31 };
static byte myip[] = { 192, 168, 1, 203 }; //STATIC IP address

//ethernet buffer config
byte Ethernet::buffer[BUFFERSIZE];
BufferFiller bfill;

//motor settings
static int backlashSteps;
static int currentPosition;
static int currentSpeed;
//-1=counter clock wise motion, +1 = clockwise.
static int previousDirection = 0;
Stepper myStepper(STEPS_PER_REVOLUTION, 6, 7);

/**
 * Setup: Initalise the ethernet module and motor controller. Set absolute position to default
 */
void setup () {
  Serial.begin(9600);
  currentPosition = DEFAULT_ABS_POSN;
  currentSpeed = MAX_SPEED;
  backlashSteps = DEFAULT_BACKLASHSTEPS;
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) {
    Serial.println(F("Failed to access Ethernet controller"));
  }
  ether.staticSetup(myip);
  myStepper.setSpeed(currentSpeed);
}

/**
 * Build the HTTP response for the focus json object.
 */
static word focusResponse() {
  long t = millis() / 1000;
  word h = t / 3600;
  byte m = (t / 60) % 60;
  byte s = t % 60;
  bfill = ether.tcpOffset();
  bfill.emit_p(FOCUS_RESPONSE,
               h / 10, h % 10, m / 10, m % 10, s / 10, s % 10, currentSpeed, backlashSteps, currentPosition);
  return bfill.position();
}

/**
 * Bubild the 404 HTTP response
 */
static word notFound() {
  bfill = ether.tcpOffset();
  bfill.emit_p(NOTFOUND_RESPONSE);
  return bfill.position();
}


/**
 * Main loop
 */
void loop () {
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);
  if (pos)  {
    bfill = ether.tcpOffset();
    char* data = (char *) Ethernet::buffer + pos;
    //Serial.println(data);
    String stringData = String(data);
    if (!stringData.startsWith("GET /focuser")) {
      ether.httpServerReply(notFound());
    } else if (stringData.startsWith("GET /focuser?")) {
      interpretCommandFromQueryString(data);
      ether.httpServerReply(focusResponse());
    } else {
      ether.httpServerReply(focusResponse());
    }
  }
}

static int getIntArg(const char* data, const char* key, int value = -1) {
  char temp[30];
  if (ether.findKeyVal(data + 7, temp, sizeof temp, key) > 0) {
    value = atoi(temp);
  }
  return value;
}

/**
 * Iterpret the query string and move the stepper if needed
 */
static void interpretCommandFromQueryString (const char* data) {
  int requestedPosition = currentPosition;
  if (data[12] == '?') {
    int s = getIntArg(data, "speed", currentSpeed);
    int a = getIntArg(data, "absolutePosition", currentPosition);
    int b = getIntArg(data, "backlashSteps", backlashSteps);
    if (s > 0) {
      myStepper.setSpeed(s);
      currentSpeed = s;
    }
    if (a > 0) {
      requestedPosition = a;
    }
    if (b > 0) {
      backlashSteps = b;
    }
  }
  if (requestedPosition != currentPosition) {
    int steps = currentPosition - requestedPosition;
    moveMotor(steps);
    currentPosition = requestedPosition;
  }
}

/**
 * Move the motor and also apply backlash compensation depending on settings.
 * If ALWAYS_APPROACH_CCW_BACKLASH_COMPENSATION==true, then backlash is only applied for all CW motion. Rotating CW, then back CCW the backlash steps.
 * If ALWAYS_APPROACH_CCW_BACKLASH_COMPENSATION==false then backlash is applied on direction changes.
 * Whenever backlash motion is applied, motor speed is set to max.
 */
static void moveMotor(int steps) {
  String debugMessage = "moving: ";
  if (ALWAYS_APPROACH_CCW_BACKLASH_COMPENSATION) {
    //always finish CCW backlash compensation mode
    Serial.println(debugMessage + (steps * GEARBOX_MULTIPLIER));
    myStepper.step(steps * GEARBOX_MULTIPLIER);
    if (steps > 0) {
      Serial.println("Going CW then back CCW by backlash ammount");
      //CW motion request so we need to go further than requested then back
      myStepper.setSpeed(MAX_SPEED);
      myStepper.step(backlashSteps * GEARBOX_MULTIPLIER);    //go CW backlash
      myStepper.step(backlashSteps * -1 * GEARBOX_MULTIPLIER); //now go back CCW to where we came from;
      myStepper.setSpeed(currentSpeed);
    }
  } else {
    //standard backlash compensation mode
    if (!backlashSteps > 0 && (steps > 0 && previousDirection < 0 || steps < 0 && previousDirection > 0)) {
      //backlash compensation required due to direction change. Go as fast as possible to compensate for backlash
      Serial.println("Backlash compensating");
      myStepper.setSpeed(MAX_SPEED);
      if (steps > 0) {
        myStepper.step(backlashSteps * GEARBOX_MULTIPLIER);
      } else {
        myStepper.step(backlashSteps * -1 * GEARBOX_MULTIPLIER);
      }
      myStepper.setSpeed(currentSpeed);
    }
    Serial.println(debugMessage + (steps * GEARBOX_MULTIPLIER));
    myStepper.step(steps*GEARBOX_MULTIPLIER);
  }
  if (steps < 0) {
    previousDirection = -1;
  } else {
    previousDirection = 1;
  }
}


