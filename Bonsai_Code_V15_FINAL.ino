#include "RTClib.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

const int pResistor = A1; // Photoresistor at Arduino analog pin A1

const int BUTTON = 2;

const int NEOPIXEL_PIN = 6;
const int NEOPIXEL_COUNT = 40;

Adafruit_7segment matrix = Adafruit_7segment(); // Set up clock display

RTC_PCF8523 rtc; // set up rtc

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);



// ** CONSTANTS ** ---------------------------------------------------------------------------------------------------------------------------

const int PRESISTOR_UPDATE_RANGE = 45;
const int BRIGHTNESS_UPDATE_INTERVAL = 600; // seconds

const int HOLD_TO_SET_TIME = 8; // number of seconds to hold button to set the time
const int HOLD_TO_CHANGE_TIME_SET = 3; // number of seconds to hold button to switch between time setting


const int ANIMATE_MAX_BRIGHTNESS = 150; // max brightness for neopixels in animation
const int ANIMATE_MIN_BRIGHTNESS = 0; // min brightness for neopixels in animation
const int ANIMATE_BRIGHTNESS_OFFSET = 0; // how much to offset the tree auto brightness

const int LAMP_MODE_COLOR[3] = {38, 75, 90}; // HSV (0-360,0-100,0-100)
const int LIGHT_BLUE[] = {0, 255, 255};
const int ORANGE[] = {255, 50, 0};
const int PINK[] = {255, 0, 191};
const int MAGENTA[] = {255, 51, 204};
const int GREEN[] = {120, 100, 50}; // HSV

const int QUARTER_WAVE_SPEED = 10;
const int QUARTER_WAVE_LEN = 100;

const int WAVE_FINISH_OFFSET = 10; // number of pixels past the end of the tree that the crest of the wave should go

const int SHOWOFF_BRIGHTNESS = 200; // tree brightness in showOff mode

const int HEIGHT_PULSE_QUANTITY = 5;
const int HEIGHT_PULSE_SCALE = 250;
const int LAYER_INTERVALS = HEIGHT_PULSE_SCALE / HEIGHT_PULSE_QUANTITY;
const int LAYER_SCALE_LOCATIONS[HEIGHT_PULSE_QUANTITY] = {LAYER_INTERVALS / 2, LAYER_INTERVALS + (LAYER_INTERVALS / 2), (LAYER_INTERVALS * 2) + (LAYER_INTERVALS / 2), (LAYER_INTERVALS * 3) + (LAYER_INTERVALS / 2), (LAYER_INTERVALS * 4) + (LAYER_INTERVALS / 2)};
const int SORT_BY_HEIGHT0[3] = {19, 27, 29};
const int SORT_BY_HEIGHT1[5] = {18, 20, 28, 35, 22};
const int SORT_BY_HEIGHT2[8] = {15, 21, 14, 30, 33, 36, 5, 6};
const int SORT_BY_HEIGHT3[12] = {16, 9, 34, 23, 25, 26, 37, 12, 13, 2, 4, 7};
const int SORT_BY_HEIGHT4[11] = {0, 1, 3, 8, 10, 11, 24, 32, 31, 38, 39};

const int HOW_MANY_RAINBOW_BLINKS = 180;
const int POTENTIAL_RAINBOW_COLORS = 80;

const int HOURLY_RAINBOW_BLINKS = 600;

// ** GLOBAL VARS ** ---------------------------------------------------------------------------------------------------------------------------
int nowHour;
int nowMinute;
int nowSecond;
long nowUnix;
int nowDay;
int nowMonth;
int nowYear;

int timeSettingState = -1;
int setToThisHour = -1;
int setToThisMinute = -1;

int pResistorValue; // Store value from photoresistor (0-1023)
long lastBrightnessUpdate = 0;
int pResistorOnLastUpdate = 0;
int brightnessUpper = 0;
int brightnessLower = 0;

int blueShimmerVals[NEOPIXEL_COUNT] = {};
int blueShimmerCounter = 0;
bool blueShimmerDecrease = true;

int lampModeFade = 0;

int invadeFadeStates[NEOPIXEL_COUNT] = {};
float invadeCrests[] = {0, 0};

int showOffState = 0;

int buttonState = 0; // 0 - blueShimmer ,1 - Lamp mode,2 - off,3 - show off, 5 - timeSetting
int oldButtonState = 7;
bool buttonPressed = false;
long buttonPressStartTime = -1; // unix timestamp of when the button press started

