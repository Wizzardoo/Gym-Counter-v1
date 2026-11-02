#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TM1637Display.h>
#include <Preferences.h>

#include "config.h"

//  NTP Time Settings 
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;      // Change this to your timezone offset in seconds (e.g., 3600 for UTC+1)
const int   daylightOffset_sec = 3600;  // Daylight saving offset in seconds (0 if none)

//  Pin Definitions 
#define BTN_TOGGLE_CLOCK  32
#define BTN_TOGGLE_SCREEN 33
#define BTN_RETURN        25
#define BTN_NEXT          26
#define BTN_CONFIRM       27

#define TM1637_CLK        22
#define TM1637_DIO        23

#define OLED_SDA          21
#define OLED_SCK          18
#define SCREEN_WIDTH      128
#define SCREEN_HEIGHT     64

// Display Objects 
TM1637Display tm(TM1637_CLK, TM1637_DIO);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Variables 
int gym_counter = 0;
bool clock_enabled = true;
bool screen_enabled = true;

// Weather variables
float current_temp = 0.0;
int weather_code = 0;
unsigned long previousMillis = 0; // for timing the interval when the capture is made

// Today's weather at certain times
float today_8am_temp = 19.0;
int   today_8am_code = 1;
float today_2pm_temp = 25.0;
int   today_2pm_code = 12;
float today_8pm_temp = 12.0;
int   today_8pm_code = 0;

// Today's weather at certain times
float tomorrow_8am_temp  = 0;
int   tomorrow_8am_code  = 0;
float tomorrow_2pm_temp  = 0;
int   tomorrow_2pm_code  = 0;
float tomorrow_8pm_temp  = 0;
int   tomorrow_8pm_code  = 0;

// Days of the week
const char* daysOfWeek[] PROGMEM = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Idk"};
struct tm timeinfo;
int day_of_the_week_index = 7;
// Date
char dateStr[10];           // store today's date as a str
char tomorrowDateStr[10];   // store tomorrow's date as a str

// Alarms
typedef struct{
  int is_set = 0; // not created by default
  int hour = 0;
  int minute = 0;
  int day_table[7] = { 0, 0, 0, 0, 0, 0, 0 };
  int repeatable = 0; // felt cute, might add later
} alarm_t;

alarm_t alarms[6];
int alarms_index = 0; // index for the alarms expanded screen, 1 default screen + 6 alarm selected screens
int alarm_edit_part_index = 0;   // this index is for the selection of the part we want to edit of the alarm (hours(2), minutes(2), day of the week(7), confirm, delete) = 13 
int alarm_is_editing_part = 0;   // flag that tells to the alarm edit screen we want to chnage the current value highlighted/selected
char alarm_days_set_dict[7] = { 'M','T','W','T','F','S','S' }; // small dictionary that is used for the ALARM_EXPANDED screen to sho won what days the alarm is active
Preferences preferences;

// Button debounce, last state recorded. With internal pullup, not presed means HIGH, pressed reads low
bool last_clock_btn_state = HIGH;
bool last_screen_btn_state = HIGH;
bool last_next_btn_state = HIGH;
bool last_return_btn_state = HIGH;
bool last_confirm_btn_state = HIGH;

#define IMAGE_WIDTH  24
#define IMAGE_HEIGHT 24

// 24 x 24 monochrome, 3 bytes per row, 72 bytes, horizontal rows, MSB first
const unsigned char sun_bitmap_24[] PROGMEM = {
  0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x18, 0x00, 0x04,
  0x18, 0x20, 0x02, 0x18, 0xC0, 0x03, 0x81, 0x80, 0x01, 0x3C, 0x80, 0x00, 0x7E, 0x00, 0x00, 0xFF,
  0x00, 0xFE, 0xFF, 0x70, 0x0E, 0xFF, 0x7F, 0x00, 0xFF, 0x00, 0x00, 0x7E, 0x00, 0x01, 0x3C, 0x80,
  0x01, 0x81, 0xC0, 0x03, 0x18, 0x40, 0x04, 0x18, 0x20, 0x00, 0x18, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x10, 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00
};

