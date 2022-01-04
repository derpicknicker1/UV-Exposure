/*******************************************************************************
(c) 2020 - Florian Schütte
*******************************************************************************/
#include <Adafruit_SSD1306.h>
#include <Button2.h>
#include <ESPRotary.h>

#define VERSION "0.0.1"

/*********************************** RELAY ************************************/
#define RELAY_PIN   16  // D0 (makro D0 does not work?!)

/********************************** ENCODER ***********************************/
#define ROTARY_PIN1 D6
#define ROTARY_PIN2 D5
#define BUTTON_PIN  D7
#define CLICKS_PER_STEP   4
ESPRotary r = ESPRotary(ROTARY_PIN1, ROTARY_PIN2, CLICKS_PER_STEP);
Button2 b = Button2(BUTTON_PIN);

/********************************** DISPLAY ***********************************/
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);
#if (SSD1306_LCDHEIGHT != 48)
#warning("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

/********************************** SETTINGS **********************************/
struct {
  char sw_version[6] = VERSION;
  uint16_t default_time = 600;
  uint16_t min_time = 0;
  uint16_t max_time = 9999;
  uint16_t last_time = 600;
  uint16_t sec_inc = 5;
} settings;

/*********************************** STATES ***********************************/
enum {
  S_START,
  S_EXPO_M,
  S_MANUAL,
  S_SET_TIME,
  S_SET_TIME_DONE,
  S_EXPOSE,
  S_EXPOSE_DONE,
  S_PAUSE,
  S_STOP,
  S_SETTINGS,
  S_SET_DEFAULT,
  S_SET_MIN,
  S_SET_MAX,
  S_INFO
};

/*********************************** MENUS ************************************/
String startMenu[3] = {"Start", "Setup", "Info"};
String expoMenu[3] = {"Auto", "Man.",""};
String settingsMenu[3] = {"Default", "Min", "Max"};
int startMenuLen = sizeof(startMenu)/sizeof(startMenu[0]);
int expoMenuLen = sizeof(expoMenu)/sizeof(expoMenu[0]);
int settingsMenuLen = sizeof(settingsMenu)/sizeof(settingsMenu[0]);

/********************************** GLOBALS ***********************************/
int oldPosition = -1;
unsigned long oldMilli = 0;
int oldState = -1;
int state = S_START;
int secElapsed = 0;
unsigned long oldTime = 0;
int lastState = S_START;

/********************************** METHODS ***********************************/
void disp_menu(String* , int , bool = true);
void disp_setTime();
void disp_setTimeDone();
void disp_manual();
void disp_expose();
void disp_exposeDone();
void disp_stop();
void disp_info();
void drawCentreString(const String&, int, int);
void draw_pause(int, int, bool = false) ;
void draw_stop(int, int, bool = false);
void draw_play(int, int, bool = false);
void draw_back(int, int, bool = false);
void draw_clock(int, int, bool = false);
void draw_sel(int x, int y);
void click(Button2&);
void checkPosition();

/*******************************************************************************
************************************* INIT *************************************
*******************************************************************************/
void setup() {
  Serial.begin(9600);
  
  // RELAY
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // ENCODER
  b.setTapHandler(click);
  r.resetPosition(0);

  // DISPLAY
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setRotation(2);
  disp_menu(startMenu, startMenuLen);
}