int wavedThisQuarter = 0;

long firstPixelHue = 0;
int wavedThisHour = 0;
int rainbowDivisor = 0;


int pulseLocation = 0;
int pulseSpeedVar = 0;

int rainbowBlinkSpeedVar = 0;
int rainbowBlinks = 0;

int heightPulseLocation = 0;
int heightPulseNumber = 0;

int dropFillPixelsFilled = 0;
int dropFillDropLoc = 0;
int dropFillSpeedVar = 0;



// ** SETUP ** ---------------------------------------------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(57600);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

//  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // adjusts RTC to computer time
//  DateTime atCompile = rtc.now();
//  DateTime realTime (atCompile + TimeSpan(0, 0, 0, 10));
//  rtc.adjust(realTime);

  //  rtc.adjust(DateTime(2014, 1, 21, 4, 59, 57)); // sets RTC to this time

  matrix.begin(0x70);
  matrix.setBrightness(5); // from 0-15, initial brightness setting

  pinMode(pResistor, INPUT);// Set pResistor pinmode
  pinMode(BUTTON, INPUT);// Set button pinmode

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  for (int i = 0; i < NEOPIXEL_COUNT; i++) {
    blueShimmerVals[i] = 0;
    invadeFadeStates[i] = 0;
  }

}



// ** LOOP ** ---------------------------------------------------------------------------------------------------------------------------------
void loop() {
  getDateTime(); // gets variables from RTC

  manageBrightness();

  buttonLogicTheSequel();

  if (buttonState == 5) {
    setTime();
  }
  else {
    displayTime();
  }

  matrix.writeDisplay();
  //  serialPrintTime();


  treeState();
}




// ** TIME FUNCTIONS ** ---------------------------------------------------------------------------------------------------------------------------------
int dispHourLogic() {
  int hourToDisp = nowHour;

  if (hourToDisp > 12) { // if is after noon, subtract 12
    hourToDisp -= 12;
  }
  else if (hourToDisp == 0) { // 12midnight = 0, so add 12
    hourToDisp += 12;
  }
  return hourToDisp;
}



void getDateTime() {
  DateTime now = rtc.now();
  nowHour = now.hour();
  nowMinute = now.minute();
  nowSecond = now.second();
  nowUnix = now.unixtime();
}



void displayTime() {
  matrix.print((dispHourLogic() * 100) + nowMinute);

  if (nowHour >= 12 && nowHour != 24) {
    matrix.writeDigitNum(4, nowMinute % 10, true);
  }

  if ((millis() % 1000) >= 500) {
    matrix.drawColon(true);
  }
  else {
    matrix.drawColon(false);
  }
}


void setTime() {
  switch (timeSettingState) {
    case -1:
      matrix.blinkRate(2);
      setToThisHour = nowHour;
      setToThisMinute = nowMinute;
      matrix.clear();
      if (digitalRead(BUTTON) == LOW && !buttonPressed) {
        timeSettingState = 0;
      }
      break;
    case 0:
      if (setToThisHour >= 24) {
        setToThisHour = 0;
      }

      matrix.clear();
      if (setToThisHour >= 10) {
        matrix.writeDigitNum(0, setToThisHour / 10);
      }
      matrix.writeDigitNum(1, setToThisHour % 10);
      break;
    case 1:
      matrix.clear();
      matrix.writeDigitNum(3, setToThisMinute / 10);
      matrix.writeDigitNum(4, setToThisMinute % 10);
      if (digitalRead(BUTTON) == LOW && !buttonPressed) {
        timeSettingState = 2;
      }
      break;
    case 2:
      if (setToThisMinute >= 60) {
        setToThisMinute = 0;
      }

      matrix.clear();
      matrix.writeDigitNum(3, setToThisMinute / 10);
      matrix.writeDigitNum(4, setToThisMinute % 10);
      break;
    case 3:
      buttonState = 3; // set it to second-to-last button state for when it increments
      rtc.adjust(DateTime(nowYear, nowMonth, nowDay, setToThisHour, setToThisMinute, nowSecond)); // sets RTC to this time
      getDateTime();
      buttonPressStartTime = nowUnix;

      matrix.blinkRate(0);
      timeSettingState = -1;
      break;
  }

  matrix.drawColon(true);
}


