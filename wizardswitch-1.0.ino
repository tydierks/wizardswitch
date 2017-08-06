/* Detects patterns of knocks and triggers a motor to unlock
   it if the pattern is correct.
   
   By Steve Hoefer http://grathio.com
   Version 0.1.10.20.10
   Licensed under Creative Commons Attribution-Noncommercial-Share Alike 3.0
   http://creativecommons.org/licenses/by-nc-sa/3.0/us/
   (In short: Do what you want, just be sure to include this line and the four above it, and don't sell it or use it in anything you sell without contacting me.)
 */

#include <EEPROM.h>

const byte eepromValid = 123;    // If the first byte in eeprom is this then the data is valid.
 
// Pin definitions
const int soundSensor = A0;        // Piezo sensor on pin A0.
const int programSwitch = 7;       // Used to program in a new pattern
const int relay = 3;               // Relay pin
const int redLED = 4;              // Status LED
const int greenLED = 5;            // Status LED
const int audioOut = 8;            //Audio out from the piezo buzzer
const int playbackButton = 9;      //Used to playback currently stored pattern
 
// Tuning constants.  Could be made vars and hoooked to potentiometers for soft configuration, etc.
const int threshold = 50;           // Minimum signal from the piezo to register as a knock
const int rejectValue = 20;        // If an individual knock is off by this percentage of a knock we don't unlock..
const int averageRejectValue = 15; // If the average timing of the knocks is off by this percent we don't unlock.
const int knockFadeTime = 150;     // milliseconds we allow a knock to fade before we listen for another one. (Debounce timer.)
const int maximumKnocks = 20;       // Maximum number of knocks to listen for.
const int knockComplete = 1200;     // Longest time to wait for a knock before we assume that it's finished.


// Variables.
int secretCode[maximumKnocks] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // Initial setup: "Shave and a Hair Cut, two bits."
int knockReadings[maximumKnocks];       // When someone knocks this array fills with delays between knocks.
int soundSensorValue = 0;               // Last reading of the knock sensor.
boolean programButtonPressed = false;   // Flag so we remember the programming button setting at the end of the cycle.
int maxIntervalGlobal = 100;            // Needed for pattern playback function. Only a temporary solution, needs proper implementation.

void setup() {
  readPattern();
  pinMode(relay, OUTPUT);           // Load pattern from EEPROM, if any.
  digitalWrite(relay, HIGH);
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(programSwitch, INPUT);
  digitalWrite(programSwitch, LOW);
  digitalWrite(greenLED, HIGH);      // Green LED turns on, good to go.
}

void loop() {
  
  // Listen for any knock at all.
  soundSensorValue = analogRead(soundSensor);

  if (digitalRead(programSwitch)==HIGH){  // is the program button pressed?
    programButtonPressed = true;          // Yes, so lets save that state
    digitalWrite(redLED, HIGH);           // and turn on the red light too so we know we're programming.
  } else {
    programButtonPressed = false;
    digitalWrite(redLED, LOW);
  }
  
  if (soundSensorValue >=threshold){
    listenToSecretSound();
  }
} 

// Records the timing of knocks.
void listenToSecretSound(){ 

  int i = 0;
  // First lets reset the listening array.
  for (i=0;i<maximumKnocks;i++){
    knockReadings[i]=0;
  }
  
  int currentKnockNumber=0;         			// Incrementer for the array.
  int startTime=millis();           			// Reference for when this knock started.
  int now=millis();
  
  digitalWrite(greenLED, LOW);      			// we blink the LED for a bit as a visual indicator of the knock.
  if (programButtonPressed==true){
     digitalWrite(redLED, LOW);                         // and the red one too if we're programming a new knock.
  }
  delay(knockFadeTime);                       	        // wait for this peak to fade before we listen to the next one.
  digitalWrite(greenLED, HIGH);  
  if (programButtonPressed==true){
     digitalWrite(redLED, HIGH);                        
  }

  do {
    //listen for the next knock or wait for it to timeout. 
    soundSensorValue = analogRead(soundSensor);
    if (soundSensorValue >=threshold){                   //got another knock...
      //record the delay time.

      now=millis();
      knockReadings[currentKnockNumber] = now-startTime;
      currentKnockNumber ++;                             //increment the counter
      startTime=now;          
      // and reset our timer for the next knock
      digitalWrite(greenLED, LOW);  
      if (programButtonPressed==true){
        digitalWrite(redLED, LOW);                       // and the red one too if we're programming a new knock.
      }
      delay(knockFadeTime);                              // again, a little delay to let the knock decay.
      digitalWrite(greenLED, HIGH);
      if (programButtonPressed==true){
        digitalWrite(redLED, HIGH);                         
      }
    }

    now=millis();
    
    //did we timeout or run out of knocks?
  } while ((now-startTime < knockComplete) && (currentKnockNumber < maximumKnocks));
  
  //we've got our knock recorded, lets see if it's valid
  if (programButtonPressed==false){             // only if we're not in progrmaing mode.
    if (validateKnock() == true){
      triggerRelay();
    } else {
      digitalWrite(greenLED, LOW);  		// We didn't unlock, so blink the red LED as visual feedback.
      for (i=0; i<4; i++){					
        digitalWrite(redLED, HIGH);
        delay(100);
        digitalWrite(redLED, LOW);
        delay(100);
      }
      digitalWrite(greenLED, HIGH);
    }
  } else { // if we're in programming mode we still validate the lock, we just don't do anything with the lock
    validateKnock();
    // and we blink the green and red alternately to show that program is complete.
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH);
    for (i=0; i<3; i++){
      delay(100);
      digitalWrite(redLED, HIGH);
      digitalWrite(greenLED, LOW);
      delay(100);
      digitalWrite(redLED, LOW);
      digitalWrite(greenLED, HIGH);      
    }
  }
}


