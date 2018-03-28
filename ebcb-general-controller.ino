// ebcb-general-controller.ino      Andrew Ward  6/23/2017
//
// A General purpose controller for stuff @ EBCB... Starting with Stained Glass Windows
//
// Two active LED channels are controlled.... Interfaced to Blynk apps, and IFTTT control.
// updates:
//   2018-03-26   Added 3rd LED channel (for side light control)

#include <blynk.h> // automatically added by the Particle IDE
#include <MDNS.h>  // automatically added by the Particle IDE.
#include <Debounce.h>  // automatically added by the Particle IDE.
#include <Adafruit_PCA9685.h> // automatically added by the Particle IDE.

SYSTEM_THREAD(ENABLED); // new beta feature as of 4.0.7 firmware: allows the code to run while the firmware is trying to connect to wifi

char auth[] = "c4f66cc955a141cf951ca9bb8225ddce";
char my_device_name[] = "EBCBcontrol";

//Adafruit_PCA9685 pwm = Adafruit_PCA9685(0x40, true);  // Use the default address, but also turn on debugging
Adafruit_PCA9685 pwm = Adafruit_PCA9685(0x40);
Debounce debouncer = Debounce(); 
MDNS mdns;

unsigned long millis_next_off   = 0l;
unsigned long next_status_print = 0l;
bool current_ps_state;

// -- Photon Hardware Assigmments 
int boardLed = D7; // This is the LED that is already on your device.
int PIR0 = A0; 

// channel assignemnts in 9685 controller 
#define CH_8_PS_CTRL            8
#define CH_0_SG_BKGND           0
#define CH_1_SG_BIBLE           1
#define CH_2_SG_SIDES           2

#define NUM_CHANNELS            16
#define PWM_FREQ_DEFAULT        400
#define PWM_VALUE_DEFAULT       128

#define SG_BKGND_CHANNEL        0 
#define SG_BKGND_PWM_DEFAULT    4095
 
#define SG_BIBLE_CHANNEL        1 
#define SG_BIBLE_PWM_DEFAULT    1000

#define SG_SIDES_CHANNEL        2 
#define SG_SIDES_PWM_DEFAULT    4095

#define SG_POWER_SUPPLY_CHANNEL 7 

#define OFF_AFTER_MINS          10 
#define STATUS_INTERVAL_MINS    2   

void setup() {
    unsigned long end_of_ser_conn_wait = millis() + 6000; // 3 seconds
    int chan_ctr=0, pwm_value=PWM_VALUE_DEFAULT;
    IPAddress ip;

    pinMode(boardLed,OUTPUT); // Our on-board LED is output as well

    debouncer.attach(PIR0, INPUT_PULLUP);
    debouncer.interval(3000); // interval in ms
    
    // Serial.begin();
    USBSerial1.begin();
    setupCloudFunctions();
    
    
    digitalWrite(boardLed,HIGH);
    while(millis() < end_of_ser_conn_wait ){
        if (!(millis() % 500)) digitalWrite(boardLed,!digitalRead(boardLed));
        // let it run out:  if (USBSerial1.isConnected()) break;
        Particle.process();
        if (System.buttonPushed()) {

        }
    }
    digitalWrite(boardLed, LOW);

    USBSerial1.println("HELLO USB Andrew!");
    
    Blynk.begin(auth);

    pwm.begin();
    pwm.setPWMFreq(PWM_FREQ_DEFAULT);  // This is the maximum PWM frequency
  
    for(;chan_ctr < NUM_CHANNELS; chan_ctr++) {
        pwm.setVal(chan_ctr, pwm_value); 
    }

    // Set to On value by default.  TBD - use nvram to remember last state.
    Ctrl_PS_On();

    USBSerial1.println("All DONE with Setup");
    
    delay(2000);
    ip = WiFi.resolve("blynk-cloud.com");
    Particle.publish("BlynkCloud ", String(ip), PRIVATE);
    
    if (mdns.setHostname(my_device_name)) {
        mdns.begin();
    }
}


