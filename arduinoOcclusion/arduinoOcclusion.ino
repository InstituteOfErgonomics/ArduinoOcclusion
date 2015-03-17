//------------------------------------------------------
//Revision History 'Arduino Occlusion'
//------------------------------------------------------
//Version  Date		Author		  Mod
//1        July, 2014	Michael Krause	  initial
//2        Oct, 2014	Michael Krause	  ethernet
//
//------------------------------------------------------
/*
    
        The MIT License (MIT)

        Copyright (c) 2015 Michael Krause (krause@tum.de), Institute of Ergonomics, Technische Universität München

        Permission is hereby granted, free of charge, to any person obtaining a copy
        of this software and associated documentation files (the "Software"), to deal
        in the Software without restriction, including without limitation the rights
        to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
        copies of the Software, and to permit persons to whom the Software is
        furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be included in
        all copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
        AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
        OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
        THE SOFTWARE.
*/
#include <SPI.h>
#include <Ethernet.h>

//---------------------------------------------------------------------------
//CONST
const int BUTTON_START_STOP_PIN = 2;//to start/stop experiment (we nedd the ISR pin 2)
const int BUTTON_TOGGLE_PIN = 6;//button to toggle open/close if experiment not running
const int LED_EXP_RUNNING_PIN = 7; // show if experiment is running
const int CONTROL_LEFT_PIN = 8; // pin to control open/close of external (milgram) googles 
const int CONTROL_RIGHT_PIN = 9; //  pin to control open/close of external (milgram) googles
//10,11,12,13 is ethernet shield?!
const int CONTROL_PDLC_PIN = 1; //  pin to control open/close of PDLC
const int LED_OCC_PIN = 3; //  pin to show occ open/close controll
const int LED_CONNECTED_PIN = 5; //  pin to show ip/port connected LED


const unsigned long OPEN_US = 1500000;//micro seconds
const unsigned long CLOSED_US = 1500000;//micro seconds



  // Enter a MAC address for your controller below.
  // Newer Ethernet shields have a MAC address printed on a sticker on the shield
  byte gMac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x03 };
  IPAddress gIp(192,168,1,18);
  // the router's gateway address:
  //byte gGateway[] = { 192, 168, 2, 1 };
  // the subnet:
  //byte gSubnet[] = { 255, 255, 255, 0 };
  
  EthernetServer gServer(7009);//enter PORT number
  EthernetClient gClient;

//global VARs
volatile byte gExpRunningF = false; //flag, set by startExperiment, cleared by stopExperiment
volatile byte gUnhandeledPushEventF = false; //flag set by ISR, cleared by program loop
volatile byte gUnhandeledReleaseEventF = false; //flag set by ISR, cleared by program loop

volatile byte gOpen = false; //currently open or closed?
volatile unsigned long gButtonDownT;//start of button down, in uptime micro sec
volatile unsigned long gHoldStartT;//start of button hold, in uptime micro sec
volatile unsigned long gHoldStopT; //stop of button hold, in uptime micro sec
unsigned long gExpStartT;//start of experiment in uptime, micro sec
unsigned long gLastBeacon;//send time of last beacon

int gButtonState;//button state of start/stop button