void serialPrintTime() {
  Serial.print(nowHour);
  Serial.print(':');
  Serial.print(nowMinute);
  Serial.print(':');
  Serial.print(nowSecond);
  Serial.println();
}


// ** BRIGHTNESS FUNCTIONS ** ---------------------------------------------------------------------------------------------------------------------------------

void manageBrightness() {
  pResistorValue = analogRead(pResistor); // read pResistor

  brightnessUpper = pResistorOnLastUpdate + PRESISTOR_UPDATE_RANGE;
  brightnessLower = pResistorOnLastUpdate - PRESISTOR_UPDATE_RANGE;

  if (nowUnix > (lastBrightnessUpdate + BRIGHTNESS_UPDATE_INTERVAL) || pResistorValue >= brightnessUpper || pResistorValue <= brightnessLower) {
    lastBrightnessUpdate = nowUnix;

    if (buttonState == 0) {
      mapTreeBrightness();
    }
    matrix.setBrightness(map(pResistorValue, 0, 1000, 0, 15)); // from 0-15
    pResistorOnLastUpdate = pResistorValue;
    Serial.println("Mapped Brightness"); // #######
  }
}

void mapTreeBrightness() {
  int brightness = map(pResistorValue, 0, 1000, ANIMATE_MIN_BRIGHTNESS, ANIMATE_MAX_BRIGHTNESS);
  brightness += ANIMATE_BRIGHTNESS_OFFSET;
  if (brightness < 0) {
    brightness = 0;
  }
  else if (brightness > 255) {
    brightness = 255;
  }
  strip.setBrightness(brightness);
}


// ** BUTTON FUNCTIONS ** ---------------------------------------------------------------------------------------------------------------------------------

void buttonLogicTheSequel() {
  if (digitalRead(BUTTON) == HIGH) {
    if (!buttonPressed) { // if button press is new
      buttonPressed = true;
      buttonPressStartTime = nowUnix;
    }

    if (buttonState == 5) { // code for timeSetting Mode
      if (nowUnix - buttonPressStartTime > HOLD_TO_CHANGE_TIME_SET && timeSettingState != -1) {
        buttonPressStartTime = nowUnix;
        timeSettingState ++;
      }
    }

    else {
      if (nowUnix - buttonPressStartTime > HOLD_TO_SET_TIME) { // in any state other than time setting state, if the button is held down for this many seconds, start time setting sequence
        buttonState = 5; // 3 states of button
        timeSettingState = -1;
      }
    }
  }

  else if (digitalRead(BUTTON) == LOW) {
    buttonPressStartTime = nowUnix;
    if (buttonPressed) { // if button release is new
      buttonPressed = false;

      if (buttonState < 4) {
        buttonState ++;
      }

      else if (buttonState == 5) { // if in timeSetting Mode
        if (timeSettingState == 0) {
          setToThisHour ++;
        }
        else if (timeSettingState == 2) {
          setToThisMinute ++;
        }
      }

    }

  }
  if (buttonState == 4) { // if the buttonState is too high, set back to 0
    buttonState = 0; // 3 states of button
  }
}


// ** LIGHT FUNCTIONS ** ---------------------------------------------------------------------------------------------------------------------------------

void treeState() {
  switch (buttonState) {
    case 0:
      usualAnimation(); // blueshimmer and time-specific
      break;
    case 1:
      lampMode(); // fade up to lamp
      break;
    case 2:
      treeOff(); //fades from lampMode to off
      break;
    case 3:
      //      Serial.println("ShowOffMode");
      showOff();
      break;
    case 5:
      strip.fill(strip.Color(0, 255, 0));
      strip.show();
      break;
    default:
      Serial.println("buttonState is too high!!");
      break;
  }
}



void usualAnimation() { // treeState 0
  if (oldButtonState != buttonState) { // run once when switch
    strip.clear();
    strip.show();
    mapTreeBrightness();
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
      blueShimmerVals[i] = 0;
    }
    invadeCrests[0] = 0;
    invadeCrests[1] = 0;
    oldButtonState = buttonState;
  }

  if (!timeChecks()) {
    blueShimmer();
  }
  strip.show();
}



bool timeChecks() {
  if (hourlyRainbowBlink()) {
    return true;
  }
  else if (quarterHourWave()) {
    return true;
  }
  return false;
}