const unsigned char cloud_bitmap_24[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x7C, 0x00, 0x00, 0xF6, 0x00, 0x01, 0xFB, 0x00, 0x01, 0xFD, 0x00, 0x0F, 0xFD, 0xB8, 0x3B, 0xFF,
  0xF4, 0x7D, 0xFF, 0xFA, 0x7E, 0xFF, 0xFA, 0x7F, 0xFF, 0xFE, 0x3F, 0xFF, 0xFC, 0x0F, 0x7F, 0x78,
  0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char rain_bitmap_24[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x7C, 0x00, 0x00, 0xF6, 0x00, 0x01, 0xFB, 0x00, 0x01, 0xFD, 0x00, 0x0F, 0xFD, 0xB8, 0x3B, 0xFF,
  0xF4, 0x7D, 0xFF, 0xFA, 0x7E, 0xFF, 0xFA, 0x7F, 0xFF, 0xFE, 0x3F, 0xFF, 0xFC, 0x0F, 0x7F, 0x78,
  0x00, 0x3E, 0x00, 0x06, 0x00, 0x18, 0x0C, 0x63, 0x30, 0x18, 0xC6, 0x60, 0x01, 0x8C, 0xC0, 0x03,
  0x18, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00
};

const unsigned char tick_bitmap_6[] PROGMEM = {
  0x00, 0x04, 0x88, 0x50, 0x20, 0x00
};

const unsigned char tick_inverted_bitmap_8[] PROGMEM = {
  0xFF, 0xFF, 0xFD, 0xBB, 0xD7, 0xEF, 0xFF, 0xFF
};

// Screen macros for drawing information
// Gym Counter
#define SCREEN_MAIN_PART_GYMC( colour_int){ \
  if (colour_int == 1) {                    \
    display.setTextColor(BLACK, WHITE);     \
    display.fillRect(0, 0, 64, 48, WHITE);  \
  } else {                                  \
    display.setTextColor(WHITE, WHITE);     \
  }                                         \
  display.setTextSize(1);                   \
  display.setCursor(22, 6);                 \
  display.print("Gym");                     \
  display.setCursor(12, 16);                \
  display.print("Counter");                 \
  display.setTextSize(2);                   \
  display.setCursor(26, 30);                \
  display.printf("%d", gym_counter);        }

#define SCREEN_PART_WEATHER_CODE_BITMAP( weather_code_int,  coord_x,  coord_y,  colour_int) {                               \ 
  if (weather_code_int == 0) {                                                                                              \
    display.drawBitmap(coord_x, coord_y, sun_bitmap_24, IMAGE_WIDTH, IMAGE_HEIGHT, (colour_int ? BLACK : WHITE)  );         \
  } else if (weather_code_int >= 1 && weather_code_int <= 3) {                                                              \
    display.drawBitmap(coord_x, coord_y, cloud_bitmap_24, IMAGE_WIDTH, IMAGE_HEIGHT, (colour_int ? BLACK : WHITE));         \
  } else {                                                                                                                  \
    display.drawBitmap(coord_x, coord_y, rain_bitmap_24, IMAGE_WIDTH, IMAGE_HEIGHT, (colour_int ? BLACK : WHITE) );         \
  }                                                                                                                         \
}

// Weather
#define SCREEN_MAIN_PART_WEATHER( colour_int){                                                                    \
  if (colour_int == 1) {                                                                                          \
    display.setTextColor(BLACK, WHITE);                                                                           \
    display.fillRect(65, 0, 62, 48, WHITE);                                                                       \
  } else {                                                                                                        \
    display.setTextColor(WHITE, WHITE);                                                                           \
  }                                                                                                               \
  /* Weather txt */                                                                                               \
  display.setTextSize(1);                                                                                         \
  display.setCursor(77, 6);                                                                                       \
  display.print("Weather");                                                                                       \
  /* Now */                                                                                                       \  
  display.setTextSize(2);                                                                                         \    
  display.setCursor(72, 18);                                                                                      \
  display.printf("%d", (int)current_temp);                                                                        \
  display.setTextSize(1);                                                                                         \   
  display.setCursor(70, 36);                                                                                      \  
  int point_left = int(current_temp * 10) % 10;                                                                   \
  display.printf(".%d C", point_left);                                                                            \                    
  /* Weather conditions*/                                                                                         \
  display.setCursor(70, 36);                                                                                      \ 
  SCREEN_PART_WEATHER_CODE_BITMAP(weather_code, 100, 18, colour_int);                                             \
}

#define SCREEN_MAIN_PART_DAYOWEEK             {               \
  display.setTextColor(WHITE, WHITE);                         \
  display.setTextSize(1);                                     \
  display.setCursor(2, 52);                                   \
  display.printf("%s", daysOfWeek[day_of_the_week_index]);    \
}

