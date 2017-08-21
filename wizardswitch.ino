
/*
   By Steve Hoefer http://grathio.com
   Version 0.1.10.20.10
   Licensed under Creative Commons Attribution-Noncommercial-Share Alike 3.0
   http://creativecommons.org/licenses/by-nc-sa/3.0/us/
   (In short: Do what you want, just be sure to include this line and the four above it, and don't sell it or use it in anything you sell without contacting me.)
 */

#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define PIN 5

Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);

const byte eepromValid = 123;    // If the first byte in eeprom is this then the data is valid.
 
// Pin definitions
const int soundSensor = A0;        // Piezo buzzer on pin A0.
const int programSwitch = 6;       // Used to program in a new pattern
const int relay = 3;               // Relay pin
const int audioOut = 8;            // Audio out from the piezo buzzer
const int playbackButton = 9;      // Used to playback currently stored pattern
 
// Tuning constants.
const int threshold = 250;           // If sound exceeds this threshold, it is rejected.
const int maxThreshold = 600;         // Automatically reject any noise that is too loud to be the user.
const int rejectValue = 20;          // If an individual sound is off by this percentage it is rejected (Lower value = More strict)
const int averageRejectValue = 15;   // If the average timing of the sounds is off by this percent it is rejected. (Lower value = More strict)
const int fadeTime = 150;            // Time before we listen for another sound. (Debounce timer).
const int patternMax = 10;           // Pattern sound limit. Maximum number of sounds to listen for. (Default is 10)
const int timeOut = 1200;            // Longest time to wait for the next sound before we assume that it's finished.


// Variables
int waitTime = 2;                                                //Time to fade in and out of colors during startup sequence.
int patternArray[patternMax] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};   //Array where current pattern is stored
int incomingSound[patternMax];                                   //Array where incoming sounds are stored. To then be compared to the stored pattern.
int soundSensorValue = 0;                                        //Amplitude of the incoming sound from the sound detector board.
boolean programLEDFlag = false;                                  //Needed for the a blue colour to playback properly.
boolean programButtonPressed = false;                            //Flag so we remember the programming button setting at the end of the cycle.
int maxIntervalGlobal = 300;                                     // Needed for pattern playback function. A value needs to be passed in order for the playbackPattern() to work properly.
                                                                  //TODO: Rewrite the playbackPattern() so it does not require this workaround.
 
void setup() {
  strip.begin();
  strip.setBrightness(80);                // Sets brightness to a comfortable level.
  fade(255, 0, 0, waitTime);              // Red  
  fade(0, 255, 0, waitTime);              // Green
  fade(0, 0, 255, waitTime);              // Blue
  fade(255, 255, 0, waitTime);            // Yellow
  readPattern();                          // Load pattern from EEPROM, if any.
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);
  pinMode(programSwitch, INPUT);
  audioFeedback(500, 1000);              //Play a short tone so user knows device is running
  delay(100);
  audioFeedback(500, 1500);
}

void loop() {

  //Checks to see if playback button is pressed first
  if (digitalRead(playbackButton) == HIGH) {
    playbackPattern(maxIntervalGlobal);
    strip.setPixelColor(0, 0, 0, 0);          // Turn off the LED for normal operation
    strip.show();
    delay(100);
  }
      
  
  soundSensorValue = analogRead(soundSensor); // Listen for any sound at all.
  
  if (digitalRead(programSwitch)==HIGH){      // Is the program button pressed?
    programButtonPressed = true;              // Yes, so lets save that state then turn of the LED for normal operation.
    strip.setPixelColor(0, 0, 0, 0);
    strip.show();
  } else {
    programButtonPressed = false;
    strip.setPixelColor(0, 0, 0, 0);          //If not, just move on.
    strip.show();
  }
  
  if (soundSensorValue >= threshold && soundSensorValue <= maxThreshold){   //Sound must fall between the two threshold values to be passed on. Otherwise, it is rejected.
    listenToSecretSound();                      //Begin listening to sounds
  }
} 

