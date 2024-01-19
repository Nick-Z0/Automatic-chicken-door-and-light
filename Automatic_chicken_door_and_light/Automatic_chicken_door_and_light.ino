// Author: Nick Papanikolaou
// Use SetSerial.ino example from DS3232RTC library to set the RTC to (!)UTC(!) NOT YOUR local timezone (important).

#include <TimeLib.h>     // v1.6.1  https://github.com/PaulStoffregen/Time
#include <Timezone.h>    // v1.2.4  https://github.com/JChristensen/Timezone
#include <DS3232RTC.h>   // v2.0.1  https://github.com/JChristensen/DS3232RTC
#include <JC_Sunrise.h>  // v1.0.3  https://github.com/JChristensen/JC_Sunrise
#include <EEPROM.h>

// Stepper Parameters
// Keeps track of the current step.
int currentStep = 0;  // for stepper pin loop
int maxStep = 20480;  // 20480 are the steps for the door movement: 2048 steps per rev x 10 revolutions
long step = 100000;   // initial step value, 100000 means door is open, maxStep + 100000 (120480) means door is closed
#define STEPPER_PIN_1 4
#define STEPPER_PIN_2 5
#define STEPPER_PIN_3 6
#define STEPPER_PIN_4 7

const uint8_t test_btn_pin = 2;   // Test button
const uint8_t relay_pin = 9;      // Pin to toggle the relay module
const uint8_t btn_up_pin = 12;    // Buttons for fine-tuning the door position
const uint8_t btn_down_pin = 11;  // Buttons for fine-tuning the door position

// Timezone and Location Parameters
constexpr float myLat{ 39.6611 }, myLon{ 21.6385 };

// Eastern European Time (Athens)
// Adjust timezone settings according to https://github.com/JChristensen/Timezone (v1.2.4)
TimeChangeRule EEST = { "EEST", Last, Sun, Mar, 2, 180 };  // Eastern European Summer Time
TimeChangeRule EET = { "EET ", Last, Sun, Oct, 3, 120 };   // Eastern European Standard Time
Timezone EE(EEST, EET);
TimeChangeRule *tcr;  // pointer to the time change rule, use to get TZ abbrev

JC_Sunrise sun{ myLat, myLon, JC_Sunrise::officialZenith };

// Other Parameters (time, light and door status)
DS3232RTC myRTC;
time_t currentTime = 0;     // Current time
bool lightStatus = false;   // Light relay status
time_t extraLightTime = 0;  // Time the light stays on
time_t sunRise, sunSet;
time_t sunSetBuffer = 45 * 60;  // Time after sunset when the door will close. At sunset it is not dark enough for the chickens to move in.
unsigned int doorStatus;        // Door status, open or closed
bool direction = 0;             // Motor movement direction 1 for opening, 0 for closing

unsigned long lastDiagnosticsTime = 0;      // Tracks the last time diagnostics was run
unsigned long lastDoorStatusPrintTime = 0;  // Tracks the last time the door status was printed
unsigned long lastRTCSyncTime = 0;          // Tracks the last time the RTC was synced
unsigned long lastTimeUpdateTime = 0;       // Tracks the last time the currentTime was updated


void setup() {
  pinMode(STEPPER_PIN_1, OUTPUT);
  pinMode(STEPPER_PIN_2, OUTPUT);
  pinMode(STEPPER_PIN_3, OUTPUT);
  pinMode(STEPPER_PIN_4, OUTPUT);

  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);

  Serial.begin(9600);
  while (!Serial)
    ;
  myRTC.begin();

  setSyncProvider(myRTC.get);  // get the time from the RTC

  if (timeStatus() != timeSet)
    Serial.println("Unable to sync with the RTC");
  else
    Serial.println("RTC has set the system time");

  Serial.print("Today is (UTC)");
  printDate(myRTC.get());
  Serial.println("");

  // Set beginning door status = open, run only once to populate EEPROM address
  // EEPROM.update(0, 1);

  pinMode(relay_pin, OUTPUT);  // sets the digital pin relay_pin as output (light relay)
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(test_btn_pin, INPUT_PULLUP);
  pinMode(btn_up_pin, INPUT_PULLUP);
  pinMode(btn_down_pin, INPUT_PULLUP);

  doorStatus = EEPROM.read(0);
  // This sets step value according to door status, the arduino does not remember the exact position of door (step)
  // in case of a power failure in the middle of door movement at the sketch's current state but can be implemented
  // by writing step to EEPROM.
  if (doorStatus == 1) {
    step = 100000;  // Door Open
  } else {
    step = maxStep + 100000;  // Door Closed
  }

  syncTimeWithRTCAndUpdateSunTimes();  // Initial update of time variables
  diagnostics(currentTime, sunRise, sunSet, doorStatus);
}


