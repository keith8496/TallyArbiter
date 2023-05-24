#include "Common.h"
#include "TallyArbiter.h"
#include "CustomWifi.h"
#include "Screens.h"
#include <PinButton.h>

#define TRIGGER_PIN 0   //reset pin 

//M5StickC variables
PinButton btnM5(37);      //the "M5" button on the device
PinButton btnAction(39);  //the "Action" button on the device
uint8_t wasPressed();


// Start Setup
void setup() {
  
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  while (!Serial);

  // Initialize the M5StickC object
  logger("Initializing M5StickC+.", "info-quiet");

  setCpuFrequencyMhz(80); //Save battery by turning down the CPU clock
  btStop();               //Save battery by turning off BlueTooth

  uint64_t chipid = ESP.getEfuseMac();
  listenerDeviceName = "m5StickC-" + String((uint16_t)(chipid>>32)) + String((uint32_t)chipid);

  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  //M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.println("booting...");
  logger("Tally Arbiter M5StickC+ Listener Client booting.", "info");
  logger("Listener device name: " + listenerDeviceName, "info");

  preferences.begin("tally-arbiter", false);

  // added to clear out corrupt prefs
  //preferences.clear();
  logger("Reading preferences", "info-quiet");
  if(preferences.getString("deviceid").length() > 0){
    DeviceId = preferences.getString("deviceid");
  }
  if(preferences.getString("devicename").length() > 0){
    DeviceName = preferences.getString("devicename");
  }
  if(preferences.getString("taHost").length() > 0){
    String newHost = preferences.getString("taHost");
    logger("Setting TallyArbiter host as" + newHost, "info-quiet");
    newHost.toCharArray(tallyarbiter_host, 40);
  }
  if(preferences.getString("taPort").length() > 0){
    String newPort = preferences.getString("taPort");
    logger("Setting TallyArbiter port as" + newPort, "info-quiet");
    newPort.toCharArray(tallyarbiter_port, 6);
  }
 
  preferences.end();

  delay(100); //wait 100ms before moving on
  connectToNetwork(); //starts Wifi connection
  M5.Lcd.println("SSID: " + String(WiFi.SSID()));
  while (!networkConnected) {
    delay(200);
  }

  #if TALLY_EXTRA_OUTPUT
    // Enable interal led for program trigger
    pinMode(led_program, OUTPUT);
    digitalWrite(led_program, LOW);
    pinMode(led_preview, OUTPUT);
    digitalWrite(led_preview, LOW);
    pinMode(led_aux, OUTPUT);
    digitalWrite(led_aux, LOW);
  #endif
  
  connectToServer();

}


// Main Loop
void loop() {
  
  if(portalRunning){
    wm.process();
  }
  
  checkReset(); //check for reset pin
  socket.loop();
  btnM5.update();
  btnAction.update();

  if (btnM5.isClick()) {
    switch (currentScreen) {
      case 0:
        showSettings();
        currentScreen = 1;
        break;
      case 1:
        showTally();
        currentScreen = 0;
        break;
    }
  }

  if (btnAction.isClick()) {
    updateBrightness();
  }
}


void checkReset() {
  // check for button press
  if ( digitalRead(TRIGGER_PIN) == LOW ) {

    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      M5.Lcd.setCursor(0, 40);
      M5.Lcd.fillScreen(TFT_BLACK);
      M5.Lcd.setFreeFont(FSS9);
      //M5.Lcd.setTextSize(1);
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.println("Reset button pushed....");
      logger("Button Pressed", "info");
      // still holding button for 3000 ms, reset settings, code not ideal for production
      delay(3000); // reset delay hold
      if ( digitalRead(TRIGGER_PIN) == LOW ) {
        M5.Lcd.println("Erasing....");
        logger("Button Held", "info");
        logger("Erasing Config, restarting", "info");
        wm.resetSettings();
        ESP.restart();
      }

      M5.Lcd.println("Starting Portal...");
      // start portal w delay
      logger("Starting config portal", "info");
      wm.setConfigPortalTimeout(120);

      if (!wm.startConfigPortal(listenerDeviceName.c_str())) {
        logger("failed to connect or hit timeout", "error");
        delay(3000);
        // ESP.restart();
      } else {
        //if you get here you have connected to the WiFi
        logger("connected...yeey :)", "info");
      }
    }
  }
}


