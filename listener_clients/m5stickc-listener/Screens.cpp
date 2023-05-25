#include "Common.h"
#include "Screens.h"
#include "TallyArbiter.h"
#include "CustomWifi.h"

TFT_eSprite tftSprite = TFT_eSprite(&M5.Lcd);


// Settings Screen
void showSettings() {
  
  if (!portalRunning) {
    wm.startWebPortal();
    portalRunning = true;
  }

  //displays the current network connection and Tally Arbiter server data
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  //M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(WHITE, BLACK);

  M5.Lcd.println(listenerDeviceName);
  M5.Lcd.println(DeviceId + " " + DeviceName);
  M5.Lcd.println(String(tallyarbiter_host) + ":" + String(tallyarbiter_port));
  M5.Lcd.println();

  M5.Lcd.println(String(WiFi.SSID()));
  M5.Lcd.println(WiFi.localIP());

}


// Tally Screen
void showTally() {
  
  if(portalRunning) {
    wm.stopWebPortal();
    portalRunning = false;
  }
  
  M5.Lcd.setCursor(4, 82);
  M5.Lcd.setFreeFont(FSS24);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.println(DeviceName);

  //displays the currently assigned device and tally data
  evaluateMode();
}


void showPowerInfo() {

  if(portalRunning) {
    wm.stopWebPortal();
    portalRunning = false;
  }

  tftSprite.fillSprite(BLACK);
  tftSprite.setCursor(0, 0, 1.75);
  
  // M5.Axp.GetBatCurrent() == (M5.Axp.GetIdischargeData() / 2 * -1) when off charger
  // M5.Axp.GetBatCurrent() == (M5.Axp.GetIchargeData() / 2) when on charger
  tftSprite.printf("Bat: %s\r\n  V: %.3fv     %.3f%%\r\n", batWarningLevel, batVoltage, batPercentage);
  tftSprite.printf("  I: %.3fma  I: %.3fma\r\n", batCurrent, batChargeCurrent);
  tftSprite.printf("  Imax: %ima  Cmax: %.3f%%\r\n", chargeCurrent, batPercentage_M);
  // tftSprite.printf("  chrg:  %.3fma  cap: %dmw\r\n", M5.Axp.GetIchargeData() / 2, M5.Axp.GetVapsData());
  // tftSprite.printf("  dchrg: %.dma  Iin: %.3fma\r\n", M5.Axp.GetIdischargeData() / 2 * -1, M5.Axp.GetIinData() * 0.625);
  // tftSprite.printf("  currentOut %.3fma\r\n", M5.Axp.Read13Bit(0x7C)*0.5);
  //if (M5.Axp.GetBatPower() > 0) {
  //  tftSprite.printf("     %.3fmw\r\n", M5.Axp.GetBatPower() * -1);
  //}
  
  tftSprite.printf("USB:\r\n  V: %.3fv  I: %.3fma\r\n",
                    vbusVoltage,
                    M5.Axp.GetVBusCurrent());

  tftSprite.printf("5V-In:\r\n  V: %.3fv  I: %.3fma\r\n",
                    vinVoltage,
                    M5.Axp.GetVinCurrent());

  tftSprite.printf("APS:\r\n  V: %.3fv\r\n",
                    M5.Axp.GetAPSVoltage());

  tftSprite.printf("AXP:\r\n  Temp: %.1fc\r\n",
                   M5.Axp.GetTempInAXP192());
  
  tftSprite.print("\r\n"+powerMode);

  tftSprite.pushSprite(20, 10);

}