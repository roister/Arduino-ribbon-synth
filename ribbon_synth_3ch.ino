/*
* Arduino Ribbon Synth MIDI controller
* ------------------------------------
* ©2014 Dean Miller
*/

#include <EEPROM.h>

//------- Constants -------//
#define PIEZO_THRESHOLD_ON 30

#define PIN_LED 13
#define PIN_SOFTPOT_1 A0
#define PIN_SOFTPOT_2 A1
#define PIN_SOFTPOT_3 A2
#define PIN_PIEZO_1 A3
#define PIN_PIEZO_2 A4
#define PIN_PIEZO_3 A5
#define PIN_POT_1 8
#define PIN_POT_2 6

#define PIN_BUTTON_STICK 3

#define PIN_BUTTON_RIGHT 11
#define PIN_BUTTON_UP 5
#define PIN_BUTTON_DOWN 2
#define PIN_BUTTON_LEFT 7

#define PIN_JOYSTICK_X 9
#define PIN_JOYSTICK_Y 10

#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3
#define STICK 4

#define PIEZO_SAMPLES 400
#define NUM_STRINGS 3
#define PADDING 3

//------Note Class---------//
/*
* A note class that stores some info about each note played is necessary
* to ensure that open strings are held for the specified amount of time.
* That is a problem with using the piezos as triggers instead of FSRs, they
* only register momentary impact or vibration, creating a problem for open strings.
*/

class Note {
  int _number;
  int _velocity;
  int _startTime;
  int _fretted;
  
  public:
  void init(int number, int velocity, int startTime, int fretted) {
    _number = number;
    _velocity = velocity;
    _startTime = startTime;
    _fretted = fretted;
  }
  
  int number() {
    return _number;
  }
  
  int velocity() {
    return _velocity;
  }
  
  int fretted() {
    return _fretted;
  }
  
  int timeActive() {
    return millis() - _startTime;
  }
};

//------ Global Variables ---------//

/*
fret defs stored in EEPROM for calibration purposes.
Lower output voltages from USB ports result in different values read from
SoftPots and wonky fret definitions.
*/
int F0 = 220;
int F1_1, F2_1, F3_1, F4_1, F5_1, F6_1, F7_1, F8_1, F9_1, F10_1, F11_1, F12_1, F13_1, F14_1, F15_1, F16_1, F17_1, F18_1, F19_1, F20_1;
int F1_2, F2_2, F3_2, F4_2, F5_2, F6_2, F7_2, F8_2, F9_2, F10_2, F11_2, F12_2, F13_2, F14_2, F15_2, F16_2, F17_2, F18_2, F19_2, F20_2;
int F1_3, F2_3, F3_3, F4_3, F5_3, F6_3, F7_3, F8_3, F9_3, F10_3, F11_3, F12_3, F13_3, F14_3, F15_3, F16_3, F17_3, F18_3, F19_3, F20_3;
int F21 = 0;

int fretDefs[3][22];

int piezoVals[] = {0, 0, 0};
int piezoPins[] = {PIN_PIEZO_1, PIN_PIEZO_2, PIN_PIEZO_3};

int softPotVals[3];
int softPotPins[] = {PIN_SOFTPOT_1, PIN_SOFTPOT_2, PIN_SOFTPOT_3};

int potVal1Old = -1;
int potVal2Old = -1;
int softPotValsOld[] = {0, 0, 0};

int fretTouched[3];
int noteFretted[3];

int stringActive[] = {false, false, false};
int stringPlucked[] = {false, false, false};

Note *activeNotes[3];

int calibrationMax[] = {0, 0, 0};
int calibrationMin[3];

//true for low strings, false for high strings
int stringSetLow = true;
int octave = 3;

//E A D
int offsets_low[] = {16, 21, 26};

//G B E
int offsets_high[] = {19, 23, 28};

//default offsets
int offsets[] = {52, 57, 62};

//states of control buttons
int buttonStates[] = {false, false, false, false, false};
int stickActive = false;
int stickZeroX = 0;
int stickZeroY = 0;
int stickState = false;

int altControlSet = false;
int stickXY = false;
int fullLegatoMode = false;

int minDurationOpen = 75;
int minVelocity = 75;