#define SCREEN_MAIN_PART_ALARM(colour_int){         \
  if (colour_int == 1) {                            \
    display.setTextColor(BLACK, WHITE);             \
    display.fillRect(84, 48, 128, 16, WHITE);       \
  } else {                                          \
    display.setTextColor(WHITE, WHITE);             \
  }                                                 \
  /* Alarm */                                       \
  display.setTextSize(1);                           \
  display.setCursor(89, 52);                        \     
  display.print("Alarms");                          \
}

#define SCREEN_MAIN_PART_DATE(colour_int){                    \
  if (colour_int == 1) {                                      \
    display.setTextColor(BLACK, WHITE);                       \
    display.fillRect(24, 48, 60, 16, WHITE);                  \
  } else {                                                    \
    display.setTextColor(WHITE, WHITE);                       \
  }                                                           \
  display.setTextSize(1);                                     \
  display.setCursor(31, 52);                                  \     
  display.print(dateStr);                                     \
}


// Actual screens that use the macros above as parts of their screens
void draw_SCREEN_MAIN(int part_gymc, int part_weath, int part_date, int part_alarm){          \
  /* Middle line screen separator */                                                          \
  display.drawLine(64, 0, 64, 47, WHITE);                                                     \
  display.drawLine(24, 48, 24, 63, WHITE);                                                    \
  display.drawLine(84, 48, 84, 63, WHITE);                                                    \
  SCREEN_MAIN_PART_GYMC(part_gymc)                                                            \
  SCREEN_MAIN_PART_WEATHER(part_weath)                                                        \
  SCREEN_MAIN_PART_DAYOWEEK                                                                   \
  SCREEN_MAIN_PART_DATE(part_date)                                                            \
  SCREEN_MAIN_PART_ALARM(part_alarm)                                                          \
}

void draw_SCREEN_GYMC_EXPANDED_MACRO() {            \
  display.setTextSize(1);                           \    
  display.setCursor(0,0);                           \
  display.print("gym counter confirm screen temp"); \
}

// day is either today=0 or tomorrow=1
void draw_SCREEN_WEATHER_EXPANDED(int day) {                                                        \
  display.setTextSize(1);                                                                           \    
  display.setCursor(2, 2);                                                                          \
  if (day == 0){                                                                                    \
    display.printf("Today : %s %s", daysOfWeek[day_of_the_week_index], dateStr);                    \
  } else {                                                                                          \
    display.printf("Tmrw  : %s %s", daysOfWeek[ (day_of_the_week_index+1) % 7 ], tomorrowDateStr);  \
  }                                                                                                 \
  /* 8:00 or 8am */                                                                                 \
  display.setTextSize(1);                                                                           \    
  display.setCursor(6, 14);                                                                         \
  display.print("8am");                                                                             \
  display.setTextSize(1);                                                                           \    
  display.setCursor(2, 54);                                                                         \
  if (day == 0){                                                                                    \
    display.printf("%.1f C", today_8am_temp);                                                       \
    SCREEN_PART_WEATHER_CODE_BITMAP(today_8am_code, 2, 23, 0)                                       \
  } else {                                                                                          \
    display.printf("%.1f C", tomorrow_8am_temp);                                                    \
    SCREEN_PART_WEATHER_CODE_BITMAP(tomorrow_8am_code, 2, 23, 0)                                    \
  }                                                                                                 \
  /* 14:00 or 2pm */                                                                                \
  display.setTextSize(1);                                                                           \    
  display.setCursor(56, 14);                                                                        \
  display.print("2pm");                                                                             \
  display.setTextSize(1);                                                                           \   
  display.setCursor(48, 54);                                                                        \
  if ( day == 0 ){                                                                                  \
    display.printf("%.1f C", today_2pm_temp);                                                       \
    SCREEN_PART_WEATHER_CODE_BITMAP(today_2pm_code, 52, 23, 0)                                      \
  } else {                                                                                          \
    display.printf("%.1f C", tomorrow_2pm_temp);                                                    \
    SCREEN_PART_WEATHER_CODE_BITMAP(tomorrow_2pm_code, 52, 23, 0)                                   \
  }                                                                                                 \
  /* 20:00 or 8pm */                                                                                \
  display.setTextSize(1);                                                                           \    
  display.setCursor(102, 14);                                                                       \
  display.print("8pm");                                                                             \
  display.setTextSize(1);                                                                           \  
  display.setCursor(92, 54);                                                                        \
  if (day == 0){                                                                                    \
    display.printf("%.1f C", today_8pm_temp);                                                       \
    SCREEN_PART_WEATHER_CODE_BITMAP(today_8pm_code, 100, 23, 0)                                     \
  } else {                                                                                          \
    display.printf("%.1f C", tomorrow_8pm_temp);                                                    \
    SCREEN_PART_WEATHER_CODE_BITMAP(tomorrow_8pm_code, 100, 23, 0)                                  \
  }                                                                                                 \
}  


