#include <RTClib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TM1637Display.h>

const int CLK = 17; //Set the CLK pin connection to the display
const int DIO = 16; //Set the DIO pin connection to the display
 
TM1637Display display(CLK, DIO); //set up the 4-Digit Display.

int getStatus = 0;
#define HOUR_TO_SYNC 04
#define MIN_TO_SYNC  01

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

RTC_DS3231 rtc;  // SDA is GPIO21, SCL is GPIO22

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
// getTime function
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

// ####################################################
// TIME TASK
// obtains time from the RTC every two seconds
// ####################################################
TaskHandle_t Task1;

void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    if (getStatus == 0)
      getTime();
    delay(2000);
  } 
}

// ####################################################
// TEMPERATURE TASK
// obtains external temperature information from the server
// ####################################################
long TimeOfLastDebounce = 0;  // holds the last time the switch was pressed
long DelayofDebounce = 1000;  // amount of time that needs to be expired between presses

TaskHandle_t Task2;

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

  // ####################################################
  // DISPLAY
  // ####################################################
  display.setBrightness(0x05); //set the diplay brightness  (0x0a is max)
  
  havent_done = 1;
  state = TIME;
  Serial.begin(115200);
  delay(2000); // wait for console opening

  display.showNumberDec(1);

  // ####################################################
  // WIFI
  // ####################################################
  WiFi.begin("SSID", "PASSWORD");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");

  TimeOfLastDebounce = millis();
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(0, btn_isr, FALLING);
  

  // ####################################################
  // RTC
  // ####################################################
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  toggle = 0;

  display.showNumberDec(2);

  do_sync();

  getStatus = 0; 

  // ####################################################
  // TIME TASK
  // ####################################################
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

                    
  // ####################################################
  // GET TEMPERATURE TASK
  // ####################################################
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
// MAIN LOOP
// ####################################################
void loop() {
  
  // show current time
  display.showNumberDecEx(hour_1*1000 + hour_2*100 + minute_1*10 + minute_2, 0, true);

  // flashing dots code
  toggle = ~toggle;
  if (toggle)
  {
    display.showNumberDecEx(hour_1*1000 + hour_2*100 + minute_1*10 + minute_2, (0x80 >> 1), true);
  } 

  delay(500);
}
