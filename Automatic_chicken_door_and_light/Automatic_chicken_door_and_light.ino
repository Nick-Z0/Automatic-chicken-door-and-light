// Author: Nick Papanikolaou
// Use SetSerial.ino example from DS3232RTC library to set the RTC to (!)UTC(!) NOT YOUR local timezone (important).

#include <TimeLib.h>    // v1.6.1  https://github.com/PaulStoffregen/Time
#include <Timezone.h>   // v1.2.4  https://github.com/JChristensen/Timezone
#include <DS3232RTC.h>  // v2.0.0  https://github.com/JChristensen/DS3232RTC
#include <JC_Sunrise.h> // v1.0.3  https://github.com/JChristensen/JC_Sunrise
#include <EEPROM.h>

// Stepper Parameters
// Keeps track of the current step.
// We'll use a zero based index.
int currentStep = 0;          // for stepper pin loop
unsigned int maxstep = 22528; // 2048 steps per rev x 11 revolutions
int step = 0;                 // actual steps in max steps, 0 means door is open, maxstep (22528) means door is closed
#define STEPPER_PIN_1 4
#define STEPPER_PIN_2 5
#define STEPPER_PIN_3 6
#define STEPPER_PIN_4 7

// Timezone and Location Parameters
constexpr float myLat{39.6611}, myLon{21.6385};

// Eastern European Time (Athens)
// Adjust timezone settings according to https://github.com/JChristensen/Timezone (v1.2.4)
TimeChangeRule EEST = {"EEST", Last, Sun, Mar, 2, 180}; // Eastern European Summer Time
TimeChangeRule EET = {"EET ", Last, Sun, Oct, 3, 120};  // Eastern European Standard Time
Timezone EE(EEST, EET);
TimeChangeRule *tcr; // pointer to the time change rule, use to get TZ abbrev

JC_Sunrise sun{myLat, myLon, JC_Sunrise::officialZenith};

// Other Parameters (time, light and door status)
DS3232RTC myRTC;
unsigned int currentTime;    // Current time in seconds
bool lightStatus = false;    // Light relay status
unsigned int extraLightTime; // Time the light stays on
unsigned int doorStatus;     // Door status, open or closed
bool command;

// Interrupt system
const uint8_t btn_pin = 2;
const uint8_t relay_pin = 9;
volatile uint8_t flag = 0;

void setup()
{
  pinMode(STEPPER_PIN_1, OUTPUT);
  pinMode(STEPPER_PIN_2, OUTPUT);
  pinMode(STEPPER_PIN_3, OUTPUT);
  pinMode(STEPPER_PIN_4, OUTPUT);

  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);

  Serial.begin(9600);
  while (!Serial);
  myRTC.begin();

  setSyncProvider(myRTC.get); // the function to get the time from the RTC
  if (timeStatus() != timeSet)
    Serial.println("Unable to sync with the RTC");
  else
    Serial.println("RTC has set the system time");

  Serial.print("Today is (UTC)");
  printDate(myRTC.get());
  Serial.println("");

  time_t utc = now();
  time_t currentTime = EE.toLocal(utc, &tcr);
  time_t sunRise, sunSet;
  sun.calculate(currentTime, tcr->offset, sunRise, sunSet);

  Serial.print("Today sunrise and sunset: ");
  printDate(sunRise);
  Serial.print(" / ");
  printDate(sunSet);
  Serial.println("");

  // Set beginning door status = open, run only once to populate EEPROM address
  // EEPROM.update(0, 1);

  pinMode(relay_pin, OUTPUT); // sets the digital pin relay_pin as output (light relay)
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(btn_pin, INPUT_PULLUP);
  EIFR = (1 << INTF0); // clear interrupt 0 flag from previous runs (adjust the correct interrupt either INTF0 or INTF1 depending on which pin is used)
  attachInterrupt(digitalPinToInterrupt(btn_pin), toggle, FALLING);
}

