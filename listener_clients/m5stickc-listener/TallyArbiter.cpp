#include "Common.h"
#include "TallyArbiter.h"
#include "Screens.h"
#include <Arduino_JSON.h>

#define maxTextSize 5   //larger sourceName text

#define TALLY_EXTRA_OUTPUT false
#if TALLY_EXTRA_OUTPUT
const int led_program = 10;
const int led_preview = 26; //OPTIONAL Led for preview on pin G26
const int led_aux = 36;     //OPTIONAL Led for aux on pin G36
#endif

bool LAST_MSG = false; // true = show log on tally screen<
char tallyarbiter_host[40] = "192.168.13.52"; //IP address of the Tally Arbiter Server
char tallyarbiter_port[6] = "4455";

// General Variables
String listenerDeviceName = "m5StickC-1";
int currentScreen = 0;

String prevType = "";                     // reduce display flicker by storing previous state
String actualType = "";
String actualColor = "";
int actualPriority = 0;

//Tally Arbiter variables
SocketIOclient socket;
JSONVar BusOptions;
JSONVar Devices;
JSONVar DeviceStates;
String DeviceId = "unassigned";
String DeviceName = "Unassigned";
String LastMessage = "";


void ws_emit(String event, const char *payload = NULL) {
  if (payload) {
    String msg = "[\"" + event + "\"," + payload + "]";
    //Serial.println(msg);
    socket.sendEVENT(msg);
  } else {
    String msg = "[\"" + event + "\"]";
    //Serial.println(msg);
    socket.sendEVENT(msg);
  }
}


void socket_Connected(const char * payload, size_t length) {
  logger("Connected to Tally Arbiter server.", "info");
  logger("DeviceId: " + DeviceId, "info-quiet");
  String deviceObj = "{\"deviceId\": \"" + DeviceId + "\", \"listenerType\": \"" + listenerDeviceName.c_str() + "\", \"canBeReassigned\": true, \"canBeFlashed\": true, \"supportsChat\": true }";
  char charDeviceObj[1024];
  strcpy(charDeviceObj, deviceObj.c_str());
  ws_emit("listenerclient_connect", charDeviceObj);
}


String strip_quot(String str) {
  if (str[0] == '"') {
    str.remove(0, 1);
  }
  if (str.endsWith("\"")) {
    str.remove(str.length()-1, 1);
  }
  return str;
}


void evaluateMode() {
  if(actualType != prevType) {
    M5.Lcd.setCursor(4, 82);
    M5.Lcd.setFreeFont(FSS24);
    //M5.Lcd.setTextSize(maxTextSize);
    actualColor.replace("#", "");
    String hexstring = actualColor;
    long number = (long) strtol( &hexstring[1], NULL, 16);
    int r = number >> 16;
    int g = number >> 8 & 0xFF;
    int b = number & 0xFF;
    if (actualType != "") {
      M5.Lcd.setTextColor(BLACK);
      M5.Lcd.fillScreen(M5.Lcd.color565(r, g, b));
      M5.Lcd.println(DeviceName);
    } else {
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.fillScreen(TFT_BLACK);
      M5.Lcd.println(DeviceName);
    }
    
    #if TALLY_EXTRA_OUTPUT
    if (actualType == "\"program\"") {
      digitalWrite (led_program, LOW);
      digitalWrite (led_preview, HIGH);
      digitalWrite (led_aux, HIGH);
    } else if (actualType == "\"preview\"") {
      digitalWrite (led_program, HIGH);
      digitalWrite (led_preview, LOW);
      digitalWrite (led_aux, HIGH);
    } else if (actualType == "\"aux\"") {
      digitalWrite (led_program, HIGH);
      digitalWrite (led_preview, HIGH);
      digitalWrite (led_aux, LOW);
    } else {
      digitalWrite (led_program, HIGH);
      digitalWrite (led_preview, HIGH);
      digitalWrite (led_aux, HIGH);
    }
    #endif
    
    logger("Device is in " + actualType + " (color " + actualColor + " priority " + String(actualPriority) + ")", "info");
    Serial.print(" r: " + String(r) + " g: " + String(g) + " b: " + String(b));

    prevType = actualType;
  }

  M5.Lcd.printf("%.3f%% bat", batPercentage);
  
  if (LAST_MSG == true){
    M5.Lcd.println(LastMessage);
  }
}


void SetDeviceName() {
  for (int i = 0; i < Devices.length(); i++) {
    if (JSON.stringify(Devices[i]["id"]) == "\"" + DeviceId + "\"") {
      String strDevice = JSON.stringify(Devices[i]["name"]);
      DeviceName = strDevice.substring(1, strDevice.length() - 1);
      break;
    }
  }
  preferences.begin("tally-arbiter", false);
  preferences.putString("devicename", DeviceName);
  preferences.end();
  evaluateMode();
}