/*******************************************************************************
********************************** MAIN LOOP ***********************************
*******************************************************************************/
void loop() {
  bool tick = false;

  // check encoder
  r.loop();
  b.loop();

  // check encoder position limits
  checkPosition();

  // count seconds
  if(digitalRead(RELAY_PIN) && (oldTime + 1000 <= millis())) {
    secElapsed++;
    oldTime += 1000;
    tick = true;
  }

  // check if something changed and we have to update display
  bool timePassed = oldMilli + 300 < millis();
  bool newPos = oldPosition != r.getPosition();
  bool newState = oldState != state;

  // update display depending on state when something changes
  if( timePassed && (newPos || newState || tick) ) {
    oldState = state;
    oldMilli = millis();
    oldPosition= r.getPosition();
    switch(state) {
      case S_INFO:
        disp_info();
        break;
      case S_EXPO_M:
        disp_menu(expoMenu, expoMenuLen, false);
        draw_back(0,36,(r.getPosition()==2 ? true : false));
        display.display();
        break;
      case S_MANUAL:
        disp_manual();
        break;
      case S_SET_TIME:
        disp_setTime();
        break;
      case S_SET_TIME_DONE:
        disp_setTimeDone();
        break;
      case S_EXPOSE:
        disp_expose();
        if(settings.last_time - secElapsed <= 0){
          state = S_EXPOSE_DONE;
          digitalWrite(RELAY_PIN, LOW);
        }
        break;
      case S_EXPOSE_DONE:
        disp_exposeDone();
        break;
      case S_STOP:
        disp_stop();
        break;
      case S_START:
      default: 
        disp_menu(startMenu, startMenuLen);
        break;
    }
  }
}

/*
 * @brief Method to handle callbacks from button clicks
 * 
 * @param btn - Button object with detailed information
 */
void click(Button2& btn) {
 if(state == S_START) {
    switch(r.getPosition()) {
      case 0:
        state = S_EXPO_M;
        break;
      case 1:
        state = S_SETTINGS;
        break;
      case 2:
        state = S_INFO;
        break;
    }
    r.resetPosition(0);
  }
  
  else if (state == S_SET_TIME) {
    settings.last_time = r.getPosition() * settings.sec_inc;
    r.resetPosition(1);
    state = S_SET_TIME_DONE;
  }

  else if (state == S_SET_TIME_DONE) {
    switch(r.getPosition()) {
      case 0:
        state = S_EXPO_M;
        r.resetPosition(2);
        break;
      case 1:
        secElapsed = 0;
        oldTime = millis();
        digitalWrite(RELAY_PIN, HIGH);
        r.resetPosition(0);
        state = S_EXPOSE;
        break;
    }
  }

  else if (state == S_MANUAL) {
    switch(r.getPosition()) {
      case 0:
        if(!digitalRead(RELAY_PIN)){
          oldTime = millis();
          oldPosition = -1;
          digitalWrite(RELAY_PIN, HIGH);
        }
        else {
          oldPosition = -1;
          digitalWrite(RELAY_PIN, LOW);
        }
        break;
      case 1:
        r.resetPosition(0);
        lastState = state;
        state = S_STOP;
        break;
    }
  }

  else if (state == S_EXPOSE) {
    switch(r.getPosition()) {
      case 0:
        if(!digitalRead(RELAY_PIN)){
          oldTime = millis();
          oldPosition = -1;
          digitalWrite(RELAY_PIN, HIGH);
        }
        else {
          oldPosition = -1;
          digitalWrite(RELAY_PIN, LOW);
        }
        break;
      case 1:
        r.resetPosition(0);
        lastState = state;
        state = S_STOP;
        break;
    }
  }

  else if (state == S_STOP) {
    switch(r.getPosition()) {
      case 0:
        state = lastState;
        break;
      case 1:
        r.resetPosition(0);
        digitalWrite(RELAY_PIN, LOW);
        state = S_START;
        break;
    }
  }
  
  else if(state == S_INFO) {
    r.resetPosition(0);
    state = S_START;
  }

  else if(state == S_EXPOSE_DONE) {
    r.resetPosition(0);
    state = S_START;
  }

  else if(state == S_EXPO_M) {
    switch(r.getPosition()) {
      case 0:
        r.resetPosition(settings.default_time/settings.sec_inc);
        state = S_SET_TIME;
        break;
      case 1:
        r.resetPosition(0);
        secElapsed = 0;
        oldTime = millis();
        digitalWrite(RELAY_PIN, HIGH);
        state = S_MANUAL;
        break;
      case 2:
       r.resetPosition(0);
        state = S_START;
        break;
      
    }
  }
}

/*
 * @brief Method to check the limits of the encoder positions 
 *        for each of the different states
 */