void loop() {
  unsigned long currentMillis = millis();
  doorStatus = EEPROM.read(0);

  // Sync time with RTC twice a day, at 00:00 and 12:00
  int currentHour = hour();
  if ((currentHour == 0 || currentHour == 12) && (currentMillis - lastRTCSyncTime >= 43200000)) {  // 43200000 milliseconds = 12 hours
    syncTimeWithRTCAndUpdateSunTimes();
    lastRTCSyncTime = currentMillis;
  }

  // Update currentTime every minute
  if (currentMillis - lastTimeUpdateTime >= 60000) {  // 60000 milliseconds = 1 minute
    updateCurrentTime();
    lastTimeUpdateTime = currentMillis;
  }

  if (currentMillis - lastDiagnosticsTime >= 10000) {  // Print diagnostics every 10 seconds
    diagnostics(currentTime, sunRise, sunSet, doorStatus);
    lastDiagnosticsTime = currentMillis;
  }

  // DOOR
  if (currentTime < sunRise) {
    if (doorStatus == 1) {  // If the door is open before sunrise
      Serial.println("Door Status: Closing door before sunrise...");
      direction = 0;  // down
      step = move_motor(direction, step, maxStep);
      EEPROM.update(0, 0);  // Save door status on EEPROM
      Serial.println("Door Status: JUST CLOSED DOOR");
    } else {
      if (currentMillis - lastDoorStatusPrintTime >= 5000) {
        Serial.println("Door Status: CLOSED");
        lastDoorStatusPrintTime = currentMillis;
      }
    }
  } else if (currentTime >= sunRise && currentTime < sunSet + sunSetBuffer) {
    if (doorStatus == 0.00) {
      // OPEN DOOR == 1
      Serial.println("Door Status: Opening door for sunrise...");
      direction = 1;  // up
      step = move_motor(direction, step, maxStep);
      EEPROM.update(0, 1);  // Save door status on EEPROM
      Serial.println("Door Status: JUST OPENED DOOR");
    } else {
      if (currentMillis - lastDoorStatusPrintTime >= 5000) {
        Serial.println("Door Status: OPEN");
        lastDoorStatusPrintTime = currentMillis;
      }
    }
  } else if (currentTime >= sunSet + sunSetBuffer) {
    if (doorStatus == 1) {
      // CLOSE DOOR == 0
      Serial.println("Door Status: Closing door for sunset...");
      direction = 0;  // down
      step = move_motor(direction, step, maxStep);
      EEPROM.update(0, 0);  // Save door status on EEPROM
      Serial.println("Door Status: JUST CLOSED DOOR");
    } else {
      if (currentMillis - lastDoorStatusPrintTime >= 5000) {
        Serial.println("Door Status: CLOSED");
        lastDoorStatusPrintTime = currentMillis;
      }
    }
  }


  // DOOR FINE-TUNING
  // Check for up button press with debounce
  if (debounceButton(btn_up_pin)) {
    Serial.println(step);
    while (debounceButton(btn_up_pin)) {
      // Move motor up slightly as long as the button is pressed
      step = move_motor(1, step, 512);  // Move motor up by 512 steps
      delay(1);                         // Short delay to control the speed of movement
      Serial.print("Moving up, step position: ");
      Serial.println(step);
    }
    Serial.println("Up button released.");
  }

  // Check for down button press with debounce
  if (debounceButton(btn_down_pin)) {
    Serial.println(step);
    while (debounceButton(btn_down_pin)) {
      // Move motor down slightly as long as the button is pressed
      step = move_motor(0, step, 512);  // Move motor down by 512 steps
      delay(1);                         // Short delay to control the speed of movement
      Serial.print("Moving down, step position: ");
      Serial.println(step);
    }
    Serial.println("Down button released.");
  }

  // LIGHT
  lightMechanism(sunRise, sunSet);

  //TESTING
  if (debounceButton(test_btn_pin)) {
    testMotorAndLight();
  }
}


