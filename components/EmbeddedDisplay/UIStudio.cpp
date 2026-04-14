/* * ESP32 Point-to-Point 3-Wire SPI 
 * Target Display: NHD-2.4-240320AF-CSXP
 */

/************* Definitions *************/

#define SDA_PIN 23
#define SCL_PIN 18
#define CS_PIN  5
#define RES_PIN 4
#define TE_PIN 34

// Standard 16-bit (RGB565) Color Definitions
#define RED    0xF800
#define GREEN  0x07E0
#define BLUE   0x001F
#define BLACK  0x0000
#define WHITE  0xFFFF

// Font maps/tables
const uint8_t font_5x7[] {
  0x7E, 0x11, 0x11, 0x11, 0x7E, // A
  0x7F, 0x49, 0x49, 0x49, 0x36  // B
};

/******** Helper Functions ***********/

// The 9-bit protocol: Bit 0 is D/C, Bits 1-8 are Data
void send_9bit(uint8_t data, bool isData) {
  digitalWrite(CS_PIN, LOW);
  
  // 1. Data/Command Bit (1st bit)
  digitalWrite(SDA_PIN, isData ? HIGH : LOW);
  digitalWrite(SCL_PIN, LOW);
  digitalWrite(SCL_PIN, HIGH); // Sampled on rising edge (DC bit)

  
  // 2. 8-bit Payload
  for (int i = 0; i < 8; i++) {
    digitalWrite(SDA_PIN, (data & 0x80) ? HIGH : LOW);
    data <<= 1;
    digitalWrite(SCL_PIN, LOW);
    digitalWrite(SCL_PIN, HIGH); // Sampled on rising edge (data bit)
  }

  digitalWrite(CS_PIN, HIGH); // end of packet
}

// Defines Canvas area for painting display using GRAM
// pointer starts at (x1, y1) and can go until (x2, y2)
// 16-bit mode, so ">>" a high-byte and "&" a low-byte
void set_canvas (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  
  // Column Address Set (CASET)
  send_9bit(0x2A, false); // (CASET CMD)
  send_9bit(x1 >> 8, true);
  send_9bit(x1 & 0xFF, true);
  send_9bit(x2 >> 8, true);
  send_9bit(x2 & 0xFF, true);

  // Row Address Set (RASET)
  send_9bit(0x2B, false); // (RASET CMD)
  send_9bit(y1 >> 8, true);
  send_9bit(y1 & 0xFF, true);
  send_9bit(y2 >> 8, true);
  send_9bit(y2 & 0xFF, true);

  // Write to RAMWR command (start addr pointer at x1, y1)
  send_9bit(0x2C, false);
}

// Fills display with color
void fill_canvas (uint16_t color) {
  set_canvas(0,0,239,319);

  // 240 * 320 = 76,800 pixels
  for (uint32_t i = 0; i < 76800; i++) {
    // sending 2 bytes per pixel (since 16-bit mode)
    send_9bit(color >> 8, true); // high byte (R + upper G)
    send_9bit(color & 0xFF, true); // Low byte (lower G + B)
  }
}

// Puts display in low power mode
void sleep() {
  send_9bit(0x10, false); // sleep in
}

// Sets device orientation
// Modes: 
// v - vertical
// h - horizontal
void set_orientation(uint8_t mode) {
  send_9bit(0x36, false); // Start MADCTL CMD
  if (mode == 'h') { // Landscape
    send_9bit(0x70, true);
  } 
  else if (mode == 'v'){ // Portrait
    send_9bit(0x00, true);
  }
}

// Writes letter defined in font bitmap on canvs
void char_write(int16_t x, uint16_t y, char letter, uint16_t color, uint16_t bg) {
  uint8_t char_index = (letter - 'A') * 5; // offset from 'A'

  // Set a canvas exactly the size of a character
  set_canvas(x, y, x + 4, y + 6);

  for (int8_t column = 0; column < 5; column++) {
    uint8_t line = font_5x7[char_index + column];
    for (int8_t row = 0; row < 7; row++) {
      if (line & 0x01) {
        send_9bit(color >> 8, true);
        send_9bit(color & 0xFF, true);
      } else {
        send_9bit(bg >> 8, true);
        send_9bit(bg & 0xFF, true);
      }
      line >>= 1;
    }
  }
}

/********* Main Setup + Loop ************/

void setup() {
  // Set pins as output for display
  pinMode(SDA_PIN, OUTPUT);
  pinMode(SCL_PIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  pinMode(RES_PIN, OUTPUT);

/****** Start-up Sequence(s) ******/

  // Hard Reset: Essential to sample IM0/IM2 pins
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(SCL_PIN, HIGH);
  digitalWrite(RES_PIN, LOW);
  delay(100);
  digitalWrite(RES_PIN, HIGH);
  delay(150);

  // Initialization (Standard ST7789 IPS)
  send_9bit(0x11, false); // Sleep Out
  delay(120);
  
  // Memory Data Access Control (MADCTL)
  uint8_t h;
  uint8_t v;
  set_orientation(v);

  // Color Modification (COLMOD)
  send_9bit(0x3A, false); send_9bit(0x05, true); // 16-bit color/RGB565 (COLMOD)

  // NHD Display Optimization Registers
  send_9bit(0xB2, false); send_9bit(0x0C, true); send_9bit(0x0C, true);
  send_9bit(0x00, true); send_9bit(0x33, true); send_9bit(0x33, true);
  send_9bit(0xB7, false); send_9bit(0x35, true);
  send_9bit(0xBB, false); send_9bit(0x2B, true);
  send_9bit(0xC0, false); send_9bit(0x2C, true);
  send_9bit(0xC2, false); send_9bit(0x01, true); send_9bit(0xFF, true);
  send_9bit(0xC3, false); send_9bit(0x11, true);
  send_9bit(0xC4, false); send_9bit(0x20, true);
  send_9bit(0xC6, false); send_9bit(0x0F, true);
  send_9bit(0xD0, false); send_9bit(0xA4, true); send_9bit(0xA1, true);
  send_9bit();

  // Mandatory IPS Inversion
  send_9bit(0x21, false); // Inversion ON (IPS requirement)
  send_9bit(0x29, false); // Display ON
  // send_9bit(0x28, false); // Display OFF
  delay(20);
}

// loops indefinitely through code
void loop() {
// Cycle through colors to prove stability
  fill_canvas(RED);
  delay(1000);
  // while (digitalRead(TE_PIN) == LOW);
  fill_canvas(GREEN);
  delay(1000);
  // while (digitalRead(TE_PIN) == LOW);
  fill_canvas(BLUE);
  delay(1000);
  // while (digitalRead(TE_PIN) == LOW);
}