// Records the timing of incoming sounds.
void listenToSecretSound() { 

  int i = 0;
  // First lets reset the listening array.
  for (i=0;i<patternMax;i++){
    incomingSound[i]=0;
  }
  
  int currentSoundNumber=0;         			// Incrementer for the array.
  int startTime=millis();           			// Reference for when this pattern started.
  int now=millis();
  

  if (programButtonPressed==true){
    strip.setPixelColor(0, 0, 0, 255);       // When programming, the LED will blink blue with every incoming sound
    strip.show();    
    programLEDFlag = true;                   // This flag is used so the LED will still be blue during playback to the user. 
  } else {
    strip.setPixelColor(0, 255, 0, 0);      // Otherwise, Green blinks for normal operation.
    strip.show(); 
  }

  delay(fadeTime);                       	        // Wait for this peak to fade before we listen to the next one.
  strip.setPixelColor(0, 0, 0, 0); 
  strip.show(); 

  do {
    //listen for the next sound or wait for it to timeout. 
    soundSensorValue = analogRead(soundSensor);
    if (soundSensorValue >= threshold && soundSensorValue <= maxThreshold){
      
      //record the delay time.
      now=millis();
      incomingSound[currentSoundNumber] = now - startTime;
      currentSoundNumber ++;                             //increment the counter.
      startTime=now;

      // and reset our timer for the next sound  
      if (programButtonPressed == true){
        strip.setPixelColor(0, 0, 0, 255);                     // Blue for programming.
        strip.show(); 
      } else {
        strip.setPixelColor(0, 255, 0, 0);                    // Green for normal use.
        strip.show();
      }
      delay(fadeTime);                              // Again, a little delay to let the sound decay.
      strip.setPixelColor(0, 0, 0, 0); 
      strip.show();
      }

    now=millis();
    
  } while ((now-startTime < timeOut) && (currentSoundNumber < patternMax));
  
  //We've got our pattern recorded, check to see if it's valid
  if (programButtonPressed == false) {             // Only if we're not in progamming mode.
    if (validatePattern() == true) {
      triggerRelay();
    } else {
  		// Pattern was wrong. Blink the red LED as visual feedback.
      for (i=0; i < 2; i++) { 
        strip.setPixelColor(0, 0, 255, 0);
        strip.show();
        delay(100);
        strip.setPixelColor(0, 0, 0, 0);
        strip.show();
        delay(100);
      }
    }
  } else {                            // If we're in programming mode we still validate but do nothing to the relay
    validatePattern();

    // Blink green and red alternately to show that the programming is complete.
    for (i=0; i<2; i++){
      strip.setPixelColor(0, 255, 0, 0); 
      strip.show();
      delay(200);
      strip.setPixelColor(0, 0, 255, 0); 
      strip.show();
      delay(200);    
    }
    strip.setPixelColor(0, 0, 0, 0);      // Turn the LED back off again to signal we are returing to normal operation.
    strip.show();
  }
}


