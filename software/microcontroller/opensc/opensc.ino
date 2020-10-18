/*
   Scintillation Counter Sketch.

   v1.0. 2020, OpenSC, phnx.
*/

#include <SAMD_AnalogCorrection.h>

#include <Adafruit_SSD1306.h>
const uint8_t SCREEN_WIDTH = 128; // OLED display width, in pixels
const uint8_t SCREEN_HEIGHT = 32; // OLED display height, in pixels
const uint8_t OLED_RESET = 4; // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include <Adafruit_DotStar.h>
const uint8_t NUMPIXELS = 1;
const uint8_t DATAPIN = 8;
const uint8_t CLOCKPIN = 6;

Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

#include <Adafruit_SPIFlash.h>
Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);
FatFileSystem fatfs;
const String FILE_NAME = "data.csv";

const uint8_t AIN_PIN = A5; //analog input pin
const uint8_t INT_PIN = 10; //signal interrupt pin
const uint8_t RST_PIN = 9; //mosfet peak detector reset pin
const uint8_t BAT_PIN = A0; //battery voltage sense pin
const uint8_t PWR_PIN = 7; //spim power enable pin

const float BAT_FACT = 0.39394; //voltage divider factor for battery voltage
const uint16_t SERIAL_TIMEOUT = 5000; //timeout for serial wait, milliseconds
const uint16_t MAX_RAM_BUFFER = 32768; //maximum number of datapoints to buffer in RAM
const bool LOG_DATA = false; //true if all data also gets saved to the data file
const uint8_t REFRESH_INT = 2;

volatile uint32_t events = 0; //event counter
volatile uint16_t adc_data[MAX_RAM_BUFFER] = {};
volatile uint16_t data_size = 0;

void set_lights(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) {
  strip.clear();
  strip.setPixelColor(0, g, r, b); //g, r, b for whatever reason
  strip.show();
}

void init_display() {
  display.clearDisplay(); //show waiting for serial message
  display.setCursor(0, 0);
  display.print("Waiting for serial \nconnection...");
  display.display();

  while (!Serial && millis() < SERIAL_TIMEOUT) //wait for serial connection
  {
    delay(10);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("OpenSC");
  display.display();
}

void write_data() {
  unsigned long micro = micros();
  File dataFile = fatfs.open(FILE_NAME, FILE_WRITE);
  if (!dataFile) {
    Serial.println("Failed to open data file for writing!");
  }
  
  for(uint16_t i = 0; i < data_size; i++){
    dataFile.println(3.3 / 4095 * adc_data[i], 3);
  }
  dataFile.close();
  
  data_size = 0;
  Serial.print("File Write! Time needed (us): ");
  Serial.println(micros() - micro);
}

void peak_reset() {
  digitalWrite(RST_PIN, HIGH);
  delayMicroseconds(1);
  digitalWrite(RST_PIN, LOW);
}

void setup() {
  pinMode(INT_PIN, INPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(AIN_PIN, INPUT);
  pinMode(BAT_PIN, INPUT);
  pinMode(PWR_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(PWR_PIN, HIGH);

  // LED STARTUP
  strip.begin(); //Init DotStar LED
  strip.setBrightness(10); //turn down brightness
  strip.show(); // Initialize all pixels to 'off'

  analogReference(AR_DEFAULT);
  analogReadResolution(12); //12-bit adc readouts
  analogReadCorrection(12, 2055);//!!!CALIBRATE ADC

  attachInterrupt(digitalPinToInterrupt(INT_PIN), event_int, HIGH);

  // DISPLAY STARTUP
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32 + onboard 3.3 regulator
  display.setTextSize(1); //initialize standard text config
  display.setTextColor(WHITE);
  
  Serial.begin(2000000);

  set_lights(0, 255, 0);
  init_display();

  if (LOG_DATA){
    // Initialize flash library and check its chip ID.
    if (!flash.begin()) {
      Serial.println("Error, failed to initialize flash chip!");
      while (1);
    }
    Serial.print("Flash chip JEDEC ID: 0x"); Serial.println(flash.getJEDECID(), HEX);
  
    if (!fatfs.begin(&flash)) {
      Serial.println("Error, failed to mount newly formatted filesystem!");
      while (1);
    }
    Serial.println("Mounted filesystem!");
  
    File writeFile = fatfs.open(FILE_NAME, FILE_WRITE);
    if (!writeFile) {
      Serial.println("Failed to open data file for writing!");
    }
    writeFile.println("### BEGIN NEW MEASUREMENT ###");
    writeFile.close();
  }

  Serial.print("Free RAM (Bytes): ");
  Serial.println(FreeRam());
}

void loop() {
  //Serial.println(FreeRam());
  //Serial.println(3.3 / 4095 * analogRead(BAT_PIN) / BAT_FACT, 2);
  update_display();
  delay(REFRESH_INT * 1000);
}

void event_int() {
  digitalWrite(LED_BUILTIN, HIGH);
  //delayMicroseconds(1);
  volatile uint16_t readv = analogRead(AIN_PIN);

  Serial.println(3.3 / 4095 * readv, 3);
  if(LOG_DATA){
    if(data_size >= MAX_RAM_BUFFER){
      write_data();
    }
    adc_data[data_size] = readv;
    data_size++;
    Serial.println(data_size);
  }
  
  events++;
  digitalWrite(LED_BUILTIN, LOW);
  peak_reset();
}

void update_display() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);

  float cps = events / REFRESH_INT;
  events = 0;
  float cpm = cps * 60;

  display.println(cpm,2);
  display.display();
}

extern "C" char *sbrk(int i);

int FreeRam() {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}
