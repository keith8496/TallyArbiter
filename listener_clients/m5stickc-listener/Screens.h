extern TFT_eSprite tftSprite;

extern String powerMode;
extern String batWarningLevel;
extern float batPercentage;
extern float batPercentage_M;
extern float batVoltage;
extern float batCurrent;
extern float batChargeCurrent;
extern float vbusVoltage;
extern float vinVoltage;
extern int chargeCurrent;

void showSettings();
void showTally();
void showPowerInfo();