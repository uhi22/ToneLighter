/*
  ToneLigher Main

  Reads audio signal via a microphone, evaluates the volume and controls an LED stripe.

  The circuit:
  - A0 Analoges Mikrofonsignal (nach Verstärkung). Der Ruhepegel liegt irgendwo zwischen 2 und 4 Volt.
  - LED
    anode (long leg) attached to digital output 13
    cathode (short leg) attached to ground

  - Note: because most Arduinos have a built-in LED attached to pin 13 on the
    board, the LED is optional.

*/

#include <Adafruit_NeoPixel.h>

/* Rectifier and smooting in hardware. This means, on the ADC there is a DC voltage, which
 *  is higher with increasing volume.
 */
#define MIT_HARDWARE_GLEICHRICHTER

int microfonPin = A0;    // select the input pin for audio signal
int ledPin = 13;      // select the pin for the LED
int analogValue = 0;  // variable to store the value coming from the sensor
#define PIN_LEDSTRIPE_WS2812   6

#define PIN_GAIN_1 8 /* for switching the amplifier gain */
#define PIN_GAIN_2 9 /* for switching the amplifier gain */

// How many NeoPixels are attached to the Arduino?
#define LED_NEOPIXEL_COUNT 20 // 30 // 90

uint32_t arrTropfen[LED_NEOPIXEL_COUNT];

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_NEOPIXEL_COUNT, PIN_LEDSTRIPE_WS2812, NEO_RGB + NEO_KHZ800);


void setup() {
  // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);
  pinMode(PIN_GAIN_1, INPUT); /* switching the gain pin to input produces the highest amplifier gain */
  pinMode(PIN_GAIN_2, INPUT);
  Serial.begin(115200);
  Serial.println(F("Starting..."));
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(250); // Set BRIGHTNESS (max = 255)  
}

#define MODE_FIRE 0
#define MODE_TROPFEN 1
#define MODE_RAINBOW 2
#define MODE_FILTER_DEBUG 3
#define MODE_KLATSCH_DEBUG 4
#define MODE_SERIAL_DEBUG 5

#define MODE_MAX 5
#define MODE_AUTO_OFF 99
int mode=MODE_TROPFEN;
uint8_t modeAuto = MODE_AUTO_OFF;

int modedivider=0;

int filteredMax=0;
int nCycleDivider=0;
uint8_t nTropenSperre=0; /* Ein angefangener Tropen soll nicht durch eine andere Farbe gleich wieder überschrieben werden, sondern
                            sauber abklingen. Sonst würden insbesondere "laute" gleich wieder durch den leiseren Nachhall gleich
                            ihre laute Farbe verlieren. */

uint32_t mittelwert_u32=0;
uint16_t amplitude_1023, mittelwert_1023;
uint16_t amplitude_pixel;
uint16_t mittelwert_pixel;
uint16_t rel_amplitude_pixel; /* ist 0 wenn amplitude=mittelwert. Ist LED_NEOPIXEL_COUNT, wenn amplitude>=dreifachem Mittelwert. */
uint32_t rel_amplitude_1023; /* ist 0 wenn amplitude=mittelwert. Ist 1023, wenn amplitude>=dreifachem Mittelwert. */

#define KLD_CYCLE_MS 14
#define KLD_PULSE_INHIBIT_TIME (50/KLD_CYCLE_MS) /* Wenn ein Puls erkannt, dann ist alles innerhalb 50ms der selbe Pule. */
#define KLD_TIME_END_OF_NUMBER (250/KLD_CYCLE_MS)
#define KLD_TIME_END_OF_SEQUENCE (1000/KLD_CYCLE_MS)

#define KLD_TIME_DEBUG (10000/KLD_CYCLE_MS)
#define KLD_TRIGGERLIMIT 400
#define KLD_STATE_IDLE 0
#define KLD_STATE_INHIBIT 1
#define KLD_STATE_PULSE_SEEN 2
uint16_t kld_timerDebug=0;
uint8_t klatschDetektorStatus = KLD_STATE_IDLE;
uint16_t klatschDetektorTimer = 0;
uint8_t klatschDetektorAnzahl = 0;

/* KlatschProcessor */
#define KLPRO_FIFO_LEN 3
#define KLPRO_STATE_IDLE 0
uint8_t klproProcessorState = KLPRO_STATE_IDLE;
uint8_t klpro_FIFO[KLPRO_FIFO_LEN];
uint8_t klpro_output;

#define NUMBER_OF_KLATSCH_DEBUG LED_NEOPIXEL_COUNT
uint32_t aKlatschDebug[NUMBER_OF_KLATSCH_DEBUG];


void addToKlatschDebugFifo(uint32_t color) {
  int i;
  for (i=NUMBER_OF_KLATSCH_DEBUG-1; i>0; i--) {
    aKlatschDebug[i] = aKlatschDebug[i-1];
  }
  aKlatschDebug[0]=color;
}


void kldAddNumberToFifo(uint8_t nKlatschpulse) {
  klpro_FIFO[2] = klpro_FIFO[1];
  klpro_FIFO[1] = klpro_FIFO[0];
  klpro_FIFO[0] = nKlatschpulse; /* neuester Eintrag steht an Index 0 */
  Serial.print("kldAddNumberToFifo "); Serial.println(nKlatschpulse);
}

void kldFinish(void) {
  uint8_t i;
  if ((klpro_FIFO[1]==5) && (klpro_FIFO[0]>1)) {
    klpro_output = klpro_FIFO[0];
    Serial.print("KlatschProcessor");
    Serial.println(klpro_output);
    switch (klpro_output) {
      case 2: mode = MODE_FIRE; break;
      case 3: mode = MODE_TROPFEN; break;
      case 4: mode = MODE_FILTER_DEBUG; break;
      case 5: mode = MODE_KLATSCH_DEBUG; break;
    }
  }
  for (i=0; i<KLPRO_FIFO_LEN; i++) {
    klpro_FIFO[i]=0;
  }
}


uint16_t amplitude_alt_1023=0;

void calculateKlatschState(void) {
  uint8_t i;
  kld_timerDebug++;
  if (kld_timerDebug>= KLD_TIME_DEBUG) {
    kld_timerDebug=0;
    //Serial.println("KLD_DEBUG");
  }

  
  switch (klatschDetektorStatus) {
    case KLD_STATE_IDLE:
      if ((amplitude_1023>amplitude_alt_1023+400) && (mittelwert_1023<600)) {
        klatschDetektorTimer = 0;
        klatschDetektorStatus = KLD_STATE_INHIBIT;
        klatschDetektorAnzahl++;
        addToKlatschDebugFifo(50);
        //Serial.println("klatschen started");
      } else {
        if (klatschDetektorTimer<KLD_TIME_END_OF_SEQUENCE) {
          klatschDetektorTimer++;
        }
        if (klatschDetektorTimer==KLD_TIME_END_OF_NUMBER) {
          klatschDetektorTimer++;
          addToKlatschDebugFifo(0x000700);
          kldAddNumberToFifo(klatschDetektorAnzahl);
          klatschDetektorAnzahl=0;
        }
        if (klatschDetektorTimer==KLD_TIME_END_OF_SEQUENCE) {
          klatschDetektorTimer++;
          addToKlatschDebugFifo(0x200000);
          kldFinish();
        }
        
      }
      break;
    case KLD_STATE_INHIBIT:
      klatschDetektorTimer++;
      if (klatschDetektorTimer>KLD_PULSE_INHIBIT_TIME) {
        klatschDetektorTimer=0;
        klatschDetektorStatus = KLD_STATE_IDLE;
      }
      break;

  }
  amplitude_alt_1023 = amplitude_1023;
}

/*------------------------------------------------------------------------------*/
uint32_t arrRainbow[LED_NEOPIXEL_COUNT];
uint16_t rainbow_firstPixelHue=0;

void calcRainbow(void) {
  rainbow_firstPixelHue+=256;
  // Hue of first pixel runs loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count endless.
    for(int i=0; i<LED_NEOPIXEL_COUNT; i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = rainbow_firstPixelHue + (i * 65536L / LED_NEOPIXEL_COUNT);
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      arrRainbow[i] = strip.gamma32(strip.ColorHSV(pixelHue));
    }
  
}


/*------------------------------------------------------------------------------*/
char serialFifo[4];
uint8_t incomingByte;
uint8_t serial_debug_value;
uint16_t serial_debug_value16;

void handleRemoteRequests(void) {
  uint16_t x;
  if (Serial.available() > 0) {
      // read the incoming byte:
      incomingByte = Serial.read();
      /* fill it in a FIFO */
      serialFifo[0] = serialFifo[1];
      serialFifo[1] = serialFifo[2];
      serialFifo[2] = serialFifo[3];
      serialFifo[3] = incomingByte;
      if (serialFifo[3]==0x0D) {
        serial_debug_value++;
        /* Zeilenende --> wir interpretieren die letzten drei empfangenen Zeichen als dreistellige Dezimalzahl. */
        serialFifo[3]=0; /* String-Ende nach drei Stellen */
        x = atoi(serialFifo);
        serial_debug_value16 = x;
        if (x>=(64+4)) {
          x=x-(64+4);
          x/=4;
          mode=x;
        }
      }
      // say what you got:
      //Serial.print("I received: ");
      //Serial.println(incomingByte, DEC);
  }
}