//--------- Setup ----------------//
void setup() {
  
  //read fret definitions from EEPROM
  for (int i=0; i<NUM_STRINGS; i++){
    fretDefs[i][0] = F0;
    for (int j=1; j<21; j++){
      fretDefs[i][j] = EEPROM.read(j + (21*i));
    }
    fretDefs[i][21]=0;
    calibrationMin[i] = EEPROM.read(21 + (21*i));
  }
  
  //begin at MIDI spec baud rate
  Serial1.begin(31250);
  pinMode(PIN_SOFTPOT_1, INPUT);
  pinMode(PIN_SOFTPOT_2, INPUT);
  pinMode(PIN_SOFTPOT_3, INPUT);
  pinMode(PIN_PIEZO_1, INPUT);
  pinMode(PIN_PIEZO_2, INPUT);
  pinMode(PIN_PIEZO_3, INPUT);
  digitalWrite(PIN_SOFTPOT_1, HIGH);
  digitalWrite(PIN_SOFTPOT_2, HIGH); 
  digitalWrite(PIN_SOFTPOT_3, HIGH);
  
  pinMode(PIN_BUTTON_RIGHT, INPUT);  
  digitalWrite(PIN_BUTTON_RIGHT, HIGH);
 
  pinMode(PIN_BUTTON_LEFT, INPUT);  
  digitalWrite(PIN_BUTTON_LEFT, HIGH);
 
  pinMode(PIN_BUTTON_UP, INPUT);  
  digitalWrite(PIN_BUTTON_UP, HIGH);
 
  pinMode(PIN_BUTTON_DOWN, INPUT);  
  digitalWrite(PIN_BUTTON_DOWN, HIGH);
 
  pinMode(PIN_BUTTON_STICK, INPUT);  
  digitalWrite(PIN_BUTTON_STICK, HIGH);  
  
  
  pinMode(PIN_LED, OUTPUT);
  
  while(millis() < 500) {
    for (int i=0; i<NUM_STRINGS; i++){
      int val = analogRead(softPotPins[i]);
      if (val > calibrationMax[i]) calibrationMax[i] = val;
    }
    
    //calibrate joystick
    stickZeroX = analogRead(PIN_JOYSTICK_X);
    stickZeroY = analogRead(PIN_JOYSTICK_Y);
    }
}

//----------Main Loop---------------//
void loop() {
 //reset
 for (int i=0; i<NUM_STRINGS; i++) {
   stringPlucked[i] = false;
   piezoVals[i] = false;
 }
  
 //read values of all sensors 
 readSensors();

 determineFrets();
 
 //if we are in full legato mode, run the function
 if (fullLegatoMode) {
   fullLegato();
 }
 
 //otherwise just do the regular thing
 else {
   //test for legato action
   legatoTest();
 
   //use this info to determine which notes to pluck
   pickNotes();
 }
 
 //send not off messages and reset necessary things
 cleanUp();
 
 //check for control changes
 readControls();
}

void readSensors() {
  for (int i=0; i<NUM_STRINGS; i++) {
    
    //read piezo vals
    int piezoVal = analogRead(piezoPins[i]);
    
    //if the value breaks the threshold read for max amplitude
    /* TODO: this is less than ideal. Have it determine which piezos were triggered and
    * then sample them all at once for better support for polyphonic stuff.
    */
    
    if (piezoVal > PIEZO_THRESHOLD_ON) {
      int v_new = piezoVal;
      for (int sample=0; sample<PIEZO_SAMPLES; sample++){
        piezoVal = analogRead(piezoPins[i]);
        if (piezoVal > v_new){
          v_new = piezoVal;
        }
      }
      piezoVals[i] = v_new;
      piezoVals[i] = map(piezoVals[i], 0, 500, 0, 127);
      piezoVals[i] = constrain(piezoVals[i], minVelocity, 127);
    }
    
    //read the value of all the softPots
    softPotVals[i] = analogRead(softPotPins[i]);
    softPotVals[i] = map(softPotVals[i], calibrationMin[i], calibrationMax[i], 0, 255);
    softPotVals[i] = constrain(softPotVals[i], 0, 255);
  }
}

void determineFrets () {
   //---------Get Fret Numbers------
 for (int i=0; i< NUM_STRINGS; i++) {
 
   int softPotVal = softPotVals[i];
    
    //check for open strings
    if (softPotVal >= F0) {
      softPotValsOld[i] = softPotVal;
      fretTouched[i]=0;
    }
    
    //loop through the array of fret definitions
    for (int j=1; j<21; j++) {
      
      int k = j-1;
      if (softPotVal <= fretDefs[i][k] && 
          softPotVal > fretDefs[i][j] &&
          abs(softPotVal-softPotValsOld[i]) > PADDING) {
            
            softPotValsOld[i] = softPotVal;
            fretTouched[i] = j;
          }
    }
    
    if (softPotVal <= fretDefs[i][20]) {
      softPotValsOld[i] = softPotVal;
      fretTouched[i]=21;
    }
  }
}