void checkPosition() {
  int pos = r.getPosition();  
  
  if(pos < 0) {
    r.resetPosition(0);
    return;
  }
  
  if(state == S_START && pos >= startMenuLen)
    r.resetPosition(startMenuLen - 1);

  else if(state == S_SETTINGS && pos >= settingsMenuLen)
    r.resetPosition(settingsMenuLen - 1);

  else if(state == S_EXPO_M && pos >= expoMenuLen)
    r.resetPosition(expoMenuLen - 1);

  else if(state == S_SET_TIME) {
    if(pos*settings.sec_inc > settings.max_time)
      r.resetPosition(settings.max_time/settings.sec_inc);
    if(pos*settings.sec_inc < settings.min_time)
      r.resetPosition(settings.min_time/settings.sec_inc);
  }

  else if(state == S_SET_TIME_DONE && pos > 1)
    r.resetPosition(1);

  else if (state == S_EXPOSE && pos > 1)
    r.resetPosition(1);
  
  else if (state == S_MANUAL && pos > 1)
    r.resetPosition(1);

  else if (state == S_STOP && pos > 1)
    r.resetPosition(1);

  else if (state == S_INFO && pos > 3)
    r.resetPosition(3);
}


/*******************************************************************************
******************************** SCREEN DRAWER *********************************
*******************************************************************************/

/*
 * @brief Method to draw a standard text menu with one item per line.
 * 
 * @param items - Array of strings which represent the menu items
 * @param len - number of items in the string array
 * @param draw - (default true) - If false, the menu is not written to display
 */
void disp_menu(String *items, int len, bool draw) {
  display.clearDisplay();
  display.setTextSize(2);
  for(int i = 0; i < len; ++i) {
    if(i == r.getPosition())
      display.setTextColor(BLACK, WHITE);
    else
      display.setTextColor(WHITE);
    display.setCursor(0, i*17);
    display.println(items[i]);
  }
  if(draw)
    display.display();
}

void disp_setTime() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawCentreString("Time", 0, 1);
  String timeVal = String(r.getPosition()*settings.sec_inc);
  drawCentreString(timeVal, 13, 2);
  drawCentreString("sec", 32, 1);
  draw_back(0, 36);
  draw_play(52, 36);
  display.display();
}

void disp_setTimeDone() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawCentreString("Time", 0, 1);
  String timeVal = String(settings.last_time);
  drawCentreString(timeVal, 13, 2);
  drawCentreString("sec", 32, 1);
  draw_back(0, 36, !bool(r.getPosition()));
  draw_play(52, 36, bool(r.getPosition()));
  display.display();
}

void disp_manual() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  String timeVal = String(secElapsed);
  drawCentreString("On since", 0, 1);
  drawCentreString(timeVal, 13, 2);
  drawCentreString("sec", 33, 1);
  if(!digitalRead(RELAY_PIN))
    draw_play(0, 36, !bool(r.getPosition()));
  else
    draw_pause(0, 36, !bool(r.getPosition()));
  draw_stop(52, 36, bool(r.getPosition()));
  display.display();
}

void disp_expose() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  String timeVal = String(settings.last_time - secElapsed);
  drawCentreString("Time left", 0, 1);
  drawCentreString(timeVal, 13, 2);
  drawCentreString("sec", 33, 1);
  if(!digitalRead(RELAY_PIN))
    draw_play(0, 36, !bool(r.getPosition()));
  else
    draw_pause(0, 36, !bool(r.getPosition()));
  draw_stop(52, 36, bool(r.getPosition()));
  display.display();
}

void disp_exposeDone() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawCentreString("DONE!", 10, 2);
  display.setTextColor(BLACK, WHITE);
  drawCentreString("OK", 40, 1);
  display.display();
}

void disp_stop() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawCentreString("Stop?", 10, 2);
  display.setTextSize(1);
  
  display.setCursor(0, 36);
  if(r.getPosition() == 0)
    display.setTextColor(BLACK, WHITE);
  display.print("NO");
  
  display.setCursor(46, 36);
  display.setTextColor(WHITE);
  if(r.getPosition() == 1)
    display.setTextColor(BLACK, WHITE);
  display.print("YES");
  display.display();
}

