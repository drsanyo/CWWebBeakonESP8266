#include <EEPROM.h>
#include <Arduino.h>

#include "config.h"

#define HTTP_LOGIN_DEFAULT "admin"
#define HTTP_PASSWD_DEFAULT "admin"
#define WIFI_CLIENT_DEFAULT_SSID ""
#define WIFI_CLIENT_DEFAULT_PSK ""
#define WIFI_AP_DEFAULT "testtest"

#define EESIZE  (sizeof(bcnConfigItem_t) * BEACON_COUNT) + sizeof(bcnConfigCom_t)

void bcnConfig::readEE()
{
  uint8_t   i;
  uint16_t  eeAdr = 0;
  
  EEPROM.begin(EESIZE);

  EEPROM.get(eeAdr, common);
  eeAdr += sizeof(bcnConfigCom_t);

  if (common.start != BEACON_START)
  { // default values if normal values have not yet been stored in the EEPROM.
    common.start = BEACON_START;
    common.freqCorr = 80000;
    common.enable = false;
    common.pause = 5; // 60;
    strcpy(common.http_login, HTTP_LOGIN_DEFAULT);
    strcpy(common.http_passwd, HTTP_PASSWD_DEFAULT);
    strcpy(common.wifiClient_SSID, WIFI_CLIENT_DEFAULT_SSID);
    strcpy(common.wifiClient_PSK, WIFI_CLIENT_DEFAULT_PSK);
    strcpy(common.wifiAP_PSK, WIFI_AP_DEFAULT);
    for (i = 0; i < BEACON_COUNT; i++)
    {
        items[i].enable = false;
        items[i].freq = 3510.0;
        strcpy(items[i].text, "VVV TEST");
        items[i].speed = 45;
        items[i].power = 1;
    }
  }
  else
  {
    for (i = 0; i < BEACON_COUNT; i++)
    {
      EEPROM.get(eeAdr, items[i]);
      eeAdr += sizeof(bcnConfigItem_t);
    }
  }
  EEPROM.end();
}

void bcnConfig::writeEE()
{
  uint8_t   i;
  uint16_t  eeAdr = 0;
  
  EEPROM.begin(EESIZE);

  EEPROM.put(eeAdr, common);
  eeAdr += sizeof(bcnConfigCom_t);
 
  for (i = 0; i < BEACON_COUNT; i++)
  {
    EEPROM.put(eeAdr, items[i]);
    eeAdr += sizeof(bcnConfigItem_t);
  }

  EEPROM.commit();
  EEPROM.end();
}

void bcnConfig::dumpToSerial()
{
  uint8_t   i;
  Serial.printf("-- Config dump (%u): --\n", BEACON_COUNT);
  Serial.printf("common.enable=%u\n", common.enable);
  Serial.printf("common.freqCorr=%d\n", common.freqCorr);
  Serial.printf("common.pause=%d\n", common.pause);
  Serial.printf("common.http_login=\"%s\"\n", common.http_login);
  Serial.printf("common.http_passwd=\"%s\"\n", common.http_passwd);
  Serial.printf("common.wifiClient_SSID=\"%s\"\n", common.wifiClient_SSID);
  Serial.printf("common.wifiClient_PSK=\"%s\"\n", common.wifiClient_PSK);
  Serial.printf("common.wifiAP_PSK=\"%s\"\n", common.wifiAP_PSK);
  
  for (i = 0; i < BEACON_COUNT; i++)
  {
    Serial.printf("%u ena=%u freq=%.3f text=\"%s\" speed=%u power=%u\n", i, items[i].enable, items[i].freq, items[i].text, items[i].speed, items[i].power);
  }
  Serial.println("-------------");
}