bool hourlyRainbowBlink() {
  if (nowMinute == 1 && wavedThisHour == 2) { //if the minute is 1, reset the wave status
    rainbowBlinks = 0;
    wavedThisHour = 0;
  }
  if (nowMinute == 0 && wavedThisHour == 0) { // if new hour and not waved, reset counters and increment wave status
    rainbowBlinks = 0;
    wavedThisHour = 1;
  }

  if (nowMinute == 0 && wavedThisHour == 1) { // if new hour and proper wave state, do the wave
    rainbowDivisor ++;

    if (rainbowBlinks < HOURLY_RAINBOW_BLINKS) {
      if (rainbowDivisor >= 40) {
        int randHue = POTENTIAL_RAINBOW_COLORS * random(0, (65536L / POTENTIAL_RAINBOW_COLORS));
        strip.setPixelColor(random(0, NEOPIXEL_COUNT), strip.gamma32(strip.ColorHSV(randHue)));
        rainbowDivisor = 0;
        rainbowBlinks ++;
        strip.show(); // Update strip with new contents
      }
      else {
        rainbowDivisor ++;
      }
      return true;
    }

    else { // if rainbow is done
      //      Serial.println("Rainbow IS DONE"); // #######
      for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        blueShimmerVals[i] = 0;
      }
      rainbowBlinks = 0;
      wavedThisHour = 2;
      return true;
    }

  }
  return false;
}


bool quarterHourWave() {
  //  Serial.println(wavedThisQuarter);
  if ((nowMinute - 1) % 15 == 0 && wavedThisQuarter == 2) { // at minutes 1,16,31,46 reset waved state
    wavedThisQuarter = 0;
  }
  else if ((nowMinute == 14 || nowMinute == 29 || nowMinute == 44) && nowSecond > 45) { // at minutes 14,29,44 reverse blueshimmer for 15 secs
    blueShimmerDecrease = false;
  }

  if (nowMinute % 15 == 0 && wavedThisQuarter == 0 && nowMinute != 0) { // at quarters (not hour) reset invades and increment waved state
    blueShimmerDecrease = true ;
    invadeCrests[0] = 0;
    invadeCrests[1] = 0;
    wavedThisQuarter = 1;
  }

  if (nowMinute % 15 == 0 && wavedThisQuarter == 1 && nowMinute != 0) { // at quarters
    if (nowSecond < 10) { // for orange wave
      if (invadeCrests[0] < (NEOPIXEL_COUNT + WAVE_FINISH_OFFSET)) {
        invade(LIGHT_BLUE, ORANGE, QUARTER_WAVE_SPEED, QUARTER_WAVE_LEN, 0);
        //        Serial.println("Orange2Blue IS DONE"); // #######
      }
      return true;
    }

    else if (invadeCrests[1] < (NEOPIXEL_COUNT + WAVE_FINISH_OFFSET)) { // while blue wave is not finished
      invade(ORANGE, LIGHT_BLUE, QUARTER_WAVE_SPEED, QUARTER_WAVE_LEN, 1);
      return true;
    }
    else { // when both are done
      //      Serial.println("Orange2Blue IS DONE"); // #######
      for (int i = 0; i < NEOPIXEL_COUNT; i++) { // set shimmer values high to transition nicely
        blueShimmerVals[i] = 255;
      }
      wavedThisQuarter = 2; // wave is done. no more this quarter
      return true;
    }
  }
  return false;
}


void lampMode() { // treeState 1
  if (oldButtonState != buttonState) {// run once when switch
    strip.setBrightness(255);
    lampModeFade = 0;

    strip.clear();
    strip.show();

    oldButtonState = buttonState;
  }
  int noColor[3] = {0, 0, 0};

  if (lampModeFade < (LAMP_MODE_COLOR[2] * 2)) {
    lampModeFade ++;

    uint32_t rgbLampColor = strip.gamma32(goodHSV(LAMP_MODE_COLOR[0], LAMP_MODE_COLOR[1], lampModeFade / 2));
    //    uint32_t rgbLampColor = fadeColor(noColor, LAMP_MODE_COLOR, (LAMP_MODE_COLOR[2] * 2), lampModeFade);


    strip.fill(rgbLampColor);
    strip.show();
  }

}