void loop()
{
  Serial.println("######### LOOP START #########");

  doorStatus = EEPROM.read(0);
  Serial.print("doorStatus:");
  Serial.println(doorStatus);
  // This sets step value according to door status, the arduino does not remember the exact position of door (step)
  // in case of a power failure in the middle of door movement at the sketch's current state but can be implemented
  // by writing step to EEPROM.
  if (doorStatus == 1) {
    step = 0;
  } else {
    step = maxstep;
  }

  if (flag) {
    Serial.println("TEST START");
    flag = 0;
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(relay_pin, !digitalRead(relay_pin)); // Invert light status

    if (doorStatus == 0.00) {
      command = 1; // up
      move_motor(command, maxstep);
      EEPROM.update(0, 1); // Save door status on EEPROM
      Serial.println("Door Status: JUST OPENED DOOR FOR TESTING");

      delay(5000);

      command = 0; // down
      step = move_motor(command, 0);
      EEPROM.update(0, 0); // Save door status on EEPROM
      Serial.println("Door Status: JUST CLOSED DOOR FOR TESTING");
    } else if (doorStatus == 1) {
      command = 0; // down
      move_motor(command, 0);
      EEPROM.update(0, 0); // Save door status on EEPROM
      Serial.println("Door Status: JUST CLOSED DOOR FOR TESTING");

      delay(5000);

      command = 1; // up
      move_motor(command, maxstep);
      EEPROM.update(0, 1); // Save door status on EEPROM
      Serial.println("Door Status: JUST OPENED DOOR FOR TESTING");
    }
    digitalWrite(relay_pin, !digitalRead(relay_pin)); // Invert light status
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("TEST END");
  }

  // Calculate sunrise and sunset
  time_t utc = now();
  time_t currentTime = EE.toLocal(utc, &tcr);
  time_t sunRise, sunSet;
  sun.calculate(currentTime, tcr->offset, sunRise, sunSet);
  time_t extraLightTime = 0;

  //  DOOR

  // TESTING

  // if (currentTime > sunRise && currentTime < sunSet + 45 * 60 && doorStatus == 0.00)
  Serial.println("if (currentTime > sunRise && currentTime < sunSet + 45 * 60 && doorStatus == 0.00)");
  Serial.println("### currentTime:");
  printDate(currentTime);
  Serial.println("");
  Serial.println("###sunRise:   ########################");
  printDate(sunRise);
  Serial.println("");
  Serial.println("### sunSet:   ########################");
  printDate(sunSet);
  Serial.println();
  Serial.println("### sunSet + 45 minutes:   ############");
  printDate(sunSet + 45 * 60);
  Serial.println("");
  Serial.println("### sunSet - sunRise   #################");
  printDate(sunSet - sunRise);
  Serial.println("");

  if (currentTime > sunRise && currentTime < sunSet + 45 * 60)
  {
    if (doorStatus == 0.00) {
      // OPEN DOOR == 1
      command = 1; // up
      step = move_motor(command, step);
      EEPROM.update(0, 1); // Save door status on EEPROM
      Serial.println("Door Status: JUST OPENED DOOR");
    } else {
      Serial.println("Door Status: OPEN");
    }
  }
  else if (currentTime > sunSet + 45 * 60)
  {
    if (doorStatus == 1) {
      // CLOSE DOOR == 0
      command = 0; // down
      step = move_motor(command, step);
      EEPROM.update(0, 0); // Save door status on EEPROM
      Serial.println("Door Status: JUST CLOSED DOOR");
    } else {
      Serial.println("Door Status: CLOSED");
    }
  }

  // Light Mechanism

  time_t fifteenHours = 54000; // 15 * 3600 , hours of light chickens need
  extraLightTime = 0;
  if (sunSet - sunRise < fifteenHours)
  {
    extraLightTime = fifteenHours - (sunSet - sunRise);
  }
  else
    extraLightTime = 0;

  if (currentTime > sunSet && currentTime < sunSet + extraLightTime && lightStatus == false)
  {
    digitalWrite(relay_pin, HIGH); // Turn light ON
    lightStatus = true;
  }
  else if (currentTime > sunSet + extraLightTime)
  {
    digitalWrite(relay_pin, LOW); // Turn light OFF
    lightStatus = false;
  }
  else if (currentTime < sunSet)
  {
    digitalWrite(relay_pin, LOW); // Turn light OFF
    lightStatus = false;
  }

  delay(30000);     // 30 seconds
  // delay(240000); // 4 minutes
}