//Actives the relay
void triggerRelay(){
  int i=0;
  audioFeedback(500, 1500);
  audioFeedback(500, 1000); 
  digitalWrite(relay, LOW);
  delay(100);
  digitalWrite(greenLED, HIGH);           
  digitalWrite(relay, HIGH); 

  //Green LED blinks for visual feedback
     for (i=0; i < 5; i++) { 
      digitalWrite(greenLED, LOW);
      delay(100);
      digitalWrite(greenLED, HIGH);
      delay(100);
  }   
}

// Plays a tone on the piezo.
// playTime = milliseconds to play the tone
// delayTime = time in microseconds between ticks. (smaller = higher pitch tone.)
void audioFeedback(int playTime, int delayTime) {
  long loopTime = (playTime * 1000L) / delayTime;
  pinMode(audioOut, OUTPUT);
  for (int i = 0; i < loopTime; i++) {
    digitalWrite(audioOut, HIGH);
    delayMicroseconds(delayTime);
    digitalWrite(audioOut, LOW);
  }
  pinMode(audioOut, INPUT);
}


// Sees if our knock matches the secret.
// returns true if it's a good knock, false if it's not.
// todo: break it into smaller functions for readability.
boolean validateKnock(){
  
  int i=0;
  int currentKnockCount = 0;
  int secretKnockCount = 0;
  int maxInterval = 0;          			// We use this later to normalize the times.
  
  for(i=0;i<maximumKnocks;i++){
    if(knockReadings[i] > 0){
      currentKnockCount++;
    }
    if(secretCode[i] > 0){  
      secretKnockCount++;
    }
    if(knockReadings[i] > maxInterval){ 	//Collect normalization data while we're looping.
      maxInterval = knockReadings[i];
    }
  }
  maxIntervalGlobal = maxInterval;
  
  //If we're recording a new knock, save the info and get out of here.
  if (programButtonPressed==true){
      for (i = 0; i < maximumKnocks; i++){ // normalize the times
        secretCode[i]= map(knockReadings[i],0, maxInterval, 0, 100);  //Saves new patterns to secretCode array
      }
      savePattern();
      playbackPattern(maxInterval);
	    return false;                                                   //Returns false so we dont open the door while programming
  }
  if (currentKnockCount != secretKnockCount){                         //Quickest check first. Returns false if the counts don't match
    return false; 
  }
  
  /*  Now we compare the relative intervals of our knocks, not the absolute time between them.
      (ie: if you do the same pattern slow or fast it should still open the door.)
      This makes it less picky, which while making it less secure can also make it
      less of a pain to use if you're tempo is a little slow or fast. 
  */
  int totaltimeDifferences=0;
  int timeDiff=0;
  for (i=0;i<maximumKnocks;i++){ // Normalize the times
    knockReadings[i]= map(knockReadings[i],0, maxInterval, 0, 100);      
    timeDiff = abs(knockReadings[i]-secretCode[i]);
    if (timeDiff > rejectValue){ // Individual value too far out of whack
      return false;
    }
    totaltimeDifferences += timeDiff;
  }
  // It can also fail if the whole thing is too inaccurate.
  if (totaltimeDifferences/secretKnockCount>averageRejectValue){
    return false; 
  }
  return true;
}

// Plays back the pattern of the knock in blinks and beeps
void playbackPattern(int maxInterval) {
  digitalWrite(redLED, LOW);
  digitalWrite(greenLED, LOW);
  delay(1000);
  digitalWrite(redLED, HIGH);
  digitalWrite(greenLED, HIGH);
  audioFeedback(200, 1800);
  for (int i = 0; i < maximumKnocks ; i++) {
      digitalWrite(redLED, LOW);
      digitalWrite(greenLED, LOW);
    // only turn it on if there's a delay
    if (secretCode[i] > 0) {
      delay(map(secretCode[i], 0, 100, 0, maxInterval)); // Expand the time back out to what it was. Roughly.
        digitalWrite(redLED, HIGH);
        digitalWrite(greenLED, HIGH);
      audioFeedback(200, 1800);
    }
  }
  digitalWrite(redLED, LOW);
  digitalWrite(greenLED, LOW);
}

// Loads pattern stored in EEPROM (if any)
void readPattern() {
 byte reading;
  int i;
  reading = EEPROM.read(0);
  if (reading == eepromValid) {   // only read EEPROM if the signature byte is correct.
    for (int i = 0; i < maximumKnocks ; i++) {
      secretCode[i] =  EEPROM.read(i + 1);
    }
  }
}

// Saves new pattern to EEPROM
void savePattern() {
  EEPROM.write(0, 0);  // clear out the signature. That way we know if we didn't finish the write successfully.
  for (int i = 0; i < maximumKnocks; i++) {
    EEPROM.write(i + 1, secretCode[i]);
  }
  EEPROM.write(0, eepromValid);  // all good. Write the signature so we'll know it's all good.
}
