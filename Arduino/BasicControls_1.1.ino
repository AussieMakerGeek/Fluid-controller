#include <PinButton.h>
#include <TFT_eSPI.h>      // Hardware-specific library

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

// Pin Defs
#define buttonPinUp PB7
#define buttonPinDn PB6
#define buttonPinLe PB5
#define buttonPinRt PB8
#define buttonPinOK PB9
#define buttonPinTL PB4
#define buttonPinTM PB3
#define buttonPinTR PA15
#define buttonPinBL PA12
#define buttonPinBM PA11
#define buttonPinBR PA8

//LCD Colours
#define TFT_LIGHTBLUE 0x86FF
#define TFT_LIGHTRED 0xF9E7
#define TFT_DARKBLUE 0x0018
#define TFT_OVERYELLOW 0xFF35
#define TFT_MEDIUMGREEN 0x04A2
#define TFT_DARKRED 0xA800
#define TFT_MEDIUMBLUE 0x3D5F
#define TFT_DARKORANGE 0xFAC0
#define TFT_LEFT    1
#define TFT_CENTER  2
#define TFT_RIGHT   3
#define TFT_TRANSP  true
#define TFT_OBLQ  false

#define MAX_JOGSTR 50
#define MAX_STATUSSTR 50
#define LINEHEIGHT_DEF  40

enum machinestate {
  Unknwn,
  Alarm,
  Idle,
  Jog,
  Home,
  Check,
  Run,
  Cycle,
  Hold,
  Door,
  Sleep
};

PinButton buttonUp(buttonPinUp);
PinButton buttonDn(buttonPinDn);
PinButton buttonLe(buttonPinLe);
PinButton buttonRt(buttonPinRt);
PinButton buttonOK(buttonPinOK);
PinButton buttonTL(buttonPinTL);
PinButton buttonTM(buttonPinTM);
PinButton buttonTR(buttonPinTR);
PinButton buttonBL(buttonPinBL);
PinButton buttonBM(buttonPinBM);
PinButton buttonBR(buttonPinBR);

HardwareSerial Serial2(PB11, PB10);

static char mStateStr[][11] { "? ?", "ALM", "IDLE", "JOG", "HOME", "CHECK", "RUN", "CYCLE", "HOLD", "DOOR", "SLEEP" };
char jogStr[MAX_JOGSTR];
char statusStr[MAX_STATUSSTR];
char sbuf[20];
float mX, mY, mZ;  //Machine positions
float wX, wY, wZ;  //Work positions

float lastXPos = 0.1, lastYPos = 0.1, lastZPos = 0.1;
float lastXwPos = 0.1, lastYwPos = 0.1, lastZwPos = 0.1;

unsigned long sinceStart, lastGrblState;
int jogDistance[] = {1,5,10,50,100,200};
int jogIndex = 2;
bool zJog = 0;
bool laserStatus = 0;
const int updateInterval = 198;
bool doorValue = 0;
bool lastDoorValue = 1;

bool doorUpdated = false;
bool xyUpdated = true;
bool jogUpdated = true;

String grblState;
machinestate mState = Unknwn;
machinestate lastmState = Idle;

//************************************
// Functions
//************************************

void printAxisCoords(float m, float w, byte row) {
  tft.fillRect(35, row * LINEHEIGHT_DEF + 2, 285, LINEHEIGHT_DEF, TFT_BLACK); //clear to eol
  if (m > 0) tftPrint(TFT_LEFT, TFT_TRANSP, TFT_LIGHTBLUE, TFT_BLACK, 25, row * LINEHEIGHT_DEF, 0, "+");
  else if (m < 0) tftPrint(TFT_LEFT, TFT_TRANSP, TFT_LIGHTBLUE, TFT_BLACK, 25, row * LINEHEIGHT_DEF, 0, "-");
  sprintf(sbuf, "%03d.%03d", (int)abs(m) % 1000, (int)(fabsf(m) * 1000) % 1000);
  tftPrint(TFT_LEFT, TFT_TRANSP, TFT_LIGHTBLUE, TFT_BLACK, 40, row * LINEHEIGHT_DEF + 2, 0, sbuf);
  if (w > 0) tftPrint(TFT_LEFT, TFT_TRANSP, TFT_WHITE, TFT_BLACK, 170, row * LINEHEIGHT_DEF, 0, "+");
  else if (w < 0) tftPrint(TFT_LEFT, TFT_TRANSP, TFT_WHITE, TFT_BLACK, 170, row * LINEHEIGHT_DEF, 0, "-");
  sprintf(sbuf, "%03d.%03d", (int)abs(w) % 1000, (int)(fabsf(w) * 1000) % 1000);
  tftPrint(TFT_LEFT, TFT_TRANSP, TFT_WHITE, TFT_BLACK, 185, row * LINEHEIGHT_DEF + 2, 0, sbuf);
}