void pickNotes() {
  for (int i=0; i< NUM_STRINGS; i++) {
    
    //if the piezo was hit, play the fretted note
    if (piezoVals[i]){
      switch (i) {
        case 0:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
        case 1:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
        case 2:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
      }
      
      if (stringActive[i]){
        
        //turn off the currently active note on that string
        noteOff(0x80 + i, activeNotes[i]->number(), 0);
        free(activeNotes[i]);
        }
      
      if (!stringActive[i]) {
        
        //mark string as active
        stringActive[i] = true;
      }
        //register with active notes
        activeNotes[i] = (Note *) malloc(sizeof(Note));
        
        if (fretTouched[i] > 0) activeNotes[i]->init(noteFretted[i], piezoVals[i], millis(), true);
      
        else activeNotes[i]->init(noteFretted[i], piezoVals[i], millis(), false);
        
        //turn on fretted note
        noteOn(0x90 + i, activeNotes[i]->number(), activeNotes[i]->velocity());
      
        //mark that the string was plucked
        stringPlucked[i] = true; 
      }
    }
}

void legatoTest() {
  for (int i=0; i< NUM_STRINGS; i++) {
    if (stringActive[i]) {
      switch (i) {
        case 0:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
        case 1:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
        case 2:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
      }
    
    if (noteFretted[i] != activeNotes[i]->number() && fretTouched[i]) {
      //turn on new note
      int vel = activeNotes[i]->velocity();
      noteOn(0x90 + i, noteFretted[i], vel);
      
      //turn off old note
      noteOff(0x80 + i, activeNotes[i]->number(), 0);
      free(activeNotes[i]);
      
      //register new note as the active one
      activeNotes[i] = (Note *) malloc(sizeof(Note));
      activeNotes[i]->init(noteFretted[i], vel, millis(), true);
      }
    }
  }
}

void fullLegato() {
  for (int i=0; i<NUM_STRINGS; i++) {
    if (fretTouched[i]) {
      
      switch (i) {
        case 0:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
        case 1:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
        case 2:
          noteFretted[i] = fretTouched[i] + offsets[i];
          break;
      }
      
      int vel = 80;
      
      if (!stringActive[i]) {
        noteOn(0x90 + i, noteFretted[i], vel);
        
        //register new note as the active one
        activeNotes[i] = (Note *) malloc(sizeof(Note));
        activeNotes[i]->init(noteFretted[i], vel, millis(), true);
        stringActive[i] = true;
      }
      else {
        
        if (noteFretted[i] != activeNotes[i]->number()) {
          int vel = 80;
          noteOn(0x90 + i, noteFretted[i], vel);
          
          //turn off old note
          noteOff(0x80 + i, activeNotes[i]->number(), 0);
          free(activeNotes[i]);
        
          //register new note as the active one
          activeNotes[i] = (Note *) malloc(sizeof(Note));
          activeNotes[i]->init(noteFretted[i], vel, millis(), true);
        }
      }
    }
  }
}

void cleanUp() {
  for (int i=0; i< NUM_STRINGS; i++) {
    
    //no fret is touched and the string is marked active
    if (!fretTouched[i] && stringActive[i]){
      
      //if open string
      if (!activeNotes[i]->fretted()) {
        if (activeNotes[i]->timeActive() > minDurationOpen) {
          //turn off the active note
          noteOff(0x80 + i, activeNotes[i]->number(), 0);
      
          //mark as inactive
          stringActive[i] = false;
          free(activeNotes[i]);
        }
      }
     else { 
      //turn off the active note
      noteOff(0x80 + i, activeNotes[i]->number(), 0);
      
      //mark as inactive
      stringActive[i] = false;
      free(activeNotes[i]);
     }
    }
  }
}

