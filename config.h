
#define BEACON_COUNT 10
#define BEACON_TEXT_MAX 60
#define BEACON_START 43166
#define HTTP_LOGIN_MAXLEN 12
#define WIFI_MAXLEN 32

// common parameters
struct bcnConfigCom_t
{
  uint16_t  start;
  bool      enable;
  int32_t   freqCorr;
  uint16_t  pause;
  char      http_login[HTTP_LOGIN_MAXLEN + 1];
  char      http_passwd[HTTP_LOGIN_MAXLEN + 1];
  char      wifiClient_SSID[WIFI_MAXLEN + 1];
  char      wifiClient_PSK[WIFI_MAXLEN + 1];
  char      wifiAP_PSK[WIFI_MAXLEN + 1];
};

// parameters of each beacon
struct bcnConfigItem_t
{
  char      text[BEACON_TEXT_MAX];
  bool      enable;
  double    freq;
  uint16_t  speed;
  uint8_t   power;
};

class bcnConfig
{
  public:
  bcnConfigItem_t items[BEACON_COUNT];
  bcnConfigCom_t  common;

  void    readEE();
  void    writeEE();
  void    dumpToSerial();

};
