/*
   Scintillation Counter Sketch.

   v1.0. 2020, OpenSC, phnx.
*/

#include <SAMD_AnalogCorrection.h>
#include <Adafruit_DotStar.h>

Adafruit_DotStar strip(1,8,6,DOTSTAR_RGB);

const uint8_t AIN_PIN = A5; //analog input pin
const uint8_t INT_PIN = 10; //signal interrupt pin
const uint8_t RST_PIN = 9; //mosfet peak detector reset pin
const uint32_t REFRESH_INT = 900; //seconds, 900 = 15 minuten

volatile uint32_t events = 0; //event counter

void set_lights(const uint8_t r=0, const uint8_t g=0, const uint8_t b=0){
  //strip.clear();
  strip.setPixelColor(0,r,g,b);
  strip.show();
}

void event_int(){
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println(3.3 / 4095 * analogRead(AIN_PIN), 3);
  events++;
  
  digitalWrite(RST_PIN, HIGH); //reset peak detector
  delayMicroseconds(1);
  digitalWrite(RST_PIN, LOW);

  digitalWrite(LED_BUILTIN, LOW);
}

void setup(){
  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(AIN_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // LED STARTUP
  strip.begin(); //Init DotStar LED
  strip.setBrightness(5); //turn down brightness
  strip.show(); //Initialize all pixels to 'off'

  analogReference(AR_DEFAULT);
  analogReadResolution(12); //12-bit adc readouts
  analogReadCorrection(11, 2054); //!!!CALIBRATE ADC

  attachInterrupt(digitalPinToInterrupt(INT_PIN), event_int, HIGH);

  set_lights(0,255,0);
  
  Serial.begin(2000000);
}

void loop(){
  /*
  detachInterrupt(digitalPinToInterrupt(INT_PIN));
  Serial.println(float(events) / float(REFRESH_INT)); //print average cps over REFRESH_INT time.
  events = 0;
  attachInterrupt(digitalPinToInterrupt(INT_PIN), event_int, HIGH);
  */
  delay(REFRESH_INT * 1000);
}