void disp_info(){
  display.clearDisplay();
  display.setTextColor(WHITE);
  //display.drawRect(0, 0, 64, 48, WHITE);
  String ver( String("Version:\n") + VERSION);
  drawCentreString("Version:", 0, 1);
  drawCentreString(settings.sw_version, 8, 1);
  drawCentreString("Author:", 16, 1);
  drawCentreString("F. S.", 24, 1);
  
  if(r.getPosition() == 0) {
    draw_pause(0,36,true);
    draw_stop(12,36);
    draw_play(24,36);
    draw_back(36,36);
  }
  else if(r.getPosition() == 1){
    draw_pause(0,36);
    draw_stop(12,36,true);
    draw_play(24,36);
    draw_back(36,36);
  }
  else if(r.getPosition() == 2){
    draw_pause(0,36);
    draw_stop(12,36);
    draw_play(24,36,true);
    draw_back(36,36);
  }
  else {
    draw_pause(0,36);
    draw_stop(12,36);
    draw_play(24,36);
    draw_back(36,36,true);
  }
  
  display.display();
}


/*******************************************************************************
**************************** DISPLAY DRAWING HELPER ****************************
*******************************************************************************/

/*
 * @brief Function to output centered text on display
 * 
 * @param buf - String which should be centered
 * @param y - The y-position of the text
 * @param txt_size - The size of the text
 */
void drawCentreString(const String &buf, int y, int txt_size)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(txt_size);
    display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h); //calc width of new string
    display.setCursor(SSD1306_LCDWIDTH / 2 - w / 2, y);
    display.println(buf);
}

/*
 * @brief Function to draw a pause symbol
 *        The symbol will need 12x12px on the display
 *        (full 12x12px only if selection marker is drawn)
 * 
 * @param x - The top left x-position of the icon
 * @param y - The top left y-position of the icon
 * @param sel - (default false) - If true, selection marker is drawn 
 */
void draw_pause(int x, int y, bool sel) {
  draw_stop(x, y, sel);
  display.fillRect(x+5, y+2, 2, 8, BLACK);
}

/*
 * @brief Function to draw a stop symbol
 *        The symbol will need 12x12px on the display
 *        (full 12x12px only if selection marker is drawn)
 * 
 * @param x - The top left x-position of the icon
 * @param y - The top left y-position of the icon
 * @param sel - (default false) - If true, selection marker is drawn 
 */
void draw_stop(int x, int y, bool sel) {
  display.fillRect(x+2, y+2, 8, 8, WHITE);
  if(sel) 
    draw_sel(x, y);
}

/*
 * @brief Function to draw a play symbol
 *        The symbol will need 12x12px on the display
 *        (full 12x12px only if selection marker is drawn)
 * 
 * @param x - The top left x-position of the icon
 * @param y - The top left y-position of the icon
 * @param sel - (default false) - If true, selection marker is drawn 
 */
void draw_play(int x, int y, bool sel) {
  display.fillTriangle(x+2, y+2, x+2, y+9, x+9, y+5, WHITE);
  if(sel)
    draw_sel(x, y);
}

/*
 * @brief Function to draw a back symbol
 *        The symbol will need 12x12px on the display
 *        (full 12x12px only if selection marker is drawn)
 * 
 * @param x - The top left x-position of the icon
 * @param y - The top left y-position of the icon
 * @param sel - (default false) - If true, selection marker is drawn 
 */
void draw_back(int x, int y, bool sel) {
  display.drawRoundRect(x+2, y+2, 9, 6, 3, WHITE);
  display.fillRect(x+2, y+2, 4, 4, BLACK);
  display.fillTriangle(x+1, y+7, x+3, y+9, x+3, y+5, WHITE);
  if(sel)
    draw_sel(x,y);
}

/*
 * @brief Function to draw a selection marker arround symbols
 *        The symbol with marker will need 12x12px on the display
 * 
 * @param x - The top left x-position of the icon
 * @param y - The top left y-position of the icon
 */
void draw_sel(int x, int y) {
  display.drawRect(x, y, 12, 12, WHITE);
  display.drawFastVLine(x, y+2, 8, BLACK);
  display.drawFastVLine(x+11, y+2, 8, BLACK);
}