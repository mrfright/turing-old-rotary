/*
Turing Old Rotary

This is the Arduino code for the old black rotary phone prop
used in the Theater Ninjas play Turing Machine.

Coded by Eric Faehnrich
*/

#include <WaveHC.h>
#include <WaveUtil.h>

enum State {
  nodial,
  dialed,
  done
};



SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the volumes root directory
FatReader f;      // This holds the information for the file we're play
WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time

uint8_t dirLevel; // indent level for file/dir names    (for prettyprinting)
dir_t dirBuf;     // buffer for directory reads

const int hangPin = 6; //pin for reciever switch. closed (HIGH) when hung up, open (LOW) when off the hook

const int ledPin = 13; //LED pin

int ledState = HIGH;
int hangState; //current reciever reading
int lastHangState = LOW; //previous reciever reading

long lastHangDebounceTime = 0;  // the last time the reciever pin was toggled
const long debounceDelay = 50;    // the debounce time; increase if the output flickers

State dialState = nodial;

void playfile(char *name) {
  if (wave.isplaying) {
    wave.stop(); 
  }

  if (!f.open(root, name)) {
    putstring_nl("Couldn't open file "); 
    Serial.print(name); 
    return;
  }

  if (!wave.create(f)) {
    putstring_nl("Not a valid WAV"); return;
  }
  
  wave.play();
}


void setup() {
  
  Serial.begin(9600);           // set up Serial library at 9600 bps for debugging
  
  putstring_nl("\nTest");  // say we woke up!
  
  putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
  Serial.println(FreeRam());

  //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)  
    putstring_nl("Card init. failed!");  // Something went wrong, lets print out why
  }
  
  // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);
  
  // Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {   // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;                           // we found one, lets bail
  }
  if (part == 5) {                     // if we ended up not finding one  :(
    putstring_nl("No valid FAT partition!");  // Something went wrong, lets print out why
  }
  
  // Lets tell the user about what we found
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(), DEC);     // FAT16 or FAT32?
  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
    putstring_nl("Can't open root dir!");      // Something went wrong,
  }
  
  // Whew! We got past the tough parts.
  putstring_nl("Files found (* = fragmented):");

  // Print out all of the files in all the directories.
  root.ls(LS_R | LS_FLAG_FRAGMENTED);
  
  
  pinMode(hangPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, ledState);
}

void loop() {
  int hangReading = digitalRead(hangPin);
  
  //debounce the reciever switch
  if (hangReading != lastHangState) {
    lastHangDebounceTime = millis();
  }

  if ((millis() - lastHangDebounceTime) > debounceDelay) { //then the bounce settled
    if (hangReading != hangState) {//then reciever state has changed
      hangState = hangReading;
      ledState = hangState;   
      
      if(hangState == LOW) {//if first time off hook, start dial tone
        dialState = nodial;
      }   
    }
  }
  
  //debounce the reciever switch
  if (hangReading != lastHangState) {
    lastHangDebounceTime = millis();
  }

  if ((millis() - lastHangDebounceTime) > debounceDelay) { //then the bounce settled
    if (hangReading != hangState) {//then reciever state has changed
      hangState = hangReading;
      ledState = hangState;   
      
      if(hangState == LOW) {//if first time off hook, start dial tone
        dialState = nodial;
      }   
    }
  }

  digitalWrite(ledPin, ledState);


  
  if(hangState == HIGH) {//then hung up
    //stop any playback
    wave.stop();
    //reset any state
  }
  else {//off the hook
    switch(dialState) {
      case nodial:
        if(!wave.isplaying) {//if stopped playing, start again
          playfile("DIALTONE.WAV");
        }
        break;
    }
  }
  
  lastHangState = hangReading;
}