void readControls() {
  
  if (altControlSet){
    minDurationOpen = analogRead(PIN_POT_1);
    minDurationOpen = map(minDurationOpen, 0, 1023, 0, 255);
  
    minVelocity = analogRead(PIN_POT_2);
    minVelocity = map(minVelocity, 0, 1023, 0, 127);
  }
  
  //Potentiometers set the values for controllers 12 and 13
  else {
    int potVal1New = analogRead(PIN_POT_1);
    int potVal2New = analogRead(PIN_POT_2);
  
    if (abs(potVal1New - potVal1Old) > 30 || potVal1New == 0 || potVal1New == 1023 ) {
      if ((potVal1New - potVal1Old) != 0){
        //Send MIDI control change message
        int val = map(potVal1New, 0, 1023, 0, 127);
        val = constrain(val, 0, 127);
        controllerChange(12, val); 
        
        potVal1Old = potVal1New;
      }
    }
  
    if (abs(potVal2New - potVal2Old) > 30 || potVal2New == 0 || potVal2New == 1023) {
      if ((potVal2New - potVal2Old) != 0){
        //Send MIDI control change message
        int val = map(potVal2New, 0, 1023, 0, 127);
        val = constrain(val, 0, 127);
        controllerChange(13, val);  
        potVal2Old = potVal2New;
      }
    }
  }
  
  //-----ENGAGE FULL LEGATO MODE-----
  //removes the need for triggering with piezo for fretted notes
  if (digitalRead(PIN_BUTTON_LEFT) == LOW && !buttonStates[LEFT]) {
    fullLegatoMode = true;
    buttonStates[LEFT] = true;
  }
  if (digitalRead(PIN_BUTTON_LEFT) == HIGH && buttonStates[LEFT]) {
    fullLegatoMode = false;
    buttonStates[LEFT] = false;
  }
  
  /*
  //switch to alt control set
  if (digitalRead(PIN_BUTTON_LEFT) == LOW && !buttonStates[LEFT]) {
    altControlSet = !altControlSet;
    buttonStates[LEFT] = true;
  }
  if (digitalRead(PIN_BUTTON_LEFT) == HIGH && buttonStates[LEFT]) buttonStates[LEFT] = false;
  
  //Right button triggers calibration
  if (digitalRead(PIN_BUTTON_RIGHT) == LOW && !buttonStates[RIGHT]) {
        buttonStates[RIGHT] = true;
        calibrate();
  }
  */
  
  //---- CHANGING THE OCTAVE -------//
  //UP and down buttons used to change offset/octave. Cycles through EAD and GBE like an infinite guitar neck.
  
  //---- UP BUTTON ----
  if (digitalRead(PIN_BUTTON_UP) == LOW) {
    //change the set of strings
    if (!buttonStates[UP]) {
      
      octave = octave - 1;
      
      stringSetLow = !stringSetLow;
      
      for (int i=0; i<NUM_STRINGS; i++) {
        //if low
        if (stringSetLow) {
          offsets[i] = offsets_low[i] + (12*octave);
        }
        //if high
        if (!stringSetLow) {
          offsets[i] = offsets_high[i] + (12*octave);
        }
      }
    }
    
    buttonStates[UP] = true;
  }
  //reset state once button is no longer being pressed
  if (digitalRead(PIN_BUTTON_UP) == HIGH && buttonStates[UP]) buttonStates[UP] = false;
  
  //----DOWN BUTTON----
  if (digitalRead(PIN_BUTTON_DOWN) == LOW) {
    //change the set of strings
    if (!buttonStates[DOWN]) {
      
      octave = octave + 1;
      
      stringSetLow = !stringSetLow;
      
      for (int i=0; i<NUM_STRINGS; i++) {
        //if low
        if (stringSetLow) {
          offsets[i] = offsets_low[i] + (12*octave);
        }
        //if high
        if (!stringSetLow) {
          offsets[i] = offsets_high[i] + (12*octave);
        }
      }
    }
    
    buttonStates[DOWN] = true;
  }
  //reset state once button is no longer being pressed
  if (digitalRead(PIN_BUTTON_DOWN) == HIGH && buttonStates[DOWN]) buttonStates[DOWN] = false;
  
  //switch stick to xy mode
  if (digitalRead(PIN_BUTTON_RIGHT) == LOW && !buttonStates[RIGHT]) {
    stickXY = !stickXY;
    buttonStates[RIGHT] = true;
  }
  if (digitalRead(PIN_BUTTON_RIGHT) == HIGH && buttonStates[RIGHT]) buttonStates[RIGHT] = false;
  
  
  //--------JOYSTICK-------//
  /* Click down the joystick to activate it. In regular mode it will read absolute position from the
  * center in any direction (sends to modwheel, MIDI controller 1), and in XY mode the x axis
  * sends to controller 2 (breath) and the y axis sends to controller 4 (foot).
  */
  
  if (digitalRead(PIN_BUTTON_STICK) == LOW) {
    //activate joystick
    if (!buttonStates[STICK]) {
      //make sure modwheel value is set to 0 when stick is off
      if (stickActive) controllerChange(1, 0);
      stickActive = !stickActive;
    }
    buttonStates[STICK] = true;
  }
  //reset once stick is no longer being pressed
  if (digitalRead(PIN_BUTTON_STICK) == HIGH && buttonStates[STICK]) buttonStates[STICK] = false;
  
  if (stickActive) {
    //read positions from center
    float xPos = map(analogRead(PIN_JOYSTICK_X), stickZeroX, 1023, 0, 127);
    float yPos = map(analogRead(PIN_JOYSTICK_Y), stickZeroY, 1023, 0, 127);
    
    //get absolute position from center
    float z = sqrt(sq(xPos) + sq(yPos));
    int stickVal = (int)constrain(z, 0, 127);
    
    if (stickVal > 0) {
      stickState = true;
      if (stickXY) {
        controllerChange(2, abs(xPos));
        controllerChange(4, abs(yPos));
      }
      else controllerChange(1, stickVal);
    }
    else if (stickState && stickVal == 0) {
      stickState = false;
      if (stickXY) {
        controllerChange(2, 0);
        controllerChange(4, 0);
      }
      else controllerChange(1, 0);
    }
  }
}

