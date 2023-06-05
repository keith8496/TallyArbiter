#include "Common.h"
#include "TallyArbiter.h"
#include "CustomWifi.h"
#include "Screens.h"
#include <PinButton.h>
#include <millisDelay.h>

#define TRIGGER_PIN 0   //reset pin 

//M5StickC variables
PinButton btnM5(37);      //the "M5" button on the device
PinButton btnAction(39);  //the "Action" button on the device
uint8_t wasPressed();

millisDelay oneSecDelay;

int currentBrightness = 9;
int maxBrightness = 12;

String powerMode = "";
String batWarningLevel = "";
float batVoltage = 0;
float batPercentage = 0;
float batCurrent = 0;
float batChargeCurrent = 0;
float vbusVoltage = 0;
float vinVoltage = 0;
int chargeCurrent = 100;

const int batPercentage_C = 60;
int batPercentage_I = 0;
float batPercentage_M = 0;
float batPercentage_A [batPercentage_C];

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

  currentBrightness = 9;
  M5.Axp.ScreenBreath(currentBrightness);
  M5.Lcd.setRotation(3);
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.println("booting...");
  logger("Tally Arbiter M5StickC+ Listener Client booting.", "info");
  logger("Listener device name: " + listenerDeviceName, "info");

  // For power screen
  tftSprite.createSprite(240, 135); // Create a 240x135 canvas.
  tftSprite.setRotation(3);
  M5.Axp.EnableCoulombcounter();    // Enable Coulomb counter.

  // Load preferences
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

  oneSecDelay.start(1000);  // start delay
  connectToServer();

  delay(3000);
  M5.Lcd.fillScreen(TFT_BLACK);
  showTally();

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
    
    M5.Lcd.fillScreen(TFT_BLACK);
    currentScreen = currentScreen + 1;
    if (currentScreen > 2) {
      currentScreen = 0;
    }
    
    switch (currentScreen) {
      case 0:
        showTally();
        break;
      case 1:
        showSettings();
        break;
      case 2:
        showPowerInfo();
        break;
    }

  }

  if (oneSecDelay.justFinished()) {
    oneSecDelay.repeat();
    doPowerManagement();
    if (currentScreen == 2) {
      showPowerInfo();
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


void updateBrightness() {
  if(currentBrightness >= maxBrightness) {
    currentBrightness = 7;
  } else {
    currentBrightness++;
  }
  M5.Axp.ScreenBreath(currentBrightness);
}


void doPowerManagement() {
  
  batWarningLevel = (M5.Axp.GetWarningLevel()) ? "LOW BATTERY" : "";
  batVoltage = M5.Axp.GetBatVoltage();

  float batPercentage1 = (batVoltage - 3.0) / (4.2-3.0) * 100; // min = 3.0 max = 4.2
  float batPercentage2 = (batPercentage1 <= 100) ? batPercentage1 : 100;
  batPercentage = batPercentage2;

  if (batPercentage_A[batPercentage_I] == batPercentage_M) {
    batPercentage_M = 0;
  }

  batPercentage_A[batPercentage_I] = batPercentage2;
  batPercentage_I = batPercentage_I + 1;
  if (batPercentage_I > batPercentage_C) {
    batPercentage_I = 0;
  }

  if (batPercentage > batPercentage_M) {
    for (int i = 0; i <= batPercentage_C; ++i) {
      if (batPercentage_A[i] > batPercentage_M) {
        batPercentage_M = batPercentage_A[i];
      }
    }
  }

  // Send batPercentage to TA
  //char messageOut[32];
  //String(batPercentage,1).toCharArray(messageOut, 32);
  //String("Hello World").toCharArray(messageOut,32);
  //ws_emit("messaging", messageOut);

  batCurrent = M5.Axp.GetBatCurrent();
  batChargeCurrent = M5.Axp.GetBatChargeCurrent();
  vbusVoltage = M5.Axp.GetVBusVoltage();
  vinVoltage = M5.Axp.GetVinVoltage();


  // Power Mode
  if (vinVoltage > 3.8) {         // 5v IN Charge
    
    maxBrightness = 12;
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  //WIFI_PS_NONE

    if ((batPercentage_M < 80) && (chargeCurrent != 780)) {
      powerMode = "Fast Charge";
      chargeCurrent = 780;
      M5.Axp.Write1Byte(0x33, 0xc8);
    } else if (batPercentage_M >= 80 && batPercentage_M < 90 && chargeCurrent != 280) {
      powerMode = "Performance";
      chargeCurrent = 280;
      M5.Axp.Write1Byte(0x33, 0xc2);
    } else if (batPercentage_M >= 90 && chargeCurrent != 190) {
      powerMode = "Performance";
      chargeCurrent = 190;
      M5.Axp.Write1Byte(0x33, 0xc1);
    }

  } else if (vbusVoltage > 3.8) {   // 5v USB Charge

    powerMode = "Performance";
    maxBrightness = 12;
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    
    if (chargeCurrent != 100) {
      chargeCurrent = 100;
      M5.Axp.Write1Byte(0x33, 0xc0);
    }

  } else {
    
    // 3v Battery

    if (chargeCurrent != 100) {
      chargeCurrent = 100;
      M5.Axp.Write1Byte(0x33, 0xc0);
    }

    if (batPercentage_M >= 80) {
      powerMode = "Balanced";
      maxBrightness = 12;
      esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    } else {
      powerMode = "Power Saver";
      maxBrightness = 9;
      if (currentBrightness > maxBrightness) {
        currentBrightness = maxBrightness;
        M5.Axp.ScreenBreath(currentBrightness);
      }
      esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    }

  }

}