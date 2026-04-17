int backpin= D0; // GPIO pin for the button
int homepin= D1; // GPIO pin for the button
int modepin= D2; // GPIO pin for the button

volatile bool backpressed = false;
volatile bool homepressed = false;
volatile bool modepressed = false;

void setup() {
  pinMode(backpin, INPUT_PULLUP); // Set the button pin as input with pull-up resistor
  pinMode(homepin, INPUT_PULLUP); // Set the button pin as input with pull-up resistor
  pinMode(modepin, INPUT_PULLUP); // Set the button pin as input
  // Attach interrupts to the button pins
  attachInterrupt(digitalPinToInterrupt(backpin),ISR, FALLING); // Trigger on button press (active LOW)
    attachInterrupt(digitalPinToInterrupt(homepin), ISR, FALLING); // Trigger on button press (active LOW)
      attachInterrupt(digitalPinToInterrupt(modepin), ISR, FALLING); // Trigger on button press (active LOW)
}
void loop() { 
    if (backpressed) {
    backpressed = false; // back action
}
if (homepressed) {
    homepressed = false; // home action
}
if (modepressed) {
    modepressed = false; // mode action
}
}
// Interrupt service routines for the buttons
void backISR() {
  backpressed = true; // Set the flag for back button press
}
void homeISR() {
  homepressed = true; // Set the flag for home button press
}
void modeISR() {
  modepressed = true; // Set the flag for mode button press
}