void tftUpdate(bool active) {
  if(active){ // Only bother if machines is not idle
    if (lastXPos != mX || lastXwPos != wX){ //Only update if the values have changed
      tftPrint(TFT_LEFT, TFT_TRANSP, TFT_WHITE, TFT_BLACK, 2, 0 * LINEHEIGHT_DEF + 2, 0, "X");
      printAxisCoords(mX, mX - wX, 0);
      lastXPos = mX;
      lastXwPos = wX;
    }
    if (lastYPos != mY || lastYwPos != wY){
      tftPrint(TFT_LEFT, TFT_TRANSP, TFT_WHITE, TFT_BLACK, 2, 1 * LINEHEIGHT_DEF + 2, 0, "Y");
      printAxisCoords(mY, mY - wY, 1);
      lastYPos = mY;
      lastYwPos = wY;
    }
    if (lastZPos != mZ || lastZwPos != wZ){
      tftPrint(TFT_LEFT, TFT_TRANSP, TFT_WHITE, TFT_BLACK, 2, 2 * LINEHEIGHT_DEF + 2, 0, "Z");
      printAxisCoords(mZ, mZ - wZ, 2);
      lastZPos = mZ;
      lastZwPos = wZ;
    }
  }
  sprintf(sbuf, "%s", mStateStr[mState]);

  if (mState == Door){
    if (lastDoorValue != doorValue){
      if (doorValue == 0){
        tftPrintBig(TFT_ORANGE, TFT_BLACK, 160, 220, 120, sbuf); // Flag to resume
      }else{
        tftPrintBig(TFT_DARKRED, TFT_BLACK, 160, 220, 120, sbuf); //Halted
      }
      lastDoorValue = doorValue;
    }
  } 

  if(lastmState != mState && mState != Door){
    if (mState == Alarm || mState == Unknwn)
      tftPrintBig(TFT_DARKRED, TFT_BLACK, 160, 220, 120, sbuf);
    else if (mState == Hold)
      tftPrintBig(TFT_ORANGE, TFT_BLACK, 160, 220, 120, sbuf);
    else if (mState == Run || mState == Jog)
      tftPrintBig(TFT_BLACK, TFT_ORANGE, 160, 220, 120, sbuf);
    else tftPrintBig(TFT_BLACK, TFT_MEDIUMGREEN, 160, 220, 120, sbuf);
    lastmState = mState;
  }
  
  // Only update when needed.
  if (jogUpdated == true){
    tftPrintBig(TFT_MEDIUMGREEN, TFT_BLACK, 50, 220, 80, String(jogDistance[jogIndex]));
    jogUpdated = false;
  }
  if (xyUpdated == true){
    if (zJog == 0){
        tftPrintBig(TFT_MEDIUMGREEN, TFT_BLACK, 270, 220, 80, "X/Y");
        xyUpdated = false;
    }else{
      tftPrintBig(TFT_DARKORANGE, TFT_BLACK, 270, 220, 80, "Z");
      xyUpdated = false;
    }
  }
}