//---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

   Ethernet.begin(gMac,gIp);
   //Ethernet.begin(mac, ip, gateway, subnet);  
   gServer.begin();

  //BUTTON_START_STOP_PIN
  pinMode(BUTTON_START_STOP_PIN, INPUT); 
  digitalWrite(BUTTON_START_STOP_PIN, HIGH); // pullUp on  
  attachInterrupt(0, buttonISR, CHANGE);

  //BUTTON_TOGGLE_PIN
  pinMode(BUTTON_TOGGLE_PIN, INPUT); 
  digitalWrite(BUTTON_TOGGLE_PIN, HIGH); // pullUp on  

  //OUTPUTS
  pinMode(LED_EXP_RUNNING_PIN, OUTPUT); 
  pinMode(CONTROL_LEFT_PIN, OUTPUT); 
  pinMode(CONTROL_RIGHT_PIN, OUTPUT); 
  pinMode(CONTROL_PDLC_PIN, OUTPUT); 
  pinMode(LED_OCC_PIN, OUTPUT); 
  pinMode(LED_CONNECTED_PIN, OUTPUT); 

  //blink two times
  digitalWrite(LED_EXP_RUNNING_PIN, HIGH);  
  digitalWrite(LED_OCC_PIN, HIGH);  
  digitalWrite(LED_CONNECTED_PIN, HIGH); 
  delay(250); 
  digitalWrite(LED_EXP_RUNNING_PIN, LOW);  
  digitalWrite(LED_OCC_PIN, LOW);  
  digitalWrite(LED_CONNECTED_PIN, LOW); 
  delay(250); 
  digitalWrite(LED_EXP_RUNNING_PIN, HIGH);  
  digitalWrite(LED_OCC_PIN, HIGH);  
  digitalWrite(LED_CONNECTED_PIN, HIGH); 
  delay(250); 
  digitalWrite(LED_EXP_RUNNING_PIN, LOW);  
  digitalWrite(LED_OCC_PIN, LOW);  
  digitalWrite(LED_CONNECTED_PIN, LOW); 
  delay(250); 


  //init gButtonState global var
  gButtonState = digitalRead(BUTTON_START_STOP_PIN);
  
  
  //Serial.println("Setup finished.");
}
//-------------------------------------------------------------------------------------
void openShutter(){
  gOpen = true;
  digitalWrite(CONTROL_LEFT_PIN, LOW);
  digitalWrite(CONTROL_RIGHT_PIN, LOW);
  digitalWrite(CONTROL_PDLC_PIN, LOW);
  digitalWrite(LED_OCC_PIN, HIGH);
  
  gServer.write('o');
}
//-------------------------------------------------------------------------------------
void closeShutter(){
  gOpen = false;
  digitalWrite(CONTROL_LEFT_PIN, HIGH);
  digitalWrite(CONTROL_RIGHT_PIN, HIGH);
  digitalWrite(CONTROL_PDLC_PIN, HIGH);
  digitalWrite(LED_OCC_PIN, LOW);
 
  gServer.write('c');
  
}
//-------------------------------------------------------------------------------------
void toggle(){
  if(gOpen){
    closeShutter();
  }else{
    openShutter();    
  }  
}
//-------------------------------------------------------------------------------------
void loop() {
  
  
   //continiously refresh buttonState
  gButtonState = digitalRead(BUTTON_START_STOP_PIN);
  
  
  
  if ((gUnhandeledPushEventF)&&(gExpRunningF)){
         stopExp();
         gUnhandeledPushEventF = false;
  }
  if ((gUnhandeledPushEventF)&&(!gExpRunningF)){
         startExp();
         gUnhandeledPushEventF = false;
  }

  int inByte = 0; //reset in every loop
  
   if ((gClient) && (!gClient.connected())) {//stop and null if disconnected
    gClient.stop();
  }
  

  
  if ((gClient) && (gClient.connected())){
    digitalWrite(LED_CONNECTED_PIN, HIGH); 
  }else{
   digitalWrite(LED_CONNECTED_PIN, LOW); 
      gClient = gServer.available();
  }
  
   //listen on serial line ------------------------------
  if (Serial.available() > 0) inByte = Serial.read();//read in
  if ((gClient) && (gClient.available())){inByte = gClient.read();} 


  if (inByte != 0){
  
    if (((inByte == '#')||(inByte == 32)) && (!gExpRunningF)) startExp();//start experiment with '#' or 32 (=SPACE)
    if (((inByte == '$')||(inByte == 27)) &&  (gExpRunningF)) stopExp();//stop experiment with '$' or 27 (=ESC)
    if ((inByte == 't') && (!gExpRunningF)){//'t' toggle shutter open/close if experiment not runing
      toggle();
    }
  }  
 
  if (gExpRunningF){ handleOpenClose();}//do the occlusion protocoll if experiment is running
  else {handleToggleButton();}//if experiment not running check toggle button
  
  //ready beacon  
   unsigned long now = micros();
   if(now - gLastBeacon > 1000000){//1 sec = 1000000us
      gServer.write('R'); //ready beacon
      gLastBeacon = now;
   }
   
}
//-------------------------------------------------------------------------------------
void startExp(){
  
    gServer.write('#');

    gExpRunningF = true;
    
    digitalWrite(LED_EXP_RUNNING_PIN,HIGH);//led on
    
    gExpStartT = micros();
    
    openShutter();
}
//-------------------------------------------------------------------------------------
void stopExp(){
  
    gServer.write('$');

    gExpRunningF = false; 
    
    unsigned long experimentT = (micros() - gExpStartT);
    unsigned long div =  experimentT / (OPEN_US + CLOSED_US);
    unsigned long mod =  experimentT % (OPEN_US + CLOSED_US);
    unsigned long add = mod;//additional time from mod, thats added to (div*OPEN_US)
    if (add > OPEN_US) add = OPEN_US;

    digitalWrite(LED_EXP_RUNNING_PIN,LOW);//led off
    
    closeShutter(); 
    
    unsigned long tsot = (div * OPEN_US) + add ;
    
    Serial.println("--------------------");    
    Serial.print("TTT: ");    
    Serial.println((float)experimentT/1000000);    
    Serial.print("TSOT:");    
    Serial.println((float)tsot/1000000);    

}
//-------------------------------------------------------------------------------------
void handleToggleButton() {//called in every loop
  
  const unsigned int TCOUNT_ACTION_AT = 20;
  const unsigned int TCOUNT_PRESSED_AND_HANDLED = 21;
  
  static unsigned long tDownOld; 
  static unsigned long tCount; 
  
  int tButton = digitalRead(BUTTON_TOGGLE_PIN);
  if (tButton == LOW){//if pressed
     if (millis() != tDownOld){
       tCount++;
       tDownOld = millis();
     }
     if (tCount == TCOUNT_ACTION_AT){//after the button is pressed a while; kind of debouncing
       toggle();
       tCount = TCOUNT_PRESSED_AND_HANDLED;
     }  
     if (tCount > TCOUNT_ACTION_AT) tCount = TCOUNT_PRESSED_AND_HANDLED;//no integer overflow of tCount, tCount is catched at TCOUNT_PRESSED_AND_HANDLED;

   }else{//if not pressed
     tCount=0;
  }
}
//-------------------------------------------------------------------------------------
void handleOpenClose(){

  unsigned long experimentT = (micros() - gExpStartT);  
  unsigned long mod =  experimentT % (OPEN_US + CLOSED_US);
  
  if ((mod < OPEN_US) && (!gOpen)){
        openShutter();
  }
  
  if ((mod >= OPEN_US) && (gOpen)){
        closeShutter();
  }
}
//-------------------------------------------------------------------------------------
volatile unsigned long gLastEdge = 0;
const unsigned long BOUNCING = 15000;//edges within BOUNCING micro secs after last handeled edge are discarded

void buttonISR(){
    
    unsigned long now = micros();
      
    if (now - gLastEdge < BOUNCING){
      gLastEdge = now;
      return; //discard
    }

    gLastEdge = now;
    
    //workaround for buttonState problem (sometimes the bouncing is so fast thta the pin state changes before/while ISR starting)
    int isrButtonState = digitalRead(BUTTON_START_STOP_PIN);
    if (isrButtonState == gButtonState){ // new == old?
      //here is something wrong, the ISR is issued by a change and now we detect no change (new value == old value)?! 
      //we trust the gButtonState (old stored value before the ISR) more and invert isrButtonState
      if(isrButtonState==HIGH){
        isrButtonState = LOW;
      }else{
        isrButtonState=HIGH;
      }
    }


   if (isrButtonState==HIGH){//this is a button release
      if (!gUnhandeledReleaseEventF){  
        gHoldStopT = now;
        gUnhandeledReleaseEventF = true;
      }  
    }
    else{ //this is a button press 

      if (!gUnhandeledPushEventF){  
        gButtonDownT = now;
        gUnhandeledPushEventF = true;
      }  
    }

}

//-------------------------------------------------------------------------------------