// page_selected =0 => page 1 ; =1 => page 2
void draw_SCREEN_ALARMS_EXPANDED(int alarm_selected) {               
  int page_selected = (alarm_selected >= 4);
  display.drawLine(0, 16, 128, 16, WHITE);                                              
  display.drawLine(0, 32, 128, 32, WHITE);      
                                        
  /* Draw a fillRect if an alarm is selected*/                                                  
  if (alarm_selected > 0){                                       
    display.setTextColor(BLACK, WHITE);                                                 
    display.fillRect(0, 16*((alarm_selected-1)%3), 128, 16, WHITE);                                             
  } else {                                                                              
    display.setTextColor(WHITE, WHITE);                                                 
  }           

  // set the appropriate text colour                                                                          
  if (alarm_selected % 3 == 1){
      display.setTextColor(BLACK, WHITE);
  } else{
    display.setTextColor(WHITE);
  }
  // depending on the page we either draw alarms 1 to 3, or 4 to 6
  if (alarms[(page_selected == 1 ? 4 : 1)-1].is_set == 1) {                                           
    display.setTextSize(1);                                                             
    display.setCursor(8,5);                                                            
    display.printf("%02d:%02d", alarms[(page_selected == 1 ? 4 : 1)-1].hour, alarms[(page_selected == 1 ? 4 : 1)-1].minute);
    for(int i = 0; i < 7; i++){
      display.setCursor(58 + i*10, 5);
      display.printf("%c", alarms[(page_selected == 1 ? 4 : 1)-1].day_table[i] == 1 ? alarm_days_set_dict[i] : '-');
    }                                
  } else {                                                                              
    display.setTextSize(1);                                                             
    display.setCursor(62, 4);                                                           
    display.print("+");                                                                 
  }                                                                                     

  if (alarm_selected % 3 == 2){
    display.setTextColor(BLACK, WHITE);
  }else{
    display.setTextColor(WHITE);
  }
  if (alarms[(page_selected == 1 ? 5 : 2)-1].is_set == 1) {                                           
    display.setTextSize(1);                                                             
    display.setCursor(8, 21);                                                            
    display.printf("%02d:%02d", alarms[(page_selected == 1 ? 5 : 2)-1].hour, alarms[(page_selected == 1 ? 5 : 2)-1].minute);
    for(int i = 0; i < 7; i++){
      display.setCursor(58 + i*10, 21);
      display.printf("%c", alarms[(page_selected == 1 ? 5 : 2)-1].day_table[i] == 1 ? alarm_days_set_dict[i] : '-');
    }                        
  } else {                                                                              
    display.setTextSize(1);                                                             
    display.setCursor(62, 20);                                                           
    display.print("+");                                                                 
  }  
                
  if ((alarm_selected  == 6) or (alarm_selected  == 3)) {
    display.setTextColor(BLACK, WHITE);
  }else{
    display.setTextColor(WHITE);
  }                                                                
  if (alarms[(page_selected == 1 ? 6 : 3)-1].is_set == 1) {                                           
    display.setTextSize(1);                                                          
    display.setCursor(8, 37);                                                        
    display.printf("%02d:%02d", alarms[(page_selected == 1 ? 6 : 3)-1].hour, alarms[(page_selected == 1 ? 6 : 3)-1].minute);
    for(int i = 0; i < 7; i++){
      display.setCursor(58 + i*10, 37);
      display.printf("%c", alarms[(page_selected == 1 ? 6 : 3)-1].day_table[i] == 1 ? alarm_days_set_dict[i] : '-');
    }                          
  } else {                                                                           
    display.setTextSize(1);                                                          
    display.setCursor(62, 36);                                                        
    display.print("+");                                                              
  }         

  /* Bottom part */                                                                   
  display.setTextColor(WHITE);                                                        
  display.setCursor(2, 52);                                                            
  display.printf("page %d/2 ", page_selected);                                           
  display.setCursor(82, 52);                                                          
  display.printf("%s", (alarms[alarm_selected-1].is_set == 1 ? "edit" : "create"));       
}