//Actives the relay
void triggerRelay(){
  int i=0;
  digitalWrite(relay, LOW);
  delay(100);           
  digitalWrite(relay, HIGH); 

  //Green LED blinks for visual feedback
     for (i=0; i < 2; i++) { 
      strip.setPixelColor(0, 255, 255, 0);
      strip.show();
      delay(100);
      strip.setPixelColor(0, 0, 0, 0);
      strip.show();
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


// Compares incoming pattern to the stored pattern. This is where the fun happens, here we go...
// TODO: break it into smaller functions for readability.
boolean validatePattern(){
  
  int i=0;
  int currentSoundCount = 0;
  int soundCount = 0;
  int maxInterval = 0;          			// Used to normalize the times.
  
  for(i=0;i<patternMax;i++){
    if(incomingSound[i] > 0){
      currentSoundCount++;
    }
    if(patternArray[i] > 0){  
      soundCount++;
    }
    if(incomingSound[i] > maxInterval){ 	//Collect normalization data while we're looping.
      maxInterval = incomingSound[i];
    }
  }
  
  //If we're recording a new pattern, save the info and get out of here.
  if (programButtonPressed==true){
      for (i = 0; i < patternMax; i++){ // normalize the times
        patternArray[i]= map(incomingSound[i],0, maxInterval, 0, 100);  //Saves new patterns to patternArray array for temporary storage.
      }
      savePattern();
      playbackPattern(maxInterval);
	    return false;                                                   //Returns false so we dont trigger the relay while programming.
  }
  if (currentSoundCount != soundCount){                         //Quickest check first. Returns false if the counts don't match
    return false; 
  }

  
  /*  
      Compares the relative intervals of the sounds, not the absolute time between them. 
      Easier to input the pattern if the tempo is fast or slow but makes the whole system less precise.
  */

  
  int totaltimeDifferences=0;
  int timeDiff=0;
  for (i=0;i<patternMax;i++){ // Normalize the times
    incomingSound[i]= map(incomingSound[i],0, maxInterval, 0, 100);      
    timeDiff = abs(incomingSound[i]-patternArray[i]);
    if (timeDiff > rejectValue){ // If individual value too far off, the whole pattern string it rejected. (returns false) 
      return false;
    }
    totaltimeDifferences += timeDiff;
  }

  if (totaltimeDifferences/soundCount>averageRejectValue){  //If the whole pattern is off by too much, the pattern string is rejected. (returns false)
    return false; 
  }
  return true;
}

// Plays back the pattern via LED blinks and beeps on with the buzzer.
void playbackPattern(int maxInterval) {
  if (programLEDFlag == true) {
  strip.setPixelColor(0, 0, 0, 0);
  strip.show();
  delay(1000);
  strip.setPixelColor(0, 0, 0, 255);
  strip.show();
  audioFeedback(200, 1800);
  for (int i = 0; i < patternMax ; i++) {
    strip.setPixelColor(0, 0, 0, 0);
    strip.show();
    // only turn it on if there's a delay
    if (patternArray[i] > 0) {
      delay(map(patternArray[i], 0, 100, 0, maxInterval)); // Expand the time back out to what it was. Roughly.
        strip.setPixelColor(0, 0, 0, 255);
        strip.show();
      audioFeedback(200, 1800);
    }
  }
  strip.setPixelColor(0, 0, 0, 0);
  strip.show();
  programLEDFlag = false;  
  } else {
  strip.setPixelColor(0, 0, 0, 0);
  strip.show();
  delay(1000);
  strip.setPixelColor(0, 255, 0, 0);
  strip.show();
  audioFeedback(200, 1800);
  for (int i = 0; i < patternMax ; i++) {
    strip.setPixelColor(0, 0, 0, 0);
    strip.show();
    // only turn it on if there's a delay
    if (patternArray[i] > 0) {
      delay(map(patternArray[i], 0, 100, 0, maxInterval)); // Expand the time back out to what it was. Roughly.
        strip.setPixelColor(0, 255, 0, 0);
        strip.show();
      audioFeedback(200, 1800);
    }
  }
  strip.setPixelColor(0, 0, 0, 0);
  strip.show();
  }
}

// Loads pattern stored in EEPROM, if any.
void readPattern() {
 byte reading;
  int i;
  reading = EEPROM.read(0);
  if (reading == eepromValid) {                   // Only read EEPROM if the signature byte is correct.
    for (int i = 0; i < patternMax ; i++) {
      patternArray[i] =  EEPROM.read(i + 1);
    }
  }
}

// Saves new pattern to EEPROM
void savePattern() {
  EEPROM.write(0, 0);                            // Clear out the signature. That way we know if we didn't finish the write successfully.
  for (int i = 0; i < patternMax; i++) {
    EEPROM.write(i + 1, patternArray[i]);
  }
  EEPROM.write(0, eepromValid);                  // Write the signature so we'll know it's all good.
}

//Fades colour in and out. Used during startup sequence.
void fade(uint8_t red, uint8_t green, uint8_t blue, uint8_t wait) {
  
  for(uint8_t b=0; b <255; b++) {
     for(uint8_t i=0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, red*b/255, green*b/255, blue*b/255);
     }
     strip.show();
     delay(wait);
  }

  for(uint8_t b=255; b > 0; b--) {
     for(uint8_t i=0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, red*b/255, green*b/255, blue*b/255);
     }
     strip.show();
     delay(wait);
  }
  } 