void treeOff() {
  if (oldButtonState != buttonState) { // tree off
    oldButtonState = buttonState;
  }

  if (lampModeFade > 0) {
    lampModeFade --;

    uint32_t rgbLampColor = strip.gamma32(goodHSV(LAMP_MODE_COLOR[0], LAMP_MODE_COLOR[1], lampModeFade / 2));

    strip.fill(rgbLampColor);
    strip.show();
  }
}




void showOff() { // treeState 4
  if (oldButtonState != buttonState) { // run once when switch
    strip.clear();
    strip.show();
    strip.setBrightness(SHOWOFF_BRIGHTNESS);

    showOffState = 0;

    firstPixelHue = 0;

    oldButtonState = buttonState;
  }

  switch (showOffState) {
    case 0:
      if (firstPixelHue < 5 * 65536) {
        rainbow();
        firstPixelHue += 256;
        strip.show(); // Update strip with new contents
      }
      else {
        showOffState ++;
        firstPixelHue = 0;
        strip.clear();
      }
      break;

    case 1:
      if (pulseLocation < 4 * NEOPIXEL_COUNT) {
        int wherePulse = pulseLocation;
        if (pulseLocation >= NEOPIXEL_COUNT && pulseLocation < 2 * NEOPIXEL_COUNT) {
          wherePulse = NEOPIXEL_COUNT - (pulseLocation - NEOPIXEL_COUNT);
        }
        else if (pulseLocation >= 2 * NEOPIXEL_COUNT && pulseLocation < 3 * NEOPIXEL_COUNT) {
          wherePulse = pulseLocation - 2 * NEOPIXEL_COUNT;
        }
        else if (pulseLocation >= 3 * NEOPIXEL_COUNT && pulseLocation < 4 * NEOPIXEL_COUNT) {
          wherePulse = NEOPIXEL_COUNT - (pulseLocation - (3 * NEOPIXEL_COUNT));
        }

        strip.clear();
        strip.setPixelColor(wherePulse - 3, goodHSV(315, 100, 1));
        strip.setPixelColor(wherePulse - 2, goodHSV(315, 100, 5));
        strip.setPixelColor(wherePulse - 1, goodHSV(315, 100, 10));
        strip.setPixelColor(wherePulse, goodHSV(315, 100, 50));
        strip.setPixelColor(wherePulse + 1, goodHSV(315, 100, 10));
        strip.setPixelColor(wherePulse + 2, goodHSV(315, 100, 5));
        strip.setPixelColor(wherePulse + 3, goodHSV(315, 100, 1));

        if (pulseSpeedVar == 12) {
          pulseLocation ++;
          pulseSpeedVar = 0;
        }
        else {
          pulseSpeedVar ++;
        }

      }

      else {
        showOffState ++;
        pulseLocation = 0;
        strip.clear();
      }
      break;

    case 2:
      if (rainbowBlinks < HOW_MANY_RAINBOW_BLINKS) {
        if (rainbowBlinkSpeedVar >= 40) {
          int randHue = POTENTIAL_RAINBOW_COLORS * random(0, (65536L / POTENTIAL_RAINBOW_COLORS));
          strip.setPixelColor(random(0, NEOPIXEL_COUNT), strip.gamma32(strip.ColorHSV(randHue)));
          rainbowBlinkSpeedVar = 0;
          rainbowBlinks ++;
        }
        else {
          rainbowBlinkSpeedVar ++;
        }
      }
      else {
        rainbowBlinks = 0;
        showOffState ++;
        strip.clear();
      }
      break;

    case 3:
      if (dropFillPixelsFilled < NEOPIXEL_COUNT) {
        strip.clear();

        if (dropFillSpeedVar >= 7) {
          dropFillSpeedVar = 0;
          if (dropFillDropLoc == NEOPIXEL_COUNT - dropFillPixelsFilled - 1) {
            dropFillPixelsFilled += 2;
            dropFillDropLoc = 0;
          }
          else {
            dropFillDropLoc ++;
          }
        }
        dropFillSpeedVar ++;

        strip.setPixelColor(dropFillDropLoc, goodHSV(GREEN[0], GREEN[1], 50));

        strip.fill(strip.gamma32(goodHSV(GREEN[0], GREEN[1], GREEN[2])), NEOPIXEL_COUNT - dropFillPixelsFilled);
      }

      else {
        dropFillPixelsFilled = 0;
        dropFillDropLoc = 0;
        dropFillSpeedVar = 0;
        strip.clear();
        showOffState ++;
      }
      break;


    default:
      showOffState = 0;
      strip.clear();
      break;

  }

  strip.show();
}