long move_motor(bool direction, long current_step, int step_count) {
  long target_step = direction ? current_step - step_count : current_step + step_count;

  // Inside move_motor function
  Serial.print("Current step: ");
  Serial.println(current_step);
  Serial.print("Target step: ");
  Serial.println(target_step);

  while (current_step != target_step) {
    if (direction) {
      // Sequence for moving up
      switch (currentStep) {
        case 0:
          digitalWrite(STEPPER_PIN_1, HIGH);
          digitalWrite(STEPPER_PIN_2, LOW);
          digitalWrite(STEPPER_PIN_3, LOW);
          digitalWrite(STEPPER_PIN_4, HIGH);
          break;
        case 1:
          digitalWrite(STEPPER_PIN_1, LOW);
          digitalWrite(STEPPER_PIN_2, LOW);
          digitalWrite(STEPPER_PIN_3, HIGH);
          digitalWrite(STEPPER_PIN_4, HIGH);
          break;
        case 2:
          digitalWrite(STEPPER_PIN_1, LOW);
          digitalWrite(STEPPER_PIN_2, HIGH);
          digitalWrite(STEPPER_PIN_3, HIGH);
          digitalWrite(STEPPER_PIN_4, LOW);
          break;
        case 3:
          digitalWrite(STEPPER_PIN_1, HIGH);
          digitalWrite(STEPPER_PIN_2, HIGH);
          digitalWrite(STEPPER_PIN_3, LOW);
          digitalWrite(STEPPER_PIN_4, LOW);
          break;
      }
    } else {
      // Sequence for moving down
      switch (currentStep) {
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
    }
    current_step = direction ? current_step - 1 : current_step + 1;
    currentStep = (currentStep + 1) % 4;  // Cycle through step sequence
    delayMicroseconds(2250);              // 2000 microseconds, or 2 milliseconds seems to be
                                          // about the shortest delay that is usable. Anything
                                          // lower and the motor starts to freeze.
  }

  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);

  return current_step;
}


// This function is to print date-time from t_time variable (unix time)
void printDate(time_t date) {
  char buff[20];
  sprintf(buff, "%2d-%02d-%4d %02d:%02d:%02d",
          day(date), month(date), year(date), hour(date), minute(date), second(date));
  Serial.print(buff);
}


void diagnostics(time_t currentTime, time_t sunRise, time_t sunSet, unsigned int doorStatus) {
  char buffer[50];  // Buffer to hold formatted strings

  Serial.println("########## Diagnostics ##########");

  // Print Current Date and Time
  sprintf(buffer, "Current Date:      %02d-%02d-%4d", day(currentTime), month(currentTime), year(currentTime));
  Serial.println(buffer);
  sprintf(buffer, "Current Time:      %02d:%02d:%02d", hour(currentTime), minute(currentTime), second(currentTime));
  Serial.println(buffer);

  // Print UTC Time
  time_t utcTime = myRTC.get();
  sprintf(buffer, "UTC Time:          %02d:%02d:%02d", hour(utcTime), minute(utcTime), second(utcTime));
  Serial.println(buffer);

  // Print Sunrise and Sunset Times
  sprintf(buffer, "Sunrise:           %02d:%02d:%02d", hour(sunRise), minute(sunRise), second(sunRise));
  Serial.println(buffer);
  sprintf(buffer, "Sunset:            %02d:%02d:%02d", hour(sunSet), minute(sunSet), second(sunSet));
  Serial.println(buffer);

  // Print Sunset + 45 Minutes
  time_t sunSetPlus45 = sunSet + sunSetBuffer;
  sprintf(buffer, "Sunset + 45 mins:  %02d:%02d:%02d", hour(sunSetPlus45), minute(sunSetPlus45), second(sunSetPlus45));
  Serial.println(buffer);

  // Print Day Length
  time_t dayLength = sunSet - sunRise;
  sprintf(buffer, "Day Length:        %02d:%02d:%02d", hour(dayLength), minute(dayLength), second(dayLength));
  Serial.println(buffer);

  // Print Door Status
  sprintf(buffer, "Door Status:       %u", doorStatus);
  Serial.println(buffer);

  // Print step
  sprintf(buffer, "step:              %ld", step);
  Serial.println(buffer);

  Serial.println("##################################");
}