void socket_Reassign(String payload) {
  String oldDeviceId = payload.substring(0, payload.indexOf(','));
  String newDeviceId = payload.substring(oldDeviceId.length()+1);
  newDeviceId = newDeviceId.substring(0, newDeviceId.indexOf(','));
  oldDeviceId = strip_quot(oldDeviceId);
  newDeviceId = strip_quot(newDeviceId);

  String reassignObj = "{\"oldDeviceId\": \"" + oldDeviceId + "\", \"newDeviceId\": \"" + newDeviceId + "\"}";
  char charReassignObj[1024];
  strcpy(charReassignObj, reassignObj.c_str());
  ws_emit("listener_reassign_object", charReassignObj);
  ws_emit("devices");
  
  M5.Lcd.fillScreen(RED);
  delay(200);
  M5.Lcd.fillScreen(TFT_BLACK);
  delay(200);
  M5.Lcd.fillScreen(RED);
  delay(200);
  M5.Lcd.fillScreen(TFT_BLACK);
  
  logger("newDeviceId: " + newDeviceId, "info-quiet");
  DeviceId = newDeviceId;
  preferences.begin("tally-arbiter", false);
  preferences.putString("deviceid", newDeviceId);
  preferences.end();
  SetDeviceName();
}


void socket_Flash() {
  //flash the screen white 3 times
  M5.Lcd.fillScreen(WHITE);
  delay(500);
  M5.Lcd.fillScreen(TFT_BLACK);
  delay(500);
  M5.Lcd.fillScreen(WHITE);
  delay(500);
  M5.Lcd.fillScreen(TFT_BLACK);
  delay(500);
  M5.Lcd.fillScreen(WHITE);
  delay(500);
  M5.Lcd.fillScreen(TFT_BLACK);

  //then resume normal operation
  switch (currentScreen) {
    case 0:
      showTally();
      break;
    case 1:
      showSettings();
      break;
  }
}


void socket_Messaging(String payload) {
  String strPayload = String(payload);
  int typeQuoteIndex = strPayload.indexOf(',');
  String messageType = strPayload.substring(0, typeQuoteIndex);
  messageType.replace("\"", "");
  int messageQuoteIndex = strPayload.lastIndexOf(',');
  String message = strPayload.substring(messageQuoteIndex + 1);
  message.replace("\"", "");
  LastMessage = messageType + ": " + message;
  evaluateMode();
}


String getBusTypeById(String busId) {
  for (int i = 0; i < BusOptions.length(); i++) {
    if (JSON.stringify(BusOptions[i]["id"]) == busId) {
      return JSON.stringify(BusOptions[i]["type"]);
    }
  }

  return "invalid";
}


String getBusColorById(String busId) {
  for (int i = 0; i < BusOptions.length(); i++) {
    if (JSON.stringify(BusOptions[i]["id"]) == busId) {
      return JSON.stringify(BusOptions[i]["color"]);
    }
  }

  return "invalid";
}

int getBusPriorityById(String busId) {
  for (int i = 0; i < BusOptions.length(); i++) {
    if (JSON.stringify(BusOptions[i]["id"]) == busId) {
      return (int) JSON.stringify(BusOptions[i]["priority"]).toInt();
    }
  }

  return 0;
}


void processTallyData() {
  bool typeChanged = false;
  for (int i = 0; i < DeviceStates.length(); i++) {
    if (DeviceStates[i]["sources"].length() > 0) {
      typeChanged = true;
      actualType = getBusTypeById(JSON.stringify(DeviceStates[i]["busId"]));
      actualColor = getBusColorById(JSON.stringify(DeviceStates[i]["busId"]));
      actualPriority = getBusPriorityById(JSON.stringify(DeviceStates[i]["busId"]));
    }
  }
  if(!typeChanged) {
    actualType = "";
    actualColor = "";
    actualPriority = 0;
  }
  evaluateMode();
}


void socket_event(socketIOmessageType_t type, uint8_t * payload, size_t length) {
  String eventMsg = "";
  String eventType = "";
  String eventContent = "";

  switch (type) {
    case sIOtype_CONNECT:
      socket_Connected((char*)payload, length);
      break;

    case sIOtype_DISCONNECT:
    case sIOtype_ACK:
    case sIOtype_ERROR:
    case sIOtype_BINARY_EVENT:
    case sIOtype_BINARY_ACK:
      // Not handled
      break;

    case sIOtype_EVENT:
      eventMsg = (char*)payload;
      eventType = eventMsg.substring(2, eventMsg.indexOf("\"",2));
      eventContent = eventMsg.substring(eventType.length() + 4);
      eventContent.remove(eventContent.length() - 1);

      logger("Got event '" + eventType + "', data: " + eventContent, "info-quiet");

      if (eventType == "bus_options") BusOptions = JSON.parse(eventContent);
      if (eventType == "reassign") socket_Reassign(eventContent);
      if (eventType == "flash") socket_Flash();
      if (eventType == "messaging") socket_Messaging(eventContent);

      if (eventType == "deviceId") {
        DeviceId = eventContent.substring(1, eventContent.length()-1);
        SetDeviceName();
        showTally();
        currentScreen = 0;
      }

      if (eventType == "devices") {
        Devices = JSON.parse(eventContent);
        SetDeviceName();
      }

      if (eventType == "device_states") {
        DeviceStates = JSON.parse(eventContent);
        processTallyData();
      }

      break;

    default:
      break;
  }
}


void connectToServer() {
  logger("Connecting to Tally Arbiter host: " + String(tallyarbiter_host), "info");
  socket.onEvent(socket_event);
  socket.begin(tallyarbiter_host, atol(tallyarbiter_port));
}