void draw_SCREEN_ALARMS_EDIT(int alarm_selected, int part_selected) {                   
  display.setTextColor(WHITE);                                                          
  display.setTextSize(1);                                                               
  display.setCursor(10, 6);                                                               
  display.print("Set alarm to:");
  if (part_selected == 1){
    display.setTextColor(BLACK);
    display.fillRect(89, 4, 13, 10, WHITE);
    if ( alarm_is_editing_part ) display.drawRect(87, 2, 17, 14, WHITE);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(90, 6);
  display.printf("%02d:", alarms[alarm_selected-1].hour);

  if (part_selected == 2){
    display.setTextColor(BLACK);
    display.fillRect(106, 4, 8, 10, WHITE);
    if ( alarm_is_editing_part ) display.drawRect(104, 2, 12, 14, WHITE);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(108, 6); 
  display.printf("%d", (alarms[alarm_selected-1].minute) / 10);

  if (part_selected == 3){
    display.setTextColor(BLACK);
    display.fillRect(113, 4, 8, 10, WHITE);
    if ( alarm_is_editing_part ) display.drawRect(111, 2, 12, 14, WHITE);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(114, 6);
  display.printf("%d", (alarms[alarm_selected-1].minute) % 10);

  display.setTextColor(WHITE);
  display.setCursor(4, 23);
  display.print("Mo Tu We Th Fr Sa Su");
  //display.setCursor(2, 32);
  for (int i = 0; i < 7; i++){
    if ( alarms[alarm_selected-1].day_table[i] == 1){
      //display.setCursor(6 + (i*8), 26);
      if (part_selected == 4+i){
        display.drawBitmap(5+(i*18), 38, tick_inverted_bitmap_8, 8, 8, WHITE);
        if ( alarm_is_editing_part ) display.drawRect(3+(i*18), 36, 12, 12, WHITE);
      } else {
        display.drawBitmap(7+(i*18), 39, tick_bitmap_6, 6, 6, WHITE);
      }
    } else {
      // no tick
      if (part_selected == 4+i){
        display.fillRect(5+(i*18), 38, 8, 8, WHITE);
        if ( alarm_is_editing_part ) display.drawRect(3+(i*18), 36, 12, 12, WHITE);
      }
    }
  }

  // bottom part
  if (part_selected == 11){
    display.setTextColor(BLACK);
    display.fillRect(0, 48, 64, 16, WHITE);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(12, 53);
  display.print("Confirm");
  
  display.drawLine(64, 48, 64, 64, WHITE);
  if (part_selected == 12){
    display.setTextColor(BLACK);
    display.fillRect(64, 48, 64, 16, WHITE);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(72, 53);
  display.print("Discard");
}

// Screen States
enum ScreenState {
  SCREEN_MAIN,
  SCREEN_GYMC_SELECTED,
  SCREEN_GYMC_CONFIRM,
  SCREEN_WEATHER_SELECTED,
  SCREEN_WEATHER_EXPANDED_TODAY,
  SCREEN_WEATHER_EXPANDED_TOMORROW,
  SCREEN_DATE_SELECTED,
  SCREEN_OPTIONS,
  SCREEN_ALARMS_SELECTED,
  SCREEN_ALARMS_EXPANDED,
  SCREEN_ALARMS_EDIT
};

ScreenState current_screen = SCREEN_MAIN;

void setup() {
  Serial.begin(115200);

  // Initialize buttons with internal pull-ups
  pinMode(BTN_TOGGLE_CLOCK, INPUT_PULLUP);
  pinMode(BTN_TOGGLE_SCREEN, INPUT_PULLUP);
  pinMode(BTN_RETURN, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_CONFIRM, INPUT_PULLUP);

  // Initialize TM1637 Brightness (0x00 to 0x0f)
  tm.setBrightness(0x02);

  // Initialize I2C OLED on custom pins (SDA: 21, SCK: 18)
  Wire.begin(OLED_SDA, OLED_SCK);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");

  // Initialize NTP time sync
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  loadAlarms();

  // debug set alarm
  /*alarms[1].is_set = 1;
  alarms[1].hour = 12;
  alarms[1].minute = 54;
  alarms[1].day_table[0] = 1;
  alarms[1].day_table[1] = 1;
  alarms[1].day_table[2] = 1;
  alarms[1].day_table[3] = 1;
  alarms[1].day_table[4] = 1;
  alarms[1].day_table[5] = 1;
  alarms[1].day_table[6] = 1;*/
}

void loop() {
  
  // 1. Check all button inputs
  handleButtons();

  // 2. Fetch current time
  
  bool timeValid = getLocalTime(&timeinfo);

  // 3. Update TM1637 7-Segment Display (Shows Time HH:MM)
  if (timeValid) {
    // record the day of the week
    day_of_the_week_index = timeinfo.tm_wday;
    strftime(dateStr, sizeof(dateStr), "%d/%m/%y", &timeinfo);
    
    if (clock_enabled){
      int displayValue = (timeinfo.tm_hour * 100) + timeinfo.tm_min;
      // Show number with the middle colon enabled (0b01000000)
      tm.showNumberDecEx(displayValue, 0b01000000, true);
    } else {
      tm.clear();
    }
    
  } else {
    Serial.println("Error while reading time");
  }

  // fetch the weather every 30 minutes
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 1800000){ 
    previousMillis = currentMillis;
    fetchWeatherData();
    // Placeholder
    //current_temp = 19.5;
    //weather_code = 0;
  }

  // 4. Update OLED Display
  if (screen_enabled) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.setRotation(2);
    display.clearDisplay();
    
    switch(current_screen){
      case SCREEN_MAIN:
        draw_SCREEN_MAIN(0,0,0,0);
        break;
      case SCREEN_GYMC_SELECTED:
        draw_SCREEN_MAIN(1,0,0,0);
        break;
      case SCREEN_WEATHER_SELECTED:
        draw_SCREEN_MAIN(0,1,0,0);
        break;
      case SCREEN_DATE_SELECTED:
        draw_SCREEN_MAIN(0,0,1,0);
        break;
      case SCREEN_ALARMS_SELECTED:
        draw_SCREEN_MAIN(0,0,0,1);
        break;
      
      case SCREEN_WEATHER_EXPANDED_TODAY:
        // today
        draw_SCREEN_WEATHER_EXPANDED(0);
        break;
      case SCREEN_WEATHER_EXPANDED_TOMORROW:
        // tomorrow
        calculateTomorrowDate();
        draw_SCREEN_WEATHER_EXPANDED(1);
        break;

      // Alarms
      case SCREEN_ALARMS_EXPANDED:
        draw_SCREEN_ALARMS_EXPANDED(alarms_index);
        break;
      case SCREEN_ALARMS_EDIT:
        draw_SCREEN_ALARMS_EDIT(alarms_index, alarm_edit_part_index);
        break;
    }

    display.display();

  } else {
    // Put OLED into sleep/off mode to save power
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  delay(100); // Small loop delay for responsiveness
}

void handleButtons() {
  //  Toggle Clock Display (GPIO 32) 
  bool current_clock_btn = digitalRead(BTN_TOGGLE_CLOCK);
  if (current_clock_btn == LOW && last_clock_btn_state == HIGH) {
    clock_enabled = !clock_enabled;
    delay(200); // Simple debounce
  }
  last_clock_btn_state = current_clock_btn;

  //  Toggle Screen Display (GPIO 33) 
  bool current_screen_btn = digitalRead(BTN_TOGGLE_SCREEN);
  if (current_screen_btn == LOW && last_screen_btn_state == HIGH) {
    screen_enabled = !screen_enabled;
    delay(200);
  }
  last_screen_btn_state = current_screen_btn;

    //  BTN_RETURN: Return to the main screen (GPIO 25) 
  bool current_btn_return = digitalRead(BTN_RETURN);
  if (current_btn_return == LOW && last_return_btn_state == HIGH) {
    if (current_screen == SCREEN_ALARMS_EDIT){
      current_screen = SCREEN_ALARMS_EXPANDED;
      alarm_edit_part_index = 0;
    } else {
      alarms_index = 0;
      current_screen = SCREEN_MAIN;
    }
    delay(200);
  }
  last_return_btn_state = current_btn_return;

    //  BTN_NEXT: Move to the next option on screen (GPIO 26) 
  bool current_btn_next = digitalRead(BTN_NEXT);
  if (current_btn_next == LOW && last_next_btn_state == HIGH) {
    switch (current_screen){
      case SCREEN_MAIN:
        current_screen = SCREEN_GYMC_SELECTED;
        break;
      case SCREEN_GYMC_SELECTED:
        current_screen = SCREEN_WEATHER_SELECTED;
        break;
      case SCREEN_WEATHER_SELECTED:
        current_screen = SCREEN_DATE_SELECTED;
        break;
      case SCREEN_DATE_SELECTED:
        current_screen = SCREEN_ALARMS_SELECTED;
        break;
      case SCREEN_ALARMS_SELECTED:
        current_screen = SCREEN_GYMC_SELECTED;
        break;
      
      // Weather
      case SCREEN_WEATHER_EXPANDED_TODAY:
        current_screen = SCREEN_WEATHER_EXPANDED_TOMORROW;
        break;
      case SCREEN_WEATHER_EXPANDED_TOMORROW:
        current_screen = SCREEN_WEATHER_EXPANDED_TODAY;
        break;

      // Alarms
      case SCREEN_ALARMS_EXPANDED:
        alarms_index = (alarms_index + 1) % 7 ;
        break;
      case SCREEN_ALARMS_EDIT:
        if (alarm_is_editing_part == 1){
          //get value and increment it
          get_value_and_increment();
        }else{
          alarm_edit_part_index = (alarm_edit_part_index + 1) % 13;
        }
        break;
    }
    delay(200);
  }
  last_next_btn_state = current_btn_next;

      //  BTN_CONFIRM: Accept the currently selected option on screen or go to the gym counter if on main screen (GPIO 26) 
  bool current_btn_confirm = digitalRead(BTN_CONFIRM);
  if (current_btn_confirm == LOW && last_confirm_btn_state == HIGH) {
    switch (current_screen){
      case SCREEN_WEATHER_SELECTED:
        current_screen = SCREEN_WEATHER_EXPANDED_TODAY;
        break;

      case SCREEN_ALARMS_SELECTED:
        current_screen = SCREEN_ALARMS_EXPANDED;
        break;
      case SCREEN_ALARMS_EXPANDED:
        // we go into alarm edit obly if we have selected an alarm. When alarm_index == 0, no alarm is selected 
        if (alarms_index != 0) current_screen = SCREEN_ALARMS_EDIT;
        break;
      case SCREEN_ALARMS_EDIT:
        // if we finished editing a value we skip to the next one, exception we pressed onto "confirm" or "discard"
        if ((alarm_is_editing_part == 1) and (alarm_edit_part_index < 12)){
          //alarm_edit_part_index = (alarm_edit_part_index + 1) % 13;
          alarm_is_editing_part = 0;
        }
        else if (alarm_edit_part_index == 11){
          //confirm save and exit
          alarms[alarms_index-1].is_set = 1;
          //save
          saveAlarms();
          //exit
          alarm_edit_part_index = 0;
          current_screen = SCREEN_ALARMS_EXPANDED;
        }
        else if (alarm_edit_part_index == 12){
          //overwrite everything with 0 and SAVE
          alarms[alarms_index-1].is_set = 0;
          alarms[alarms_index-1].hour = 0;
          alarms[alarms_index-1].minute = 0;
          alarms[alarms_index-1].day_table[0] = 0;
          alarms[alarms_index-1].day_table[1] = 0;
          alarms[alarms_index-1].day_table[2] = 0;
          alarms[alarms_index-1].day_table[3] = 0;
          alarms[alarms_index-1].day_table[4] = 0;
          alarms[alarms_index-1].day_table[5] = 0;
          alarms[alarms_index-1].day_table[6] = 0;
          //save
          saveAlarms();
          //exit
          alarm_edit_part_index = 0;
          current_screen = SCREEN_ALARMS_EXPANDED;
        }
        else if (alarm_is_editing_part == 0){
          alarm_is_editing_part = !alarm_is_editing_part;
        }
        else{
          Serial.println("Error while alarm editing");
        }
        break; 
      
    }
    delay(200);
  }
  last_confirm_btn_state = current_btn_confirm;
}

void fetchWeatherData(){
  if (WiFi.status() == WL_CONNECTED){
    HTTPClient http;

    // Construct Open-Meteo API URL for current weather & daily forecast
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE, 4.0) +
                 "&longitude=" + String(LONGITUDE, 4.0) +
                 "&current=temperature_2m,weather_code" +
                 "&daily=temperature_2m_mean,weather_code&timezone=auto" + 
                 "&hourly=temperature_2m,weather_code";

    http.begin(url);
    int httpResponseCode = http.GET();

    if ( httpResponseCode > 0 ){
      String payload = http.getString();

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if(!error){
        //Get the weather from the doc
        current_temp = doc["current"]["temperature_2m"];
        weather_code = doc["current"]["weather_code"];

        today_8am_temp = doc["hourly"]["temperature_2m"][8];
        today_8am_code = doc["hourly"]["weather_code"][8];

        today_2pm_temp = doc["hourly"]["temperature_2m"][14];
        today_2pm_code = doc["hourly"]["weather_code"][14];

        today_8pm_temp = doc["hourly"]["temperature_2m"][20];
        today_8pm_code = doc["hourly"]["weather_code"][20];

        // Tomorrow's brackets (Offset by 24 hours)
        tomorrow_8am_temp = doc["hourly"]["temperature_2m"][32];
        tomorrow_8am_code = doc["hourly"]["weather_code"][32];

        tomorrow_2pm_temp = doc["hourly"]["temperature_2m"][38];
        tomorrow_2pm_code = doc["hourly"]["weather_code"][38];

        tomorrow_8pm_temp = doc["hourly"]["temperature_2m"][44];
        tomorrow_8pm_code = doc["hourly"]["weather_code"][44];

        Serial.println("Weather updated successfully!");
      } else{
        Serial.println("JSON parsing failed!");
      }

    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}
// This function is called to calculate the date when on the SCREEN_WEATHER_EXPANDED_TOMORROW screen
void calculateTomorrowDate(){
    if (getLocalTime(&timeinfo)) {
      struct tm tomorrow_info = timeinfo;

      // Add one day
      tomorrow_info.tm_mday += 1;
      mktime(&tomorrow_info); // Recalculate the data

      strftime(tomorrowDateStr, sizeof(tomorrowDateStr), "%d/%m/%y", &tomorrow_info);
  }
} 

void get_value_and_increment(){
  if (alarm_is_editing_part == 0 or alarm_is_editing_part == 11 or alarm_is_editing_part == 12){
    return;
  }
  //alarm_t current_alarm_edit = alarms[alarms_index-1];
  switch(alarm_edit_part_index){
    case 1:
      //hours
      alarms[alarms_index-1].hour = (alarms[alarms_index-1].hour + 1) % 24;
      break;
    case 2:
      //10 minute increment
      alarms[alarms_index-1].minute = (alarms[alarms_index-1].minute + 10) % 60;
      break;
    case 3:
      //1 minute increment
      alarms[alarms_index-1].minute = (alarms[alarms_index-1].minute + 1) % 60;
    //days of the week toggles
    case 4:
      alarms[alarms_index-1].day_table[0] = !alarms[alarms_index-1].day_table[0];
      break;
    case 5:
      alarms[alarms_index-1].day_table[1] = !alarms[alarms_index-1].day_table[1];
      break;
    case 6:
      alarms[alarms_index-1].day_table[2] = !alarms[alarms_index-1].day_table[2];
      break;
    case 7:
      alarms[alarms_index-1].day_table[3] = !alarms[alarms_index-1].day_table[3];
      break;
    case 8:
      alarms[alarms_index-1].day_table[4] = !alarms[alarms_index-1].day_table[4];
      break;
    case 9:
      alarms[alarms_index-1].day_table[5] = !alarms[alarms_index-1].day_table[5];
      break;
    case 10:
      alarms[alarms_index-1].day_table[6] = !alarms[alarms_index-1].day_table[6];
      break;    
  }
}

// Save alarms to flash memory
void saveAlarms() {
  preferences.begin("alarm-app", false); // Open namespace "alarm-app" in read-write mode
  preferences.putBytes("alarms_data", alarms, sizeof(alarms));
  preferences.end();
  Serial.println("Alarms saved to flash memory!");
}

// Load alarms from flash memory on startup
void loadAlarms() {
  preferences.begin("alarm-app", true); // Open namespace in read-only mode
  
  // Check if the key exists before reading to avoid loading garbage data on first boot
  if (preferences.isKey("alarms_data")) {
    preferences.getBytes("alarms_data", alarms, sizeof(alarms));
    Serial.println("Alarms loaded successfully from flash memory!");
  } else {
    Serial.println("No saved alarms found.");
  }
  
  preferences.end();
}