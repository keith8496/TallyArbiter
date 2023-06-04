#include <WebSocketsClient.h>
#include <SocketIOclient.h>

extern bool LAST_MSG;
extern char tallyarbiter_host[40];
extern char tallyarbiter_port[6];

extern String listenerDeviceName;
extern int currentScreen;

extern SocketIOclient socket;
extern String DeviceId;
extern String DeviceName;

void ws_emit(String event, const char *payload);
void connectToServer();
void evaluateMode();