#define FILTER_KOEFF 200 /* 100: wenige Sekunden zum Einschwingen; 1000: >20s */

void loop() {
  // read the value from the sensor:

  int i;
  int minValue, maxValue, diffValue;
  uint32_t color, onColor;

  #ifdef MIT_HARDWARE_GLEICHRICHTER
    /* wir messen die gleichgerichtete und geglättete Spannung. Ein Sample reicht also. */
    amplitude_1023 = analogRead(microfonPin);
    /* ca. 400 bei Klatschen in 1 Meter Entfernung,
       kaum über 500 bei maximaler Lautstärke. Wir normieren auf max = 1023, dazu verdoppeln: */
    amplitude_1023<<=2;
    if (amplitude_1023>1023) amplitude_1023=1023;
    /* Filtern: neu = 0.99 * alt       + 0.01 * input, oder
                neu = alt - 0.01 * alt +        input, dann hat mittelwert die 100fache Auflösung wie input */
    mittelwert_u32 = mittelwert_u32 - mittelwert_u32/FILTER_KOEFF + amplitude_1023;

    mittelwert_1023 = mittelwert_u32/FILTER_KOEFF;
    
    if (mittelwert_1023<1) mittelwert_1023=1; /* wir wollen später durch den Mittelwert teilen, um relative
                                                 Werte zu bekommen. Wir müssen Division durch Null vermeiden. */
    //Serial.print(amplitude_1023);
    //Serial.print(" ");
    //Serial.print(mittelwert_1023);

    amplitude_pixel=((uint32_t)amplitude_1023*LED_NEOPIXEL_COUNT)>>10;
    mittelwert_pixel=((uint32_t)mittelwert_1023*LED_NEOPIXEL_COUNT)>>10;
    
    if (amplitude_1023>mittelwert_1023) {
       rel_amplitude_pixel = (LED_NEOPIXEL_COUNT * (uint32_t)(amplitude_1023 - mittelwert_1023)) / mittelwert_1023;
       rel_amplitude_pixel/=2; /* 200% über Mittelwert sind Vollausschlag */
       if (rel_amplitude_pixel>LED_NEOPIXEL_COUNT) rel_amplitude_pixel=LED_NEOPIXEL_COUNT;
       rel_amplitude_1023 = ((uint32_t)1023 * (uint32_t)(amplitude_1023 - mittelwert_1023)) / mittelwert_1023;
       rel_amplitude_1023 /= 2; /* 200% über Mittelwert sind Vollausschlag */ 
       if (rel_amplitude_1023>1023) rel_amplitude_1023=1023;
       
    } else {
       rel_amplitude_pixel = 0;
       rel_amplitude_1023 = 0;
    }
    
    delay(10);
  #else  
    /* wir messen die Wechselspannung. Zehn Samples im Millisekundenabstand holen, und daraus die
     *  Amplitude berechnen: */
    minValue=1023;
    maxValue=0;
    for (i=0; i<10; i++) {
      analogValue = analogRead(microfonPin);
      if (analogValue>maxValue) maxValue = analogValue;
      if (analogValue<minValue) minValue = analogValue;
      delay(1);
    }
    diffValue=maxValue-minValue;
    if (diffValue>59) diffValue=59;
  #endif

  calculateKlatschState();
  calcRainbow();

  
  if (amplitude_pixel>filteredMax) filteredMax=amplitude_pixel;
  
  if (nTropenSperre==0) {
    if (rel_amplitude_1023>200) {
      if (rel_amplitude_1023>500) { nTropenSperre=2; arrTropfen[0]=strip.Color(0, 20,0); /* grün */ }
      if (rel_amplitude_1023>550) { nTropenSperre=2; arrTropfen[0]=strip.Color(0, 40,0); /* grün */ }
      if (rel_amplitude_1023>600) { nTropenSperre=3; arrTropfen[0]=strip.Color(0, 255,0); /* grün */ }
      if (rel_amplitude_1023>650) { nTropenSperre=5; arrTropfen[0]=strip.Color(255, 0, 0); /* rot */  }
      if (rel_amplitude_1023>700) { nTropenSperre=6; arrTropfen[0]=strip.Color(0, 0, 255); /* blau */ }
      if (rel_amplitude_1023>800) { nTropenSperre=8; arrTropfen[0]=strip.Color(255, 255, 255); /* weiss */ }
    }
  }

  onColor = getColorForLevel(filteredMax);


  for (i=0; i<LED_NEOPIXEL_COUNT; i++) {
    #if 0
    if (filteredMax>i) {
      /* hoher Pegel --> Pixel einschalten */
      color = onColor;
    } else {
      /* niedriger Pegel --> Pixel ausschalten */
      color = strip.getPixelColor(i);
      if ((color & 0xff)>=2) color=color-2;
      if ((color & 0xff00)>=0x200) color=color-0x200;
      if ((color & 0xff0000)>=0x20000) color=color-0x20000;
      //color=0;
    }
    #endif
    /* Spezialfall: Marker für höchsten Pegel */
    if (i==filteredMax) {
      //color = getColorForLevel(i);
    }
    switch (mode) {
      case MODE_KLATSCH_DEBUG:
          color = aKlatschDebug[i];
          break;
      case MODE_FILTER_DEBUG:
          color = strip.Color(0,0,0);
          if (i==amplitude_pixel) color = strip.Color(80,80,80);
          if (i==mittelwert_pixel) color = strip.Color(200,0,0);      
          break;
      case MODE_TROPFEN:
          color=arrTropfen[i];
          break;
      case MODE_RAINBOW:
          color=arrRainbow[i];
          break;
      case MODE_FIRE:
          if (i==rel_amplitude_pixel) {
            color = strip.Color(255, 80, 30); /* rot+gelb */
          } else if (abs(i-(int)rel_amplitude_pixel)<=2) {
            color = strip.Color(170, 50, 0); /* rot+gelb */
          } else if (abs(i-(int)rel_amplitude_pixel)<=4) {
            color = strip.Color(100, 30, 0); /* rot+gelb */
          } else {
            color =  macheDunkler(strip.getPixelColor(i), 1); 
          }
          break;
      case MODE_SERIAL_DEBUG:
          if (i==serial_debug_value) {
            color = strip.Color(255, 255, 255);
          } else {
            color = strip.Color(0, 0, 0);            
          }
          if (i<16) {
            if (serial_debug_value16 & ((uint16_t)1<<i)) {
              color = strip.Color(255, 0, 0);
            }
          }
          break;
    }
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
  }
  strip.show();                          //  Update strip to match

  nCycleDivider++;
  if (nCycleDivider>5) {
    nCycleDivider=0;
    /* Den Peak-Marker langsam runterlaufen lassen: */
    if (filteredMax>0) filteredMax--;
  }

  /* Tropen eine Position weiter schieben */
  for (i=LED_NEOPIXEL_COUNT-1; i>0; i--) {
    arrTropfen[i] = arrTropfen[i-1];
  }
  arrTropfen[0] = macheDunkler(arrTropfen[1], 20);
  if (nTropenSperre>0) nTropenSperre--;

  modedivider++;
  if (modedivider>1500) {
    modedivider=0;
    if (modeAuto!=MODE_AUTO_OFF) {
      modeAuto++;
      if (modeAuto>=MODE_KLATSCH_DEBUG) modeAuto=0;
      mode=modeAuto;
    }
  }
  
  
  //Serial.print(minValue);
  //Serial.print(" ");
  //Serial.print(maxValue);
  //Serial.print(" ");
  //Serial.println(diffValue);
  handleRemoteRequests();
   
}