//----CALIBRATION----//
/* Likely will only have to calibrate once. This is done by activating calibration mode and
* "plucking" each note on each string starting with the upper bound (after the 21st fret) and descending down
* to just after the 1st fret. Starts with the low E string, then the A string and then the D string.
* fret definitions are stored in EEPROM. Once it is calibrated for the voltage put out by the MIDI --> USB board
* you can just have the calibration button do something else because you probably won't need it again.
*/

void calibrate() {
  
  for (int i=0; i<NUM_STRINGS; i++) {
    //Flash the LED too indicate calibration
    digitalWrite(PIN_LED, HIGH);
    delay(100);
    digitalWrite(PIN_LED, LOW);
    delay(100);
    digitalWrite(PIN_LED, HIGH);
    delay(100);
    digitalWrite(PIN_LED, LOW);
    delay(100);
    digitalWrite(PIN_LED, HIGH);
  
    int sensorMax = 0;
    int sensorMin = 1023;
    int val;
    
      //loop through the array of fret definitions
      for (int j=21; j>0; j--) {
      
        int response = false;
      
        //wait for response
        while (!response) {
        
          //read piezo val
          int piezoVal = analogRead(piezoPins[i]);
        
          //get the sensor min value (highest fret) on the first round
          if (j==21) {
            int fretVal = analogRead(softPotPins[i]);
            if (fretVal > sensorMax) (sensorMax = fretVal);
          
            //if the piezo is hit, register this as the definition for this fret
            if (piezoVal > PIEZO_THRESHOLD_ON) {
              int fretVal = analogRead(softPotPins[i]);
              sensorMin = fretVal;
              val = fretVal;
              response = true;
            }
          }
        
          else {
            //get the rest of the fret definitions
            //if the piezo is hit, register this as the definition for this fret
            if (piezoVal > PIEZO_THRESHOLD_ON) {
              int fretVal = analogRead(softPotPins[i]);
              fretVal = map(fretVal, sensorMin, sensorMax, 0, 255);
              fretVal = constrain(fretVal, 0, 255);
              val = fretVal;
              response = true;
            }
          }
        }
      
        //write to memory
        digitalWrite(PIN_LED, LOW);
        EEPROM.write(j + (21*i), val);
        
        delay(100);
        digitalWrite(PIN_LED, HIGH);
      }
    
    //update global definitions
    calibrationMin[i] = EEPROM.read(21 + (21*i));

    for (int j=1; j<21; j++) {
      fretDefs[i][j] = EEPROM.read(j + (i*21));
    }
  }
  
  buttonStates[RIGHT] = false;
  digitalWrite(PIN_LED, LOW);
}

//-------------MIDI functions-----------------

//note-on message
void noteOn(int cmd, int pitch, int velocity) {
  
  Serial1.write(byte(cmd));
  Serial1.write(byte(pitch));
  Serial1.write(byte(velocity));
  digitalWrite(PIN_LED, HIGH);
}
//note-off message
void noteOff(int cmd, int pitch, int velocity) {
  
  Serial1.write(byte(cmd));
  Serial1.write(byte(pitch));
  Serial1.write(byte(0));
  digitalWrite(PIN_LED, LOW);
}

//Sends controller change to the specified controller
void controllerChange(int controller, int value) {
  Serial1.write(byte(0xb0));
  Serial1.write(byte(controller));
  Serial1.write(byte(value));
  
  Serial1.write(byte(0xb1));
  Serial1.write(byte(controller));
  Serial1.write(byte(value));
  
  Serial1.write(byte(0xb2));
  Serial1.write(byte(controller));
  Serial1.write(byte(value));
}