void loop() {
  int val;
  
  debouncer.update();
  Blynk.run();
  mdns.processQueries();
  
  
  if (millis_next_off) {  // only do this if the timer is being used (non-zero)
    if (millis() > millis_next_off) {
      millis_next_off = 0;
      Ctrl_PS_Off();
      debug("Auto OFF\n");
    }
  }

  //debug("Blot");
  if (millis() > next_status_print) {
    // mdnsAdvertiser(1, my_device_name, strlen(my_device_name));
    if (next_status_print == 0) {
        debug("INITIAL STARTUP: Status: PS is %d\n", current_ps_state);
    } else {
        // debug("Periodic Status: PS is %d, debouncer rd %d\n", 
        //     current_ps_state, debouncer.read());
        //debug("Periodic Status: PS is %d\n", 
        //    current_ps_state);
    }
    next_status_print = millis_mins_from_now(STATUS_INTERVAL_MINS);
    // delay(2);
    // debug("Status: automatic_timer value is %ld\n", millis_next_off);
    // delay(2);
    // debug("and next_status_print = %ld", next_status_print);
    // delay(2);
    // debug("and millis now %ld", millis());
    // delay(1);
  }
  if (debouncer.fell()) {
    //debug("Blot B");
      // read the input pin
    if ( millis_next_off) {   // non-zero indicates timed-mode is active
        millis_next_off = millis_mins_from_now(OFF_AFTER_MINS);
        debug("MOTION! (ACTIVELY used!)");
    } else {
        debug("MOTION! (but inactive/disabled)");
    }
  }
  // debug("end Loop");
}

// --------------------  Particle Cloud Functions (supports IFTTT) -----------------------

void setupCloudFunctions()
{
    Particle.function("DevControl", DevControl);
}

// this function will be published to the cloud, allowing the user to control the fan remotely
int DevControl(String cmd)
{
    if (cmd == "on") { 
        debug("cmd 'on': Set Power Supply 'ON'\n");
        millis_next_off = 0;
        if (!current_ps_state){
          return(Ctrl_PS_On());
        }
        return 0; 
    } else if (cmd == "on_timed") { 
        debug("cmd 'on_timed': Set Power Supply On for %d mins\n", OFF_AFTER_MINS);
        millis_next_off = millis_mins_from_now(OFF_AFTER_MINS);
        if (!current_ps_state){
          return(Ctrl_PS_On());
        }
    } else if (cmd == "off") { 
        debug("cmd 'off': Set Power Supply 'OFF'\n");
        millis_next_off = 0;
        return(Ctrl_PS_Off());
    } else if (cmd == "set_default") { 
        debug("cmd 'set_default'\n");
        return(Ctrl_Set_Defaults());
    } else if (cmd == "1") { 
        debug("cmd '1': TBD\n");
    }
    return 1;
}

// --------------------  BLYNK Functions -----------------------

BLYNK_WRITE(V0) {
    // int chan_ctr=0;
    USBSerial1.print("(V0) Set PWM ");
    USBSerial1.println(param.asInt());
    debug("Blynk (V0) Set SG Bkgnd PWM to %d\n", param.asInt());
    Ctrl_PWM_Bkgnd(param.asInt());
}

BLYNK_WRITE(V1) {
    // int chan_ctr=0;
    USBSerial1.print("(V1) Set PWM ");
    USBSerial1.println(param.asInt());
    debug("Blynk (V1) Set SG Bible PWM to %d\n", param.asInt());
    Ctrl_PWM_Bible(param.asInt());
}

BLYNK_WRITE(V2) {
    // int chan_ctr=0;
    USBSerial1.print("(V2) Set PWM ");
    USBSerial1.println(param.asInt());
    debug("Blynk (V1) Set SG Sides PWM to %d\n", param.asInt());
    Ctrl_PWM_Sides(param.asInt());
}

BLYNK_WRITE(V17) {
    USBSerial1.print("(V17) Set PWM Frequency to ");
    USBSerial1.println(param.asInt());
    debug("Blynk (V17) Set FREQUENCY to %d\n", param.asInt());
    Ctrl_Set_Freq(param.asInt()); 
}

BLYNK_WRITE(V18) {
    if (param.asInt() == 0) {   // only do on button RELEASE
        debug("Blynk (V18) Restore Defaults\n");
        Ctrl_Set_Defaults(); 
    } 
}

