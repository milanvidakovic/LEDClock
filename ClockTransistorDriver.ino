#include <RTClib.h>
#include <WiFi.h>
#include <HTTPClient.h>

// GPIO ports for LEDs
//            T  A   B   C   D   E   F   G
int LEDS[] = {2, 32, 33, 25, 26, 27, 14, 12};

// GPIO ports for digits
int DIGITS[] = { 18, 19, 17, 16 };

int DIGIT_MATRIX[][7] = {
// A     B     C     D     E     F     G  
{HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, LOW},   // 0
{LOW,  HIGH, HIGH, LOW,  LOW,  LOW,  LOW},   // 1
{HIGH, HIGH, LOW,  HIGH, HIGH, LOW,  HIGH},  // 2
{HIGH, HIGH, HIGH, HIGH, LOW,  LOW,  HIGH},  // 3
{LOW,  HIGH, HIGH,  LOW, LOW,  HIGH, HIGH},  // 4
{HIGH, LOW,  HIGH, HIGH, LOW,  HIGH, HIGH},  // 5
{HIGH, LOW,  HIGH, HIGH, HIGH, HIGH, HIGH},  // 6
{HIGH, HIGH, HIGH, LOW,  LOW,  LOW,  LOW},   // 7
{HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH},  // 8
{HIGH, HIGH, HIGH, HIGH, LOW,  HIGH, HIGH},  // 9
{LOW,  LOW,  LOW,  LOW,  LOW,  LOW,  HIGH},  // '-'  index is 10
{LOW,  LOW,  LOW,  LOW,  LOW,  LOW,  LOW}    // BLANK index is 11
};

int getStatus = 0;
#define HOUR_TO_SYNC 04
#define MIN_TO_SYNC  01

int counter;
int counter_toggle;

// states of the clock
#define TIME     0
#define EXT_TEMP 1
#define INT_TEMP 2
int state;

// four digits of the clock
int hour_1, hour_2;
int minute_1, minute_2;
int toggle;

int localTemp = 0;

RTC_DS3231 rtc;

const char root_ca[]= {
	// HTTPS CERTIFICATE BYTES GO HERE
};

// ####################################################
// TIME SYNCHRONIZATION FUNCTION
// obtains current time from the dedicated server
// ####################################################
void do_sync() 
{
  HTTPClient http;
    
  http.begin("https://xxxxxxxxxxxxx", root_ca); //HTTPS

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if(httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if(httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          Serial.println(payload);
					// ####################################################
					// PARSING GOES HERE
					// ####################################################
          rtc.adjust(DateTime(s_year, s_month, s_day, s_hour, s_minute, s_second));
      }
  } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

int havent_done;

// ####################################################
// obtains time from the RTC and 
// sets hour_1, hour_2, minute_1 and minute_2 variables
// ####################################################
void getTime()
{
  if (state == TIME)
  {
    DateTime now = rtc.now();
    int hour = now.hour();
    int minute = now.minute();

    // if there is something wrong with the time
    if (hour > 24)
    {
      do_sync();
      now = rtc.now();
      hour = now.hour();
      if (hour > 24) 
      {
        ESP.restart();    
      }
    }

    // at a given time, we sync with the server
    if (hour == HOUR_TO_SYNC && minute == MIN_TO_SYNC && havent_done)
    {
        havent_done = 0;
        do_sync();
    } else if (hour == HOUR_TO_SYNC && minute == MIN_TO_SYNC + 1)
    {
      havent_done = 1;
    }

    hour_1 = hour / 10;
    hour_2 = hour % 10;
    minute_1 = minute / 10;
    minute_2 = minute % 10;
  } else if (state == INT_TEMP)
  {
    float int_temp = rtc.getTemperature();
    int temp = (int)int_temp;
    temp = temp -2;
    hour_1 = 11;
    hour_2 = 11;
    minute_1 = temp / 10;
    minute_2 = temp % 10;

    Serial.print("Temperature: ");
    Serial.print(int_temp - 2);
    Serial.println(" C");
    Serial.println();

  }

}

TaskHandle_t Task1;

// ####################################################
// TIME TASK
// obtains time from the RTC every two seconds
// ####################################################
void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    if (getStatus == 0)
      getTime();
    delay(2000);
  } 
}

long TimeOfLastDebounce = 0;  // holds the last time the switch was pressed
long DelayofDebounce = 1000;  // amount of time that needs to be expired between presses

TaskHandle_t Task2;

// ####################################################
// TEMPERATURE TASK
// obtains external temperature information from the server
// ####################################################
void Task2code( void * pvParameters ){
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    delay(100);
    if (getStatus == 1) {
      getStatus = 0;
      HTTPClient http;
    
      http.begin("https://xxxxxxxxxxxxxxxx", root_ca); //HTTPS
    
      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();
    
      // httpCode will be negative on error
      if(httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    
          // file found at server
          if(httpCode == HTTP_CODE_OK) {
              String payload = http.getString();
              Serial.println(payload);
							// ####################################################
              // PARSING of the external temperature GOES HERE
							// ####################################################
          }
      } else {
          Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
    
      http.end();
    }    
  }
}

// ####################################################
// BUTTON SERVICE ROUTINE
// cycles between three states: showing time, 
// showing external temperature and showing internal temperature
// ####################################################
void IRAM_ATTR btn_isr() {
  if ((millis() - TimeOfLastDebounce) > DelayofDebounce) {
    TimeOfLastDebounce = millis();
    Serial.println("BUTTON");

    hour_1 = 10;
    hour_2 = 10;
    minute_1 = 10;
    minute_2 = 10;

    switch(state)
    {
      case TIME:
        getStatus = 1; 
        state = EXT_TEMP;
        break;
      case EXT_TEMP:
        state = INT_TEMP;
        break;
      case INT_TEMP:
        state = TIME;
        break;
    }
  }
}

// ####################################################
// SETUP
// ####################################################
void setup() {
  int i;
  for (i = 0; i < 8; i++) {
    pinMode(LEDS[i], OUTPUT);
  }
  for (i = 0; i < 4; i++) {
    pinMode(DIGITS[i], OUTPUT);
  }
  displayDigit(0);
  activateDigit(0);
 
  havent_done = 1;
  state = TIME;
  Serial.begin(115200);
  delay(2000); // wait for console opening

  displayDigit(1);

  WiFi.begin("SSID", "PASSWORD");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");

  TimeOfLastDebounce = millis();
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(0, btn_isr, FALLING);
  

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  counter = 0;
  toggle = 0;
  counter_toggle = 0;

  displayDigit(2);

  do_sync();

  getStatus = 0; 

  // create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  // this task will get time from RTC every two seconds
  xTaskCreatePinnedToCore(
              Task1code,   /* Task function. */
              "Task1",     /* name of task. */
              10000,       /* Stack size of task */
              NULL,        /* parameter of the task */
              1,           /* priority of the task */
              &Task1,      /* Task handle to keep track of created task */
              0);          /* pin task to core 0 */

                    
  // this task will get outside temperature from my server when a button is pressed for the first time
  xTaskCreatePinnedToCore(
             Task2code,  /* Task function. */
             "Task2",    /* name of task. */
             10000,      /* Stack size of task */
             NULL,       /* parameter of the task */
             1,          /* priority of the task */
             &Task2,     /* Task handle to keep track of created task */
             1);         /* pin task to core 0 */
}

// ####################################################
// SETS LED SEGMENTS TO FORM A GIVEN DIGIT
// ####################################################
void displayDigit(int value)
{
  int j, k;
  for (j = 1; j < 8; j++)
  {
    digitalWrite(LEDS[j], (DIGIT_MATRIX[value][j-1]));
  }
}

// ####################################################
// ACTIVATES A GIVEN DIGIT
// by connecting common cathode to the ground (controlled by the transistor activated by the predefined GPIO)
// ####################################################
void activateDigit(int digit)
{
  switch(digit)
  {
    case 0:
      digitalWrite(DIGITS[0], HIGH);
      digitalWrite(DIGITS[1], LOW);
      digitalWrite(DIGITS[2], LOW);
      digitalWrite(DIGITS[3], LOW);
      digitalWrite(LEDS[0], LOW);
    break;
    case 1:
      digitalWrite(DIGITS[0], LOW);
      digitalWrite(DIGITS[1], HIGH);
      digitalWrite(DIGITS[2], LOW);
      digitalWrite(DIGITS[3], LOW);
      if (toggle & 1 == 1)
        digitalWrite(LEDS[0], HIGH);
      else
        digitalWrite(LEDS[0], LOW);
    break;
    case 2:
      digitalWrite(DIGITS[0], LOW);
      digitalWrite(DIGITS[1], LOW);
      digitalWrite(DIGITS[2], HIGH);
      digitalWrite(DIGITS[3], LOW);
      digitalWrite(LEDS[0], LOW);
    break;
    case 3:
      digitalWrite(DIGITS[0], LOW);
      digitalWrite(DIGITS[1], LOW);
      digitalWrite(DIGITS[2], LOW);
      digitalWrite(DIGITS[3], HIGH);
      digitalWrite(LEDS[0], LOW);
    break;
    default:
      digitalWrite(DIGITS[0], LOW);
      digitalWrite(DIGITS[1], LOW);
      digitalWrite(DIGITS[2], LOW);
      digitalWrite(DIGITS[3], LOW);
      digitalWrite(LEDS[0], LOW);
  }
}

// ####################################################
// MAIN LOOP
// ####################################################
void loop() {
  switch(counter)
  {
    case 0:
      displayDigit(hour_1);
      break;
    case 1:
      displayDigit(hour_2);
      break;
    case 2:
      displayDigit(minute_1);
      break;
    case 3:
      displayDigit(minute_2);
      break;
  }
  activateDigit(counter);
  counter++;
  if (counter == 4)
  {
    counter = 0;
  }

   if (counter_toggle++ == 500)
  {
    toggle = ~toggle;
    counter_toggle = 0;
  }

    // duty cycle adjustment; total cycle is one second, divided by on and off time
    delayMicroseconds(500);
    activateDigit(-1);
    delayMicroseconds(500);
}