uint32_t getColorForLevel(uint32_t level) {
  uint32_t c;
  c=strip.Color(0, 255,0); /* grün */
  if (level>20) c = strip.Color(190, 127,0); /* gelb */
  if (level>40) c = strip.Color(255, 0, 0); /* rot */
  if (level>55) c = strip.Color(255, 255, 255); /* weiss */  
  return c;
}

uint32_t map1023_to_color(uint16_t level) {
  uint32_t c;
  c=strip.Color(0,0,0); /* aus */
  if (level>400) c = strip.Color(0, 40,0); /* grün */
  if (level>600) c = strip.Color(100, 60,0); /* gelb */
  if (level>800) c = strip.Color(120, 0, 0); /* rot */
  if (level>900) c = strip.Color(40, 40, 90); /* blau */  
  return c;
  
}

uint8_t macheDunkler_Byte(uint8_t c, uint8_t delta) {
  if (c>=delta) c-=delta; else c=0;
  return c;
}

uint32_t macheDunkler(uint32_t c, uint8_t delta) {
  uint8_t bL, bM, bH;
  bL = macheDunkler_Byte(c, delta);
  bM = macheDunkler_Byte(c>>8, delta);
  bH = macheDunkler_Byte(c>>16, delta);
  return (((uint32_t)bH)<<16) | (((uint32_t)bM)<<8) | bL;
}