BLYNK_WRITE(V19) {
    if (param.asInt() == 0) {   // only do on button RELEASE
      debug("Blynk (V18) Timed On:  Set Power Supply On for %d mins\n", OFF_AFTER_MINS);
      millis_next_off = millis_mins_from_now(OFF_AFTER_MINS);
      if (!current_ps_state){
          Ctrl_PS_On();
      }
    }
}

BLYNK_WRITE(V8) {
    if (param.asInt() > 0) {
        USBSerial1.print("(V8) Set Chan 8 (Relay) ON ");
        debug("Blynk (V8) Set PS ON\n");
        millis_next_off = 0;
        Ctrl_PS_On();
    } else {
        USBSerial1.print("(V8) Set Chan 8 (Relay) OFF ");
        debug("Blynk (V8) Set PS OFF\n");
        Ctrl_PS_Off();
    }
}

BLYNK_READ(V20) // Get and store push button status 
{
    Blynk.virtualWrite(V20, millis_next_off);
}

// This is called for all virtual pins, that don't have BLYNK_WRITE handler
BLYNK_WRITE_DEFAULT() {
  USBSerial1.print("input V");
  USBSerial1.print(request.pin);
  USBSerial1.println(":");
  // Print all parameter values
  for (auto i = param.begin(); i < param.end(); ++i) {
    USBSerial1.print("* ");
    USBSerial1.println(i.asString());
  }
}

// This is called for all virtual pins, that don't have BLYNK_READ handler
BLYNK_READ_DEFAULT() {
  // Generate random response
  int val = random(0, 100);
  USBSerial1.print("output V");
  USBSerial1.print(request.pin);
  USBSerial1.print(": ");
  USBSerial1.println(val);
  Blynk.virtualWrite(request.pin, val);
}

// --------------------  HW Action Functions -----------------------

int Ctrl_Set_Freq(uint16_t val) {
  pwm.setPWMFreq(val);  
  return 0;
}

int Ctrl_PS_Off(void) {
  pwm.setVal(CH_8_PS_CTRL, 0);    
  current_ps_state = 0;
  return 0;
}

int Ctrl_PS_On(void) {
  pwm.setVal(CH_8_PS_CTRL, 4095);    
  current_ps_state = 1;
  return 0;
}

int Ctrl_PWM_Bkgnd(uint16_t val) {
  pwm.setVal(CH_0_SG_BKGND, val);    
  return 0;
}

int Ctrl_PWM_Bible(uint16_t val) {
  pwm.setVal(CH_1_SG_BIBLE, val);    
  return 0;
}

int Ctrl_PWM_Sides(uint16_t val) {
  pwm.setVal(CH_2_SG_SIDES, val);    
  return 0;
}

int Ctrl_Set_Defaults(void) {
  pwm.setPWMFreq(PWM_FREQ_DEFAULT);  
  pwm.setVal(CH_0_SG_BKGND, SG_BKGND_PWM_DEFAULT); 
  pwm.setVal(CH_1_SG_BIBLE, SG_BIBLE_PWM_DEFAULT); 
  pwm.setVal(CH_2_SG_SIDES, SG_SIDES_PWM_DEFAULT); 
  millis_next_off = millis_mins_from_now(OFF_AFTER_MINS);
  return 0;
}

uint16_t Ctrl_get_PWM_Bkgnd(void) {
  return pwm.readPWMOn(CH_0_SG_BKGND);
}

uint16_t Ctrl_get_PWM_Bible(void) {
  return pwm.readPWMOn(CH_1_SG_BIBLE);
}

uint16_t Ctrl_get_PWM_Sides(void) {
  return pwm.readPWMOn(CH_2_SG_SIDES);
}

// --------------------  Utility Functions -----------------------

// Log message to cloud, message is a printf-formatted string
void debug(String message, int value ) {
    char msg [50];
    sprintf(msg, message.c_str(), value);
    Particle.publish("STAT", msg);
}
// Log message to cloud, message is a printf-formatted string
void debug(String message) {
    char msg [50];
    sprintf(msg, message.c_str());
    Particle.publish("STAT", msg);
}

unsigned long millis_mins_from_now(uint32_t mins) {
    return (millis() + (mins * 60 * 100));
}

