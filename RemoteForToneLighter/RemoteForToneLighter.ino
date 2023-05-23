/*
  RemoteFuerTonleuchter

  433-MHz-Fernbedienungsempfänger für Stimmungslicht.
  Features:
    - 433MHz: RadioHead-Bibliothek, ASK-Empfänger z.B. RXB-6.
    - Leitet empfangenen Code an serielle Schnittstelle weiter

 Hardware:
       - ASK-Empfänger z.B. RXB-6.
           Arduino.Pin4 == RXB6.VCC
           Arduino.Pin2 == RXB6.DATA
           Arduino.GND  == RXB6.GND
         Mit diesem Pinning passen Arduino und RXB6 als Sandwich direkt zusammen.  

       
 2018-06-14 Uwe:
    * Neu angelegt auf Basis RemoteFuerKamera
    * 
*/


  /* 433 MHz: RadioHead-Library: ****************************************************/
  #include <RH_ASK.h> /* RadioHead fuer Tx433 mit AmplitudeShiftKeying */
  #include <SPI.h> // Not actually used but needed to compile
  #define VW_MAX_MESSAGE_LEN 20

  #define TRX433_BAUD 2000
  #define TRX433_RXPIN 2 
  #define TRX433_TXPIN 6 /* nutzen wir hier vorerst nicht */
  #define TRX433_PTTPIN 0 /* brauchen wir nicht */

  RH_ASK driver433(TRX433_BAUD, TRX433_RXPIN, TRX433_TXPIN, TRX433_PTTPIN);

#define WKREASON_RESET 1
#define WKREASON_INT0 2
#define WKREASON_INT1 3
#define WKREASON_8S 4

char msg[80];
uint8_t sensorID;
uint8_t msgcounter;
uint16_t uBattSender_mV;
uint8_t senderWakeupReason;
uint16_t nReceived=0;
uint16_t nReceivedWithMovement=0;
uint16_t nSenderMoves=0;
uint16_t nLastMoves=0;
uint16_t nSenderTxCycles=0;
uint8_t  senderSensorId=0;
uint32_t nLastTime1s;

void blink(uint8_t n) {
  uint8_t i;
  for (i=1; i<=n; i++) {
     digitalWrite(LED_BUILTIN, 1);
     delay(100);
     digitalWrite(LED_BUILTIN, 0);
     delay(200);
  }
}




void task1s(void) {
    
}


void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(4, OUTPUT); // VCC für den 433MHz-Empfänger  
  digitalWrite(4, HIGH); // VCC ein
  
  Serial.begin(115200);
  Serial.println(F("Starte Empfaenger..."));
    
    if (!driver433.init()) {
         Serial.println("init of RadioHead for 433MHz failed");
    } else {
         Serial.println("init of RadioHead OK");      
    }   
}


// the loop function runs over and over again forever
void loop() {
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  byte i;
  char s[200];
  char str_temp[8];
  uint8_t tastencode;


  buflen = VW_MAX_MESSAGE_LEN;
  if (driver433.recv(buf, &buflen)) { // Non-blocking
    if (buflen==2) {
        if (buf[0]==0x41) { /* Funkfernbedienung ID 0x41 */
          /* Funkfernbedienung */
          digitalWrite(LED_BUILTIN, 1);
          tastencode = buf[1];
          sprintf(s, "%03d",tastencode);
          s[3]=0x0d; /* Zeilenumbruch */
          s[4]=0x0a; /* Zeilenumbruch */
          s[5]=0;
          
          //Serial.println(s);
          /* Der TonLeuchter mit WS2812-LEDs muss ein striktes Timing für die LED-Ansteuerung einhalten,
           *  und muss daher die Interrupts sperren. Um ihn seriell zu füttern, schicken wir Byte für Byte
           *  mit langer Pause, so dass er auch bei sporadischem Polling alles mitbekommt.
           */
          for (i=0; i<5; i++) { 
            Serial.print(s[i]);
            delay(10); /* 10ms warten */ 
          }
          digitalWrite(LED_BUILTIN, 0);
        }    
    }
  }
  
  if (millis()-nLastTime1s >= 1000uL) {
        nLastTime1s+=1000;
        task1s();
  }   
 }