void tftPrint(int align, bool transp, int fg, int bg, byte col, byte row, byte len, String txt) {
  //tft.setFreeFont(&FreeSans9pt7b);
  tft.setFreeFont(&FreeSansBold12pt7b);
  if (transp) tft.setTextColor(fg);
  else {
    tft.setTextColor(fg, bg);
    if (align == TFT_RIGHT) {
      if (len) tft.fillRect(col - len, row, len, LINEHEIGHT_DEF, TFT_BLACK);
      tft.setTextDatum(TR_DATUM);
    }
    else if (align == TFT_CENTER) {
      if (len) tft.fillRect(col - len / 2, row, len, LINEHEIGHT_DEF, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
    }
    else {
      if (len) tft.fillRect(col, row, len, LINEHEIGHT_DEF, TFT_BLACK);
      tft.setTextDatum(TL_DATUM);
    }
  }
  tft.drawString(txt, col, row);
  tft.setTextDatum(TL_DATUM);
}

void tftPrintBig(int bg, int fg, int col, int row, int len, String txt) {
  tft.setTextDatum(MC_DATUM);
  //tft.fillRect(col - len / 2, row - LINEHEIGHT_DEF - 10, len, 2 * LINEHEIGHT_DEF - 10 , bg);
  tft.fillSmoothRoundRect(col - (len / 2), row - LINEHEIGHT_DEF - 10, len, 2 * LINEHEIGHT_DEF - 10 , 8,bg,TFT_BLACK);
  //                         255-110/2,
  tft.setTextColor(fg);
  tft.setFreeFont(&FreeSansBold18pt7b);
  tft.drawString(txt, col, row - 17);
  tft.setTextDatum(TL_DATUM);
}
void tftPrintColor(int color, byte col, byte row, String txt) {
  tftPrint(TFT_LEFT, TFT_OBLQ, color, TFT_BLACK, col, row, 0, txt);  
}

void tftPrintSimple(byte col, byte row, String txt) {
  tftPrint(TFT_LEFT, TFT_OBLQ, TFT_WHITE, TFT_BLACK, col, row, 0, txt);
}

void getGrblState(bool full) {
  char a;
  Serial2.write('?');
  grblState = "";
  while (Serial2.available()) {
    a = Serial2.read();
    Serial.write(a);
    if (a == '\n') {
      if (grblState.indexOf("<") >= 0) {
        grblState.remove(0, grblState.indexOf("<"));
        //Serial.println(grblState);
        if (grblState.charAt(1) == 'J') mState = Jog;
        else if (grblState.charAt(3) == 'm') mState = Home;
        else if (grblState.charAt(4) == 'd') mState = Hold;

        // Handle the door state
        else if (grblState.charAt(1) == 'D'){
           mState = Door; //Door is open
           if (grblState.charAt(6) == '0'){
             doorValue = 0; //Door was opened but is now close
           }else{
             doorValue = 1; //Door is open
           }
        }
        else if (grblState.charAt(1) == 'R') mState = Run;
        else if (grblState.charAt(1) == 'A') mState = Alarm;
        else if (grblState.charAt(1) == 'I') mState = Idle;
        else mState = Unknwn;
        if (full) {
          if (grblState.indexOf("MPos:") >= 0) {
            grblState.remove(0, grblState.indexOf("MPos:") + 5);
            convertPos(true, grblState);
          }
          if (grblState.indexOf("|FS:") >= 0) {
            grblState.remove(0, grblState.indexOf("|FS:") + 4);
            grblState.remove(0, grblState.indexOf(",") + 1);
            //reportedSpindleSpeed = grblState.toInt();
          }
          if (grblState.indexOf("WCO:") >= 0) {
            grblState.remove(0, grblState.indexOf("WCO:") + 4);
            convertPos(false, grblState);
          }
          else if (grblState.indexOf("|Ov:") >= 0) {
            grblState.remove(0, grblState.indexOf("|Ov:") + 4);
            //convertOverride(grblState);
            if (grblState.indexOf("|A:") >= 0) {
              grblState.remove(0, grblState.indexOf("|A:") + 3);
            }
          }
        }
      }
    } else {
      grblState.concat(a);
    }
  }
  lastGrblState = sinceStart;
}

void convertPos(bool isM, String s) {
  // Serial.println(s);
  static float x, y, z;
  x = s.substring(0, s.indexOf(",")).toFloat();
  s.remove(0, s.indexOf(",") + 1);
  y = s.substring(0, s.indexOf(",")).toFloat();
  s.remove(0, s.indexOf(",") + 1);
  if (s.indexOf("|") < 7) {
    z = s.substring(0, s.indexOf("|")).toFloat();
  } else {
    z = s.substring(0, s.indexOf(">")).toFloat();
  }
  if (isM) {
    mX = x;
    mY = y;
    mZ = z;
  } else {
    wX = x;
    wY = y;
    wZ = z;
  }
}

void forceEndJog() {
  Serial.println("C");
  Serial2.write(0x85);
  //  Serial.println("cancel");
  delay(100);
  getGrblState(false);
  while (mState == Jog) {
    Serial.println("D");
    Serial2.write(0x85);
    //    Serial.println("cancel");
    delay(100);
    getGrblState(true);
  }
}

void waitEndJog() {
  getGrblState(false);
  while (mState == Jog) {
    delay(100);
    getGrblState(true);
    //    Serial.print("jog wait...");
    //    Serial.println(mState);
  }
}

//************************************
// End of Functions
//************************************


void setup() {
  Serial.begin(115200);
  Serial2.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  grblState.reserve(100);
  getGrblState(false);
  Serial.println("Init...");
  tftUpdate(true);
  delay(2000); //Wait for the buttons to settle
}

void loop() {
  sinceStart = millis() + 1000;
  if (sinceStart - lastGrblState > updateInterval) {
    if (mState != Alarm && mState != Idle && mState != Unknwn && mState != Door){
      getGrblState(true); //Machine is doing something
      tftUpdate(true); //Update full details
    }else{
      getGrblState(false); // Machine is idle, just get status
      tftUpdate(false); //Just update status
    }
  }
  buttonUp.update();
  buttonDn.update();
  buttonLe.update();
  buttonRt.update();
  buttonOK.update();
  buttonTL.update();
  buttonTM.update();
  buttonTR.update();
  buttonBL.update();
  buttonBM.update();
  buttonBR.update();

  if(buttonTL.isClick()){
    //Home
    Serial2.println((char *)"$H");
  }
  
  if(buttonBL.isClick()){
    //Home Z Azis
    Serial2.println((char *)"$HZ");
  }
  
  if(buttonTM.isClick()){
    if(mState == Alarm){ 
      Serial2.println((char *)"$X"); //Unlock
    }
      else if (mState == Run) Serial2.write('!');
      else if (mState == Hold) Serial2.write('~');
      else if (mState == Door && doorValue == 0){ //No point unless door is now closed.
        Serial2.write('~');
      } 
  }
  
  if(buttonBM.isClick()){
    if (laserStatus == 0){
      Serial2.println((char *)"M62 P0");
      laserStatus = 1;
      //tft.drawLine(310,5,310, )
      tft.drawWideLine(300, 5, 300, 80, 3, TFT_DARKRED, TFT_BLACK);
      tft.fillCircle(300, 80, 7, TFT_DARKRED);
    }else{
      Serial2.println((char *)"M63 P0");
      laserStatus = 0;
      tft.fillRect(293, 5, 15, 90, TFT_BLACK);
    }
  }

  if(buttonTR.isClick()){
    if (jogIndex<5){
      jogIndex++;
      jogUpdated = true;
      //tftPrintBig(TFT_MEDIUMGREEN, TFT_BLACK, 50, 220, 80, String(jogDistance[jogIndex]));
    }
  }

  if(buttonBR.isClick()){
    if (jogIndex>0){
      jogIndex--;
      jogUpdated = true;
      //tftPrintBig(TFT_MEDIUMGREEN, TFT_BLACK, 50, 220, 80, String(jogDistance[jogIndex]));
    }
  }

  if(buttonUp.isClick()){
    if (zJog == 0){ // Jog Y Axis
      int dist = jogDistance[jogIndex];
      snprintf(jogStr, MAX_JOGSTR, "$J=G21G91X0Y%dZ0F%d", dist, 3000);
    }else{ // Jog Z Axis
      int dist = -1 * jogDistance[jogIndex];
      snprintf(jogStr, MAX_JOGSTR, "$J=G21G91X0Y0Z%dF%d", dist, 1000);
    }
    Serial2.println(jogStr);
    Serial.println(jogStr);
  }
  if(buttonDn.isClick()){
    if (zJog == 0){ // Jog Y Axis
      int dist = -1 * jogDistance[jogIndex];
      snprintf(jogStr, MAX_JOGSTR, "$J=G21G91X0Y%dZ0F%d", dist, 3000);
    }else{
      int dist = jogDistance[jogIndex];
      snprintf(jogStr, MAX_JOGSTR, "$J=G21G91X0Y0Z%dF%d", dist, 1000);
    }
    Serial2.println(jogStr);
    Serial.println(jogStr);
  }
  if(buttonLe.isClick()){
    if (zJog == 0){  // Only Jog if Z is not enabled
      int dist = -1 * jogDistance[jogIndex];
      snprintf(jogStr, MAX_JOGSTR, "$J=G21G91X%dY0Z0F%d", dist, 3000);
      Serial2.println(jogStr);
      Serial.println(jogStr);
    }
  }
  if(buttonRt.isClick()){
    if (zJog == 0){ // Only Jog if Z is not enabled
      int dist = jogDistance[jogIndex];
      snprintf(jogStr, MAX_JOGSTR, "$J=G21G91X%dY0Z0F%d", dist, 3000);
      Serial2.println(jogStr);
      Serial.println(jogStr);
    }
  }
  if(buttonOK.isClick()){
    if (zJog == 0){
      zJog = 1;
      //tftPrintBig(TFT_DARKORANGE, TFT_BLACK, 270, 220, 80, "Z");
      xyUpdated = true;
    }else{
      zJog = 0;
      //tftPrintBig(TFT_MEDIUMGREEN, TFT_BLACK, 270, 220, 80, "X/Y");
      xyUpdated = true;
    }
  }
}
