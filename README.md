# Automatic chicken coop door and light management

This is device calculates the sunrise and sunset times for a specific location, and utilizes a stepper motor that operates a guillotine door of a chicken coop at sunrise and some predetermined time after sunset. Additionally, it calculates the hours of daylight of a specific day and if the number of hours is less than 15 hours[^1], it turns on a relay for the additional time until the total light time is 15 hours. A small push-button is also incorporated to test the door and relay operation.

[^1]: Chickens need at least 14 hours of light to achieve maximum egg production. See [here p.28-29](https://lohmann-breeders.com/media/strains/cage/management/LOHMANN-Brown-Classic-Cage.pdf)

![Finished board with motor](/images/finished-board-with-motor.jpg)

## Features
### Can handle power loss
 After each complete movement of the motor, the door status is written to the EEPROM. This saves the door status in case of power loss. The RTC module has its own battery and can also handle power loss well. I have found that this method is fairly robust and that the only time it might fail is when the power loss occurs during motor movement (see more in shortcomings chapter below). If power loss occurs at a different time (*not* during motor operation) and the status of the door is not what it should be at the time, the motor will be operated to move the door to the correct position according to the time rules.
### Motor is turned off after operation
After each motor operation the coils of the motor are turned off since this stepper motor has a gear reduction and can stay in place even with some weight pulling on its shaft. Normally, stepper motors without gear reduction need constant power to stay in place.
### Extra time after sunset
The door is programmed to open at sunrise and close 45 minutes after sunset. I found this extra 45 minutes by observing when the chickens were all inside. You might need to adjust this time since it is affected by local geography, for example the sun may set earlier due to mountains.
### Troubleshooting messages in serial monitor
The code includes also some messages which are printed to the serial monitor of the Arduino which can help with troubleshooting.
### Test button
Pressing the test button will make the motor move the door to the inverse position it is currently in and then move back. During this operation the relay is also enabled and turns the light on during the test loop (for example, if the door is closed, the motor will open  the door fully and then close it).
### Sketch loop executes every 30 seconds
The loop in the sketch executes every 30 seconds since that is enough accuracy for this application. The press of the button is registered instantly, however, the test run might take up to 30 seconds to start.
### Relay module for light
A relay module is used to operate the light using mains voltage.

![Testing](/images/testing.jpg)

## Parts
- **Arduino nano**
- **DS3232 RTC module** (accurate Real-Time-Clock module)
- **28BYJ-48 12V Stepper Motor** (I started this project with a 5V version of the motor but it lacked the power to pull up the door. Turning the voltage up to 8V on the 5V motor also worked but, in the end, I decided to get a 12V motor for a more robust system)
- **ULN2003 Stepper Motor Driver Board**
- **LM7805 voltage regulator IC** (to power the Arduino and modules to avoid using the Arduino's integrated voltage regulator)
- **Capacitors** (used to support the LM7805 IC, I used 4.7uF and 100uF but the values are not that critical)
- **Push button** (for test button)
- **Blank PCB board**
- **12V Power Supply**
- **DC connector female plug** (you can also not use a plug and wire the power supply to the circuit board)
- **Wire** (used for connections in the circuit board)
- **Plywood** (I used plywood to make the frame for the motor because I don't have access to a 3D printer. The motor mount can be easily made on a 3D printer. I used also an 8mm plywood to screw the different modules on it, using it as a base to stabilize the circuit board)
- **Screws** (to fasten the device on the chicken coop)
- **Thin rope** (I used window blind cord 0.9mm)
- **Small light carabiner** (used to connect rope and door)
- **Motor shaft mount** (This is a thing I had lying around, there may be a better option to attach and axle on the motor's shaft. A a 3D printed pulley might also work)
- **6mm metal axle** (I used superglue to glue the rope on the axle and rolled the rope around it a couple of times)
- **Bearing 6mm inside diameter** (this is used to stabilize the axle on the opposite side of the motor shaft)
- **Plastic waterproof container** (to protect the circuit board from the weather)

## Tools used
- Disk saw
- Soldering iron
- Drill
- Wood file
- Metal file

## Schematic
![Schematic](/schematic/Schematic_image.png)
The schematic is not 100% accurate since I used a [relay module](https://www.google.com/search?q=relay+module&client=opera&hs=yGF&hl=en&sxsrf=ALiCzsaHsf7KAVujgnkloH3JhEFOaq0Jtw:1656423081640&source=lnms&tbm=isch&sa=X&ved=2ahUKEwiuv4ProND4AhXy_rsIHXoNC18Q_AUoAXoECAEQAw&biw=2520&bih=1340&dpr=2#imgrc=O8RELdICkJA8eM) and not a "single" relay switch, but the wiring is straightforward. A printable and pdf version can be found [here](/schematic/).


## Instructions
These instructions are related to the edits of the sketch that need to be made to make it fit for your purposes. No instructions are given for the motor mount since it will differ from case to case. You will need to come up with what suits you best in terms of motor positioning, pulleys, door weight etc.  
See [images](/images/) for  photos of my construction process.

#### 1. Door range and step count
To adjust the maximum door range, the variable `maxstep` is used. To calculate your `maxstep` you need to know the steps needed for a full rotation stepper motor. This can be found in the datasheet. There are, however, different versions of this motor available with different gear reductions, so you have to test it yourself with some trial and error. After you figured out the total number of steps needed for one revolution, you need can take your axle/pulley and manually measure how many revolutions it takes to completely open the door. Do not forget to account for the difference in diameter caused by the rope that is rolling on itself. Finally, you multiply the number of rotations with the number of steps to get the `maxstep`. In my case: 2048 steps per revolution x 9 revolutions = 18432 steps.
#### 2. Adjust coordinates
Edit the `myLat` and `myLon` variables to the coordinates of your chicken coop.
#### 3. Adjust time zone variables
Adjust the time zone variables according to the [library instructions](https://github.com/JChristensen/Timezone#coding-timechangerules).
#### 4. Set the RTC time to UTC
To set the time of the RTC you can use the [SetSerial.ino](https://github.com/JChristensen/DS3232RTC/tree/master/examples/SetSerial) example from DS3232RTC library to set the RTC to **UTC** (*not* your timezone).
#### 5. Adjust Arduino pins
Do not forget to edit your sketch if you use different pins on your Arduino for example to connect the motor. If you use a different pin for the test button (`btn_pin`), you should make sure that it supports [hardware interrupts](https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/#:~:text=BOARD,with%20CHANGE). The relay pin can also be changed and its variable is called `relay_pin`.
#### 6. Populate the EEPROM address
For the first execution of the sketch you need to populate the EEPROM address that is used to save the door status. You can do that by uncommenting the following line, upload the sketch, comment the line again and re-upload the sketch.
```
EEPROM.update(0, 1);
```
It is not advised to make edits on the EEPROM commands without fully [understanding what it is](www.techtarget.com/whatis/definition/EEPROM-electrically-erasable-programmable-read-only-memory), since the EEPROM has limited write cycles and using it in a loop without caution will wear it out quickly.
#### 7. Adjust the overhead time
To account for the time after sunset that is still light, I used and overhead of 45 minutes. I did not use a variable for this because I had not intended to change it. You can use the find & replace function to change `45` to another value of minutes that suits your chickens better.
#### 8. Use the specified versions of libraries in the sketch
The library versions that are used in the sketch are noted in a comment next to the library. Using different versions of the same library might cause issues. If you want to make use of a newer version of a library make sure everything works as expected and the library syntax has not changed between versions.

## Shortcomings and possible solutions
As stated before, if power loss occurs during motor operation the state of the door will not be saved and the Arduino will assume the previous door status. I have found that this is a very rare occurrence since the motor operation takes less than a minute. If, however, this is a problem for you, you could power the motor and Arduino by a battery, and power the charger of the battery with mains power.

Another problem will occur when the cell button battery of the RTC module depletes and needs to be replaced. The time needs to be set again on the module. I have made my board with female headers so I can easily replace any module that needs replacement, and in this case, I could reprogram the time at home using another Arduino without the need to remove the whole motor mount which is screwed to the chicken coop. \
This issue can also be mitigated by using another microcontroller with Wi-Fi capabilities to set the time over the internet, if internet is available. Another solution would be to set the time by a GPS module. This problem however is very insignificant since the batteries in an RTC module generally last around 8 years.

A problem that can also occur is that the chickens refuse to go in the coop on a timely manner. Even though this is likely unusual given the 45 minute headroom after sunset, one can implement a system to count the chickens using a proximity sensor. This raises different issues. For example, the door might stay open waiting for a rogue chicken that refuses to go in and in turn exposes the whole flock to the dangerous predators.

## Notes
This is my first relatively advanced project so any issues, tips or comments are welcome.

## Possible Enhancements
- Option to set the upper and lower bound of the motor using an interface instead of hardcoding the steps into the sketch.
- Improve the sketch to use more advanced methods for delay using the `millis()` function instead of `delay()`.
- Using a Wi-Fi capable microcontroller, instead of an Arduino nano, to send status messages of the system (for example to telegram).
- Option to keep the door closed when it should be open according to the time rules. This can be helpful when getting new chickens or transporting chickens from different coop. When moving chickens to a new location they are not used to the new coop and wander outside and find other places to shelter. One needs to catch them and put them in the coop until they adjust to their new coop. I have mitigated this issue by using a small carabiner that connects the rope to the door. This way I can decouple the door from the system.
- Implement system to use a different EEPROM address every time to minimize EEPROM wear.

## References and Sources
1. https://github.com/samirsogay/AutomatingIkeaBlinds
2. https://github.com/PaulStoffregen/Time
3. https://github.com/JChristensen/Timezone
4. https://github.com/JChristensen/DS3232RTC
5. https://github.com/JChristensen/JC_Sunrise
6. https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
7. https://www.techtarget.com/whatis/definition/EEPROM-electrically-erasable-programmable-read-only-memory
8. https://lohmann-breeders.com/media/strains/cage/management/LOHMANN-Brown-Classic-Cage.pdf
