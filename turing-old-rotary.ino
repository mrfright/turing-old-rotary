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
  dialing,
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
const int dialPin = 7; //pin for dialing switch. close (HIGH) when  dialing, open (LOW) when not dialing
const int countPin = 8; //pin for counting switch.  closed (HIGH) for don't count, open (LOW) for each count
const int ledPin = 13; //LED pin

int dialCount = 0;
#define DIAL_ARRAY_LEN 10
int dialArray[DIAL_ARRAY_LEN];
int dialIndex = 0;

int p1[] = {1, 2, 3, 4};
const int p1_len = 4;
int p2[] = {0, 9, 8, 7};
const int p2_len = 4;

int ledState = HIGH;
int hangState; //current reciever reading
int dialState; //current dial reading
int countState;
int lastHangState = LOW; //previous reciever reading
int lastDialState = LOW; //previous dial reading
int lastCountState = LOW;

long lastHangDebounceTime = 0;  // the last time the reciever pin was toggled
long lastDialDebounceTime = 0; // last time the dial switch was toggled
long lastCountDebounceTime = 0;
const long debounceDelay = 50;    // the debounce time; increase if the output flickers
const long countDebounceDelay = 10;

State phoneState = nodial;

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
  pinMode(dialPin, INPUT);
  pinMode(countPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, ledState);
}

void loop() {
  int hangReading = digitalRead(hangPin);
  int dialReading = digitalRead(dialPin);
  int countReading = digitalRead(countPin);
  
  //debounce the reciever switch
  if (hangReading != lastHangState) {
    lastHangDebounceTime = millis();
  }

  if ((millis() - lastHangDebounceTime) > debounceDelay) { //then the bounce settled
    if (hangReading != hangState) {//then reciever state has changed
      hangState = hangReading;
      
      if(hangState == LOW) {//if first time off hook, start dial tone
        phoneState = nodial;
        putstring_nl("hang low");
      }   
      else {
        putstring_nl("hang high");
      }
    }
  }
  
  

  digitalWrite(ledPin, ledState);


  
  if(hangState == HIGH) {//then hung up
    //stop any playback
    wave.stop();
    //reset any state
    dialCount = 0;
    dialIndex = 0;
    
    int i = 0;
    for(i = 0; i < DIAL_ARRAY_LEN; i++) {
      dialArray[i] = -1;
    }
  }
  else {//off the hook
    //debounce the dial switch
    if (dialReading != lastDialState) {
      lastDialDebounceTime = millis();
    }
  
    if ((millis() - lastDialDebounceTime) > debounceDelay) { //then the bounce settled
      if (dialReading != dialState) {//then reciever state has changed
        dialState = dialReading; 
        ledState = dialState;
        
        if(dialState == LOW) {
          putstring("dial low");
          if(dialReading == HIGH) {
            putstring("\tdial reading high");
          }
          else {
            putstring("\tdial reading high\n");
          }
        }
        
        //if just started dialing, reset count
        if(dialState == HIGH) {
          dialCount = 0;
          putstring_nl("dial high");
          

        }
        //if just ended dialing, save count, set to dialing state if recied a count > 0 (10 is the real zero)
        //if dialindex bigger than dial array size, then act like wrong number
        //if dialed number matches anything then play
        else if(dialCount > 0) {
          int i;
          phoneState = dialing;
          dialArray[dialIndex] = dialCount;
          if(dialArray[dialIndex] >= 10) {
            dialArray[dialIndex] = 0;
          }
          dialIndex++;
          
          i = 0;
          int match = 1;
          for(i = 0; i < p1_len; i++) {
            if(p1[i] != dialArray[i]) {
              match = 0;
              break;
            }
          }
          
          if(match) {
            playfile("P1.WAV");
          }
          
          i = 0;
          match = 1;
          for(i = 0; i < p2_len; i++) {
            if(p2[i] != dialArray[i]) {
              match = 0;
              break;
            }
          }
          
          if(match) {
            playfile("P2.WAV");
          }
          
        }  
      }
    }
    
    if(dialState == HIGH) {
      //debounce the count switch
      if (countReading != lastCountState) {
        lastCountDebounceTime = millis();
      }
    
      if ((millis() - lastCountDebounceTime) > countDebounceDelay) { //then the bounce settled
        if (countReading != countState) {//then reciever state has changed
          countState = countReading; 
          
          
          //if just started dialing, reset count
          if(countState == LOW) {
            dialCount++;
            putstring_nl("count low");
  
          }
          else {
            putstring_nl("count high");
          }

        }
      }
    }
    
    switch(phoneState) {
      case nodial:
        if(!wave.isplaying) {//if stopped playing, start again
          playfile("DIALTONE.WAV");
        }
        break;
    }
  }
  
  lastHangState = hangReading;
  lastDialState = dialReading;
  lastCountState = countReading;
}

