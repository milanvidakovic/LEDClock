#include <FastLED.h>
#include <RTClib.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define LED_PIN     2
#define NUM_LEDS    4*28

CRGB leds[NUM_LEDS + 2];

int DIGIT_IDX_START[] = {0, 28, 56, 84};

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


#define HOUR_TO_SYNC 04
#define MIN_TO_SYNC  01

int counter;

int brightness = 3;

// states of the clock
#define RED       0
#define GREEN     1
#define BLUE      2
#define WHITE     3
#define RAINBOW   4
#define BLACK     5
#define PURPLE    6
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
// getTime function
// ####################################################
void getTime()
{
  if (1)
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
    getTime();
    delay(2000);
  } 
}

long TimeOfLastDebounce = 0;  // holds the last time the switch was pressed
long DelayofDebounce = 500;  // amount of time that needs to be expired between presses

// ####################################################
// DISPLAY ON/OFF BUTTON HANDLER
// ####################################################
void IRAM_ATTR btn_isr() {
  if ((millis() - TimeOfLastDebounce) > DelayofDebounce) {
    TimeOfLastDebounce = millis();
    Serial.println("DISPLAY ON/OFF BUTTON");

    switch (state)
    {
      case BLACK:
        state = BLUE;
        break;
      case BLUE:
        state = BLACK;
        break;
    }
  }
}

// ####################################################
// BRIGHTNESS UP/DOWN HANDLERS
// ####################################################
void IRAM_ATTR brightup_isr() {
  if ((millis() - TimeOfLastDebounce) > DelayofDebounce) {
    TimeOfLastDebounce = millis();
    if (brightness < 250)
      brightness += 1;
    Serial.println(brightness);
  }
}
void IRAM_ATTR brightdn_isr() {
  if ((millis() - TimeOfLastDebounce) > DelayofDebounce) {
    TimeOfLastDebounce = millis();
    if (brightness > 1)
      brightness -= 1;
    Serial.println(brightness);
  }
}

// ####################################################
// SETUP
// ####################################################
void setup() {
  int i;

  // ####################################################
  // LEDS
  // ####################################################
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS + 2);
  FastLED.setBrightness(brightness);
  for (i = 0; i < NUM_LEDS + 2; i++) 
  {
    leds[i] = CRGB(128, 0, 128);
  }
  FastLED.show();
 
  havent_done = 1;
  state = BLUE;
  
  Serial.begin(115200);
  delay(2000); // wait for console opening

  setDigit(0, 1);

  // ####################################################
  // WIFI
  // ####################################################
  WiFi.begin("SSID", "PASSWORD");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");

  // ####################################################
  // DISPLAY ON/OFF BUTTON
  // ####################################################
  TimeOfLastDebounce = millis();
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(0, btn_isr, FALLING);

  // ####################################################
  // BRIGHTNESS BUTTONS
  // ####################################################
  pinMode(12, INPUT_PULLUP);
  attachInterrupt(12, brightup_isr, FALLING);
  pinMode(14, INPUT_PULLUP);
  attachInterrupt(14, brightdn_isr, FALLING);

  // ####################################################
  // RTC
  // ####################################################
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  counter = 0;
  setDigit(0, 2);
  do_sync();

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

                    
}

// ####################################################
// SET A GIVEN DIGIT TO A NUMBER
// ####################################################
void setDigit(int digit, int value)
{
  int i, j;
  for (i = DIGIT_IDX_START[digit]; i < (DIGIT_IDX_START[digit] + 28); i++)
  {
    j = (i - DIGIT_IDX_START[digit]) / 4;                  // j is segment index
    if (DIGIT_MATRIX[value][j])
    {
       switch (state)
      {
        case RED:
          leds[i] = CRGB(255, 0, 0);
          break;
        case GREEN:
          leds[i] = CRGB(0, 255, 0);
          break;
        case BLUE:
          leds[i] = CRGB(0, 0, 255);
          break;
        case WHITE:
          leds[i] = CRGB(255, 255, 255);
          break;
        case RAINBOW:
          leds[i] = CRGB((i % 4 + 1) * 64, (i % 4 + 2) * 64, (i % 4 + 3)* 64);
          break;
        case BLACK:
          leds[i] = CRGB(0, 0, 0);
          break;
         case PURPLE:
          leds[i] = CRGB(128, 0, 128);
          break;
        default:
          leds[i] = CRGB(255, 0, 0);
      }
    }
    else
    {
      leds[i] = CRGB(0, 0, 0);
    }
  }
  FastLED.setBrightness(brightness);
  FastLED.show();
}

// ####################################################
// SETS TWO DOTS
// ####################################################
void setDots(int val)
{
  int i;
  
  if (val == 1)
  {
    for (i = NUM_LEDS; i < NUM_LEDS + 2; i++)
       switch (state)
      {
        case RED:
          leds[i] = CRGB(255, 0, 0);
          break;
        case GREEN:
          leds[i] = CRGB(0, 255, 0);
          break;
        case BLUE:
          leds[i] = CRGB(0, 0, 255);
          break;
        case WHITE:
          leds[i] = CRGB(255, 255, 255);
          break;
        case RAINBOW:
          leds[i] = CRGB((i % 4 + 1) * 64, (i % 4 + 2) * 64, (i % 4 + 3)* 64);
          break;
        case BLACK:
          leds[i] = CRGB(0, 0, 0);
          break;
        case PURPLE:
          leds[i] = CRGB(128, 0, 128);
          break;
        default:
          leds[i] = CRGB(255, 0, 0);
      }
  }
  else 
  {
    leds[NUM_LEDS]     =  CRGB(0, 0, 0);
    leds[NUM_LEDS + 1] =  CRGB(0, 0, 0);
  }
  FastLED.setBrightness(brightness);
  FastLED.show();
}

// ####################################################
// MAIN LOOP
// ####################################################
void loop() {
  setDots(counter % 2);

  setDigit(0, hour_1);
  setDigit(1, hour_2);
  setDigit(2, minute_1);
  setDigit(3, minute_2);
  delay(500);
  setDigit(0, hour_1);
  setDigit(1, hour_2);
  setDigit(2, minute_1);
  setDigit(3, minute_2);
  delay(500);

  counter++;
  if (counter == 2)
  {
    counter = 0;
  }
}