int move_motor(bool direction, int step)
{
  if (direction == 1) // go up
  {
    while (step > 0)
    {

      switch (currentStep)
      {
        case 0:
          digitalWrite(STEPPER_PIN_1, HIGH);
          digitalWrite(STEPPER_PIN_2, HIGH);
          digitalWrite(STEPPER_PIN_3, LOW);
          digitalWrite(STEPPER_PIN_4, LOW);

          break;
        case 1:
          digitalWrite(STEPPER_PIN_1, LOW);
          digitalWrite(STEPPER_PIN_2, HIGH);
          digitalWrite(STEPPER_PIN_3, HIGH);
          digitalWrite(STEPPER_PIN_4, LOW);

          break;
        case 2:
          digitalWrite(STEPPER_PIN_1, LOW);
          digitalWrite(STEPPER_PIN_2, LOW);
          digitalWrite(STEPPER_PIN_3, HIGH);
          digitalWrite(STEPPER_PIN_4, HIGH);
          break;
        case 3:

          digitalWrite(STEPPER_PIN_1, HIGH);
          digitalWrite(STEPPER_PIN_2, LOW);
          digitalWrite(STEPPER_PIN_3, LOW);
          digitalWrite(STEPPER_PIN_4, HIGH);
          break;
      }
      step--;
      currentStep = (++currentStep < 4) ? currentStep : 0;
      delay(2);
    }
  }
  else if (direction == 0) // go down
  {
    while (step <= maxstep)
    {
      switch (currentStep)
      {
        case 0:
          digitalWrite(STEPPER_PIN_1, LOW);
          digitalWrite(STEPPER_PIN_2, LOW);
          digitalWrite(STEPPER_PIN_3, HIGH);
          digitalWrite(STEPPER_PIN_4, HIGH);

          break;
        case 1:
          digitalWrite(STEPPER_PIN_1, LOW);
          digitalWrite(STEPPER_PIN_2, HIGH);
          digitalWrite(STEPPER_PIN_3, HIGH);
          digitalWrite(STEPPER_PIN_4, LOW);

          break;
        case 2:
          digitalWrite(STEPPER_PIN_1, HIGH);
          digitalWrite(STEPPER_PIN_2, HIGH);
          digitalWrite(STEPPER_PIN_3, LOW);
          digitalWrite(STEPPER_PIN_4, LOW);
          break;
        case 3:
          digitalWrite(STEPPER_PIN_1, HIGH);
          digitalWrite(STEPPER_PIN_2, LOW);
          digitalWrite(STEPPER_PIN_3, LOW);
          digitalWrite(STEPPER_PIN_4, HIGH);
          break;
      }
      step++;
      currentStep = (++currentStep < 4) ? currentStep : 0;
      // 2000 microseconds, or 2 milliseconds seems to be
      // about the shortest delay that is usable.  Anything
      // lower and the motor starts to freeze.
      delayMicroseconds(2250);
      // delay(2);
    }
  }

  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);
  return step;
}

// This function is to print date-time from t_time variable (unix time)
void printDate(time_t date)
{
  char buff[20];
  sprintf(buff, "%2d-%02d-%4d %02d:%02d:%02d",
          day(date), month(date), year(date), hour(date), minute(date), second(date));
  Serial.print(buff);
}

void toggle() {
  flag = 1;
}