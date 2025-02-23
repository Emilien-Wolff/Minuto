#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>       // for the rectangle draw and text display
#include <Adafruit_SSD1306.h>   // to control the OLED chip
#include <EEPROM.h>
#include <Arduino.h>

///////////////////////////
//    SCREEN DESCRIPTION
//     _________________
//    |_________________|   Area YELLOW 128 x 16 pixels (0 to 15)
//    |                 |
//    |                 |   Area BLUE 128 x 48 (16 to 63)
//    |                 |
//    |_________________|

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SCREEN_BLUE_START 16 // OLED blue area starts here in Y position
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin). No reset on the screen module.
#define SCREEN_ADDRESS 0x3C // The address written on the OLED board (0x78) must be divided by 2 => 0x3C

////////////////////////////////////////
// Information for the EEPROM management
////////////////////////////////////////
#define DATA_TERMINATOR 0xFFFFFFFF  // ints are stored
#define DATA_TERMINATOR_BYTE 0xFF

// Other constants
#define MAX_SUN_IRRADIANCE 1380 // value max expected, can be used to avoid abnormal displays
#define DELAY_BETWEEN_SCREEN_UPDATE 100  // in milliseconds
#define WAIT_TIME_BEFORE_START 30000    // Enough to type a command and stop the measurement start and then list the values in EEPROM

// Create the display object connected to I2C bus (Wire)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

///////////////////////
// VARIABLES 
///////////////////////
int measurement_counter = 0;  // count the number of measurements done; used for Serial print
float current_temp;
unsigned long current_time, delta_time;
int analogPin = A0;         // the pin where the voltage value must be read
unsigned long measurement_start_time = 0;   // Time at which the measurement has started
int irradiance=0;
int measurement_duration = 30000;   // initial value to have enough Delta T. This could be changed by the program to adjust if deltaT is not enough
int go_start =1;   // to know if the measurement loop can be started or not (boolean). By default start is authorized.
int mode_debug = 0;   // used to change the way of working: negative DeltaT is also put in EEPROM and displayed for debug use. Default is normal mode
static String commandBuffer = ""; // Buffer for incoming commands
int sizeOfDataLine;
// ADC characteristics
// Number of steps of the conversion : 10 bits for the "analogRead()"" function even if 14 bits are possible with R4 WIFI
// can be changed with analogReadResolution()
#define ADC_RESOLUTION 12   // CAUTION: 11 & 13 bits resolution does not work: analogRead() always gives 0. Changing to 12 bits works, so we keep this as default.
float MAX_VOLTAGE_ADC = 4.6;  // the real voltage used by the ADC on the arduino board
float ADC_STEPS;            // initialize in setup
int ADC_value = 0;
// Declaration of the coefficient for the function approaching the thermistance voltage with resistive voltage divider. #define does not work, so use variables...
float THERM_DIV_COEFF_A0 = 55.354; // for constant part
float THERM_DIV_COEFF_A1 = -35.265; // for linear part
float THERM_DIV_COEFF_A2 = 24.322; // for square part
float THERM_DIV_COEFF_A3 = -7.8677; // for cubic part
// Declaration of the coefficient for the linear result
float THERM_LIN_COEFF_A0 = 39.891; // for constant part
float THERM_LIN_COEFF_A1 = -8.5871; // for linear part
// Limits for the system to work properly
float MAX_TEMP=40.0;
float MIN_TEMP=7.0;
// for irradiance DELTA t calculation
float previous_temp;            // the previous temperature measured
unsigned long previous_time, initial_time;    // the value is in millisecond
// Bloc characteristics
float BLOC_MASSE = 0.622; // mass in SI
float BLOC_CAPA = 423.0;   // thermal capacity in SI
float BLOC_SURFACE = 0.05*0.05;  // Surface of the black face in SI

////////////////////////////

//       FUNCTIONS        //

////////////////////////////

/////////////////////////////////////
//  Function: draws a horizontal bar filled for the value given, in proportion to the max_value given
//  Input: an integer for the value and an integer for the max value (bar totally filled). Negative value not supported
//  Output: none
/////////////////////////////////////
void draw_value_in_bar(unsigned long value, unsigned long max_value)
{
    if (value>max_value) value=max_value;   // avoid to try to draw an impossible bar.
    display.fillRect(0, 0, display.width(), SCREEN_BLUE_START-1, SSD1306_BLACK);   // Draw black rectangle = erase the bar display area
    display.drawRect(0, 0, display.width(), SCREEN_BLUE_START-1, SSD1306_WHITE);   // Draw the frame for the full bar (empty fill)
    display.fillRect(0, 0, (int)display.width()*value/max_value, SCREEN_BLUE_START-1, SSD1306_WHITE); // Draw the necessary fill corresponding to the value
    display.display();  // push to the display
}  

/////////////////////////////////////
//  Function: displays a string centered vertically and horizontally in the blue part of the screen
//  Input: a text in the STRING type
//  Output: none
/////////////////////////////////////
void draw_string_centered(String text)
{

    // Text dimensions calculation
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    display.setTextSize(3);                     // Normal: 1   Use 3X normal size    
    display.setTextColor(SSD1306_WHITE);        // Draw white text    
    
    display.getTextBounds(text, 0, 16, &x1, &y1, &textWidth, &textHeight);  
    // Erase the display area with black rectangle
    display.fillRect(0, 16, display.width(), display.height(), SSD1306_BLACK);
    // compute the text cursor position to get it centered
    int16_t xPos = (display.width() - textWidth) / 2;
    int16_t yPos = ( ((display.height()- SCREEN_BLUE_START - textHeight) / 2) + SCREEN_BLUE_START );    
    display.setCursor(xPos, yPos);
    display.print(text);
    display.display();
}


/////////////////////////////////////
//  Function: gives the temperature corresponding the the ADC value, using the thermistance transfer function
//  Input: the ADC value an int
//  Output: the temperature as a float
/////////////////////////////////////
float calculate_temperature(int adc_value)
{
    float voltage, temperature;
    voltage = MAX_VOLTAGE_ADC * adc_value / ADC_STEPS;
    
    // The following formula was used for the first calibration results, where the curve was not really linear
    // temperature = THERM_DIV_COEFF_A0 + THERM_DIV_COEFF_A1*voltage + THERM_DIV_COEFF_A2*pow(voltage,2) + THERM_DIV_COEFF_A3*pow(voltage,3);

    // This is the final formule with a very slow temperature change during calibration. Result is linear
    temperature = THERM_LIN_COEFF_A0 + THERM_LIN_COEFF_A1*voltage;

    return(temperature);
}


/////////////////////////////////////
//  Function: calculates the irradiance
//  Input: the current temperature of the bloc and the current time
//  Output: irradiance as an INT. -1 if cooling and in normal mode (no debug mode)
/////////////////////////////////////
int calculate_irradiance(float temp_now, unsigned long time_now)
{
    float irrad, deltaT;
    deltaT = temp_now - previous_temp;
    if( (deltaT >= 0) || (mode_debug==1) )    // In debug mode, the cooling mode is disabled, show all measurements and store them
    {
      irrad = BLOC_MASSE * BLOC_CAPA * deltaT / BLOC_SURFACE / (time_now - previous_time) * 1000;
      return((int)irrad);   // round to integer value, precision of the system is best 4%
    }
    else
    {
      return((int)-1); // The bloc is cooling, so the irradiance calculation makes no sense
    }
}


/////////////////////////////////////
//  Function: Displays the content of the EEPROM until the DATA_TERMINATOR is found
//  Input: none
//  Output: list of values in terminal display in CSV format
/////////////////////////////////////
void listEEPROM() {
  Serial.println("Reading EEPROM contents up to terminator...");
  Serial.println("");  
  Serial.print("Measurement interval was set to : ");
  Serial.print(measurement_duration/1000.0);
  Serial.println(" sec ");
  Serial.println("");  
  Serial.println("Time(s);Irradiance(W/m2);DeltaT(K);Temp(°C)");   // Use comma to separate value, good for excel
  int EEPROMaddress = 0;
  int irrad, time;
  float deltaT,Temp;
  while(EEPROMaddress < (EEPROM.length() - sizeOfDataLine) )
  {
    EEPROM.get(EEPROMaddress,time);
    if(time == DATA_TERMINATOR)
    {
      Serial.println("Terminator reached. Stopping.");
      break;      
    }
    else
    {
      Serial.print(time); Serial.print(";");   // get the stored time in seconds
      Serial.print(EEPROM.get(EEPROMaddress+sizeof(int),irrad));  Serial.print(";"); // This is the irradiance (int)
      Serial.print(EEPROM.get(EEPROMaddress+2*sizeof(int),deltaT));  Serial.print(";"); // This is the DeltaT  (int)
      Serial.println(EEPROM.get(EEPROMaddress+2*sizeof(int)+sizeof(float),Temp)); // This is the Temperature (float)
      EEPROMaddress += sizeOfDataLine;
    }
  }
    Serial.println("Done listing EEPROM contents.");
}


/////////////////////////////////////
//  Function: Put FF into all EEPROM to detect end of data. Only writes if not already FF present to speed up.
//  Input: none
//  Output: none
/////////////////////////////////////
void EraseEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.update(i, DATA_TERMINATOR_BYTE); // Write each byte to EEPROM only if different, limits the time of erase
  }
  Serial.println("EEPROM content erased");
}


/////////////////////////////////////
//  Function: reads the serial input char by char but is not blocking
//            Executes the associated command if a valid one is is confirmed by ENTER
//  Input: none
//  Output: none. Action on global variables go_start, mode_debug, measurement_start_time, delta_time
/////////////////////////////////////
void handleSerialInput() {
  // Check if there is new serial data available
  while (Serial.available() > 0) {
    char incomingChar = Serial.read(); // Read the next character
    
    if (incomingChar == '\n') 
    { // Check for end of command (newline)
      commandBuffer.trim(); // Remove leading/trailing whitespace
      commandBuffer.toLowerCase();
      //Serial.print("Command: ");
      //Serial.println(commandBuffer);
      // Process the complete command
      if(commandBuffer == "list")
      {
            Serial.println("\nDoing list.");
            draw_string_centered("list...");
            listEEPROM();
            Serial.println("Waiting for next command");
            go_start = 0;   // The measurement is not started. Stay in the command input part, otherwise the data in EEPROM would be erased.
            measurement_start_time = millis();   // Restart the delay to enter another command
      }
      else if(commandBuffer == "start")
      {
            Serial.println("\nDoing start.");
            draw_string_centered("start..");
            delay(500);   // to have time to read
            delta_time = WAIT_TIME_BEFORE_START + 10; // to force exit of the waiting loop, set the delta to more than the limit
            go_start = 1;   // Now we can start the measurement
      }
      else if(commandBuffer == "stop")
      {
            Serial.println("\nDoing stop. Waiting endless until 'list' or 'start' is entered.");
            draw_string_centered("Stop...");
            go_start = 0;
            measurement_start_time = millis();   // Restart the delay to enter another command
      }
      else if(commandBuffer == "")
      {
            Serial.println("\nAvailable commands:  'list', 'stop', 'start',");
            Serial.println("                       'resXX' with XX= number of bits of ADC resolution");
            Serial.println("                       'debug' to switch the mode on/off to store all values in EEPROM and display them, even if DeltaT <0");
            Serial.println("and  validate with ENTER");
            draw_string_centered("Help...");
            measurement_start_time = millis();   // Restart the delay to enter another command
      }
      else if (commandBuffer.startsWith("res"))
      { // Check if the input starts with "res"
            String resValue = commandBuffer.substring(3); // Extract the numeric part after "res"
            int resolution = resValue.toInt(); // Convert the numeric part to an integer
            go_start = 0;
            if (resolution >= 10 && resolution <= 14)
            { // Adjust the range as per your ADC specs
              Serial.println("\nDoing Resolution change.");
              analogReadResolution(resolution); // Set the ADC resolution
              draw_string_centered("Res: "+String(resolution));
              ADC_STEPS = (pow(2,resolution) - 1)*1.0;
              Serial.print("ADC resolution set to ");
              Serial.print(resolution);
              Serial.println(" bits.");
              if( (resolution == 11) || (resolution == 13)) Serial.println("\nwarning odd resolution bit numbers do not work: data read always at 0\n");
              Serial.println("Enter next command...");
              measurement_start_time = millis();   // Restart the delay to enter another command
            }
            else
            {
              Serial.println("Error: Resolution must be between 10 and 14 included");
            }
      } 
      else if (commandBuffer == "debug")
      {
            Serial.println("\nDoing debug mode switch");
            if (mode_debug == 0)
            {
              mode_debug = 1;
              draw_string_centered("Dbg ON");
              Serial.println("Mode Debug set. Waiting for another command");
            } 
            else
            {
              mode_debug = 0;
              draw_string_centered("Dbg OFF");
              Serial.println("Mode Debug disabled. Waiting for another command");
            }
            measurement_start_time = millis();   // Restart the delay to enter another command
      }
      else
      {
              go_start = 0;
              measurement_start_time = millis();   // Restart the delay to enter another command
              Serial.println("Unknown command. Available commands: list, start, stop, resXX, debug and ENTER for help");
              draw_string_centered("Retry..");
      }
      commandBuffer = ""; // Clear the buffer after processing
    } 
    else
    {
      commandBuffer += incomingChar; // Append the character to the buffer
    }
  }
}


////////////////////////////

//          SETUP         //

////////////////////////////

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for the serial port to be ready
  }
  Serial.println("Serial Initialized.");
  // SSD1306 OLED init
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Clear the display buffer and updates the screen (full clear)
  display.clearDisplay();
  display.display();
  
  // Set ADC resolution
  ADC_STEPS = (pow(2,ADC_RESOLUTION) - 1)*1.0;
  analogReadResolution(ADC_RESOLUTION);
  draw_string_centered("Res: "+ String(ADC_RESOLUTION));
  delay(1000);  
  // prepare for EEPROM write
  sizeOfDataLine = 2*sizeof(int)+2*sizeof(float); // number of bytes in one set of data stored in the EEPROM
  // Time (int) then Irradiance (int) then DeltaT (float) then Temp (float)

  // infos for usage
  Serial.println(""); 
  Serial.println(" ########## MINUTO B11 #########");
  Serial.println("Usable temperature range of brass bloc: " + String(MIN_TEMP) + " to " + String(MAX_TEMP));
  Serial.print("ADC Resolution: "+ String(ADC_RESOLUTION) + " bits"); Serial.print("    ADC steps: "); Serial.println(String(ADC_STEPS));
  Serial.print("Measurement time interval set to : " + String(measurement_duration/1000.0) + " sec\n");  
  Serial.println("Waiting some time before measurement start");
  Serial.println("Type: ");
  Serial.println(" 'list' to display EEPROM contents,");
  Serial.println(" 'stop' to cancel the measurement automatic start.");
  Serial.println(" 'start' to launch it.");
  draw_string_centered("Waiting");
  do
  {
    measurement_start_time = millis();   // Time at which the waiting timer has started
    delta_time = millis() - measurement_start_time;   // initialize the delta time to the timer start

    while( delta_time < WAIT_TIME_BEFORE_START )  // As long as the waiting time is not exceeded, and no command entered, the command input is possible
    {
        delta_time = millis() - measurement_start_time;
        handleSerialInput(); // Check for serial input without blocking
        if(delta_time % DELAY_BETWEEN_SCREEN_UPDATE < 5) draw_value_in_bar( delta_time , WAIT_TIME_BEFORE_START);   // refresh the screen not at each loop, with a margin of 5        
    }
  }
  while(go_start!=1);  // the start is not authorized, keep waiting for a command.

  Serial.println("Now starting the measurement process. Stop it by power cut.");
  Serial.println("Impact of ADC resolution on irradiance precision is about: " + String((int)(BLOC_MASSE*BLOC_CAPA*abs(THERM_LIN_COEFF_A1)*MAX_VOLTAGE_ADC/ADC_STEPS/BLOC_SURFACE/measurement_duration*1000)) + "W/m2" );
  display.clearDisplay();
  display.display();  
  Serial.println("Erasing the EEPROM data ...");
  draw_string_centered("Erasing");
  EraseEEPROM();  // filled with 0xFF
  display.clearDisplay();
  draw_string_centered("Measure");
  previous_temp = calculate_temperature(analogRead(analogPin));   // initialize the value to the current temperature
  previous_time = millis();   // put the reference time just before going into the loop to have the first measurement correct 
  initial_time = millis();    // to get the total time since the begin of the measurement 
}





////////////////////////////

//          LOOP          //

////////////////////////////
void loop() 
{
  delay(DELAY_BETWEEN_SCREEN_UPDATE);  // The bar progression refresh interval 

  // update the time bar
  current_time = millis();
  draw_value_in_bar( current_time - previous_time , measurement_duration );  // draw the bar, max is set to the total measurement time

  if ( (current_time - previous_time) > measurement_duration )     // now the time interval is reached, do the measurement and calculation and display
  {
    // get values
    //current_temp = calculate_temperature((int)random(1024));     // fake value for test without hardware 
    ADC_value = analogRead(analogPin);
    // Serial.println("ADC_value : " + String(ADC_value));
    current_temp = calculate_temperature(ADC_value);   // Real value measured

    if( (current_temp > MIN_TEMP) && (current_temp < MAX_TEMP) )    // This is the possible temperature range where the amplifier voltage is usable
    {
      current_time = millis();
      // calculate the irrandiance
      irradiance = calculate_irradiance(current_temp, current_time);
      if(irradiance != -1)    // the value is -1 if the DeltaT is negative (cooling) if not in debug mode. See calculate_irradiance function
      {
        measurement_counter += 1;   // used to have the time elapsed since the begin of the temperature rise of the bloc. If cooling, the time is no more correlated with counter.
          Serial.print("Measurement ");
          Serial.print(measurement_counter);
            EEPROM.put((int)(measurement_counter*sizeOfDataLine-sizeOfDataLine), (int)((current_time-initial_time)/1000) ); // go back to start of data group
          Serial.print(" : ");
          Serial.print(String(irradiance)); Serial.println(" W/m2");
            EEPROM.put((int)((measurement_counter*sizeOfDataLine)-2*sizeof(float)-sizeof(int)),irradiance);   // go back to start of irradiance
          Serial.print("    Delta T : ");
          Serial.print(current_temp - previous_temp);
            EEPROM.put((int)((measurement_counter*sizeOfDataLine)-2*sizeof(float)), (current_temp - previous_temp)); // go back to start of deltaT
          Serial.print(" K    /    Delta t : ");
          Serial.print(current_time - previous_time); 
          Serial.print(" ms"); 
          Serial.print("     /     Temperature : ");
          Serial.print(current_temp);
            EEPROM.put((int)((measurement_counter*sizeOfDataLine)-sizeof(float)), current_temp);  // go back to start of Temp
          Serial.println(" degree");
          // Display on screen
          draw_string_centered(String(irradiance));          // Display the value below the time bar
      }
      else  // the DeltaT is negative, so the bloc is not stabilized in temperature. 
      {     // If there would be no exchange of the bloc with ambient air, the temperature would never go down, but this is not real world.
          draw_string_centered("Cooling");          // inform user
          Serial.println("The bloc is cooling. No measurement possible yet.");
      }
      // store for next delta T and delta t calculation
      previous_temp = current_temp;
      previous_time = current_time;
    }
    else
    {
      current_time = millis();
      if(current_temp<= MIN_TEMP) draw_string_centered("TooLow");   // put a string with less or equal than 7 chars on display
      if(current_temp>= MAX_TEMP) draw_string_centered("TooHigh");
      Serial.print("The temperature is "); Serial.print(current_temp); Serial.println(" and is out of the functional range");
      Serial.println("Skipping measurement\n");
      // Update time and temp values
      previous_temp = current_temp;
      previous_time = current_time;      
    }
  }
}