void lightMechanism(time_t sunRise, time_t sunSet) {
  time_t fifteenHours = 54000;  // 15 * 3600 , hours of light chickens need
  extraLightTime = 0;
  if (sunSet - sunRise < fifteenHours) {
    extraLightTime = fifteenHours - (sunSet - sunRise);
  } else
    extraLightTime = 0;

  if (currentTime > sunSet && currentTime < sunSet + extraLightTime && lightStatus == false) {
    digitalWrite(relay_pin, HIGH);  // Turn light ON
    lightStatus = true;
  } else if (currentTime > sunSet + extraLightTime) {
    digitalWrite(relay_pin, LOW);  // Turn light OFF
    lightStatus = false;
  } else if (currentTime < sunSet) {
    digitalWrite(relay_pin, LOW);  // Turn light OFF
    lightStatus = false;
  }
}


void syncTimeWithRTCAndUpdateSunTimes() {
  // Get the current UTC time from the RTC
  time_t utc = myRTC.get();
  // Convert UTC to local time and update the currentTime
  currentTime = EE.toLocal(utc, &tcr);
  // Calculate sunrise and sunset times based on the current local time
  sun.calculate(currentTime, tcr->offset, sunRise, sunSet);
}


void updateCurrentTime() {
  // Update currentTime from the Arduino's internal clock
  currentTime = EE.toLocal(now(), &tcr);
}


void testMotorAndLight() {
  char buffer[100];  // Buffer for formatted string

  Serial.println("TEST START");
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(relay_pin, !digitalRead(relay_pin));  // Invert light status

  if (doorStatus == 0.00) {
    Serial.println("Starting to OPEN door for testing...");
    direction = 1;  // up
    step = move_motor(direction, step, maxStep);
    EEPROM.update(0, 1);  // Save door status on EEPROM
    sprintf(buffer, "Door Status: JUST OPENED DOOR FOR TESTING, Step: %ld", step);
    Serial.println(buffer);
    delay(5000);

    Serial.println("Starting to CLOSE door for testing...");
    direction = 0;  // down
    step = move_motor(direction, step, maxStep);
    EEPROM.update(0, 0);  // Save door status on EEPROM
    sprintf(buffer, "Door Status: JUST CLOSED DOOR FOR TESTING, Step: %ld", step);
    Serial.println(buffer);

  } else if (doorStatus == 1) {
    Serial.println("Starting to CLOSE door for testing...");
    direction = 0;  // down
    step = move_motor(direction, step, maxStep);
    EEPROM.update(0, 0);  // Save door status on EEPROM
    sprintf(buffer, "Door Status: JUST CLOSED DOOR FOR TESTING, Step: %ld", step);
    Serial.println(buffer);
    delay(5000);

    Serial.println("Starting to OPEN door for testing...");
    direction = 1;  // up
    step = move_motor(direction, step, maxStep);
    EEPROM.update(0, 1);  // Save door status on EEPROM
    sprintf(buffer, "Door Status: JUST OPENED DOOR FOR TESTING, Step: %ld", step);
    Serial.println(buffer);
  }

  digitalWrite(relay_pin, !digitalRead(relay_pin));  // Invert light status
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("TEST END");
}


bool debounceButton(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(100);  // Short delay, adjust as needed
    if (digitalRead(pin) == LOW) {
      return true;
    }
  }
  return false;
}

void setTestTime(int testHour, int testMinute, int testSecond) {
  tmElements_t tm;
  tm.Hour = testHour;
  tm.Minute = testMinute;
  tm.Second = testSecond;
  tm.Day = 19;  // Set these to a specific date if needed
  tm.Month = 1;
  tm.Year = 2024 - 1970;  // Adjust the year as needed
  time_t testTime = makeTime(tm);
  currentTime = testTime;
}