// ** ANIMATION FUNCTIONS ** ---------------------------------------------------------------------------------------------------------------------------------



void invade(int INVADE_START_COLOR[], int INVADE_END_COLOR[], int INVADE_WAVE_SPEED, int INVADE_WAVE_LENGTH, int invadeCrestIndex) {
  if (invadeCrests[invadeCrestIndex] == 0) {
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
      invadeFadeStates[i] = 0;
    }
    strip.fill(strip.Color(INVADE_START_COLOR[0], INVADE_START_COLOR[1], INVADE_START_COLOR[2]));

  }

  int lastPixelToColor = round(invadeCrests[invadeCrestIndex]);
  if (lastPixelToColor > NEOPIXEL_COUNT) {
    lastPixelToColor = NEOPIXEL_COUNT;
  }

  for (int i = 0; i < lastPixelToColor; i++) {
    if (invadeFadeStates[i] < INVADE_WAVE_LENGTH) {
      invadeFadeStates[i] += 1;
    }
    strip.setPixelColor(i, fadeColor(INVADE_START_COLOR, INVADE_END_COLOR, INVADE_WAVE_LENGTH, invadeFadeStates[i]));
  }

  if (invadeCrests[invadeCrestIndex] < (NEOPIXEL_COUNT + WAVE_FINISH_OFFSET)) {
    strip.show();
    invadeCrests[invadeCrestIndex] += float(INVADE_WAVE_SPEED) / 100;
  }
}



void blueShimmer() {
  blueShimmerCounter ++;
  blueShimmerCounter %= 8;

  if (blueShimmerCounter == 0) {

    if (millis() % 2 == 0) {
      int pixelToSet = random(0, NEOPIXEL_COUNT);
      blueShimmerVals[pixelToSet] = 255;
    }
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
      if (blueShimmerVals[i] > 0 && blueShimmerDecrease) {
        blueShimmerVals[i] -= 2;
        strip.setPixelColor(i, 0, blueShimmerVals[i], blueShimmerVals[i]);
      }
      else if (!blueShimmerDecrease) {
        if (blueShimmerVals[i] < 255) {
          blueShimmerVals[i] += 4;
        }
        strip.setPixelColor(i, 0, blueShimmerVals[i], blueShimmerVals[i]);
      }

    }
  }
}


// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow() {
  for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
    int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
}




uint32_t fadeColor(int fromColor[3], int toColor[3], int steps, int fadePos) {
  int biggestFrom;
  if (fromColor[0] > fromColor[1]) {
    biggestFrom = fromColor[0];
  }
  else {
    biggestFrom = fromColor[1];
  }
  if (fromColor[2] > biggestFrom) {
    biggestFrom = fromColor[2];
  }

  int biggestTo;
  if (toColor[0] > toColor[1]) {
    biggestTo = toColor[0];
  }
  else {
    biggestTo = toColor[1];
  }
  if (toColor[2] > biggestTo) {
    biggestTo = toColor[2];
  }

  float sum = biggestFrom + biggestTo;
  float sumPos = map(fadePos, 0, steps, 0, sum);
  float retColor[3];

  if (sumPos < biggestFrom) {
    retColor[0] = fromColor[0] - ((float(fromColor[0]) / float(biggestFrom)) * sumPos);
    retColor[1] = fromColor[1] - ((float(fromColor[1]) / float(biggestFrom)) * sumPos);
    retColor[2] = fromColor[2] - ((float(fromColor[2]) / float(biggestFrom)) * sumPos);

    return strip.Color(retColor[0], retColor[1], retColor[2]);
  }
  else if (sumPos < sum) {
    sumPos -= biggestFrom;
    retColor[0] = (float(toColor[0]) / float(biggestTo)) * sumPos;
    retColor[1] = (float(toColor[1]) / float(biggestTo)) * sumPos;
    retColor[2] = (float(toColor[2]) / float(biggestTo)) * sumPos;

    return strip.Color(retColor[0], retColor[1], retColor[2]);
  }
  else {
    return strip.Color(toColor[0], toColor[1], toColor[2]);
  }
}



uint32_t goodHSV(int h, int s, int v) {
  return strip.ColorHSV(map(h, 0, 360, 0, 65536), map(s, 0, 100, 0, 255), map(v, 0, 100, 0, 255));
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b    - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
