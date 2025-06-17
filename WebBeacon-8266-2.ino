#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <Wire.h>

#include "si5351.h"
#include "si_bcn2.h"
#include "config.h"

#define VERSION "1.1"

#define POWER_CNT 6 /* number of RF power levels / кол-во уровней мощности */
#define POWER_PIN 0 /* GPIO 0 (ESP8266 D3) */
#define BAND1_PIN 14 /* GPIO 14 (ESP8266 D5) */
#define BAND2_PIN 12 /* GPIO 12 (ESP8266 D6) */
#define BAND3_PIN 13 /* GPIO 13 (ESP8266 D7) */

// LPF cutoff frequencies (kHz)
#define BAND1_LPF 2500
#define BAND2_LPF 4300
#define BAND3_LPF 7500

// for WiFi AP
#define WIFILOCALAP "ESP8266RadioBeacon"
IPAddress ip_local(192,168,55,1);
IPAddress ip_gateway(192,168,55,1);
IPAddress ip_subnet(255,255,255,0);

// RF output power values table 0..255
const uint8_t powerTab[POWER_CNT] = 
{
  27, 60, 90, 120, 180, 255
};

const char      compile_date[] = __DATE__ " " __TIME__;
Si5351          si5351;
ESP8266WebServer server(80);
bcnConfig       cfg;
si_bcn2         bcn;
uint16_t        bcnDotDelay;
uint8_t         bcnPower;
bool            wifiConnected = false;

const String mainForm1 = "<html>\
  <head>\
    <title>Beacon configuration page</title>\
    <style>\
      body { background-color: #c0c0c0; font-family: Arial, Helvetica, Sans-Serif; Color: #000080; }\
    </style>\
  </head>\
  <body>\
    <h1><a href=\"/\">Radio Beacon configuration</a></h1>\
    <form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/updateConfig/\">\
    <table cellspacing=0 cellpadding=0 border=1>\
    <tr><td>\
    <table align=left cellspacing=0 cellpadding=5 bgcolor=#e0e0e0 border=0>\
    <tr><th> Beacon number </th><th> Enabled </th><th> Frequency (kHz) </th><th> Beacon text (max " + String(BEACON_TEXT_MAX - 1) + String(" chars)</th><th>CW Speed (CPM)</th><th> Power (1-" + String(POWER_CNT) + ") </th></tr>");

const String mainForm3 = "</table>\
 </td></tr>\
 <tr><td align=center>\
   <input type=\"submit\" value=\"Write config\">\
   <input type=\"reset\" value=\"Reset changes\">\
 </td></tr>\
 </table></form>\
<br>\
Web controlled Radio Beacon ver." + String(VERSION) + " by Alexey Igonin compiled at " + String(compile_date) + "\
</body>\
</html>";


void rfOn()
{
  si5351.output_enable(SI5351_CLK0, 1);
  if ((bcnPower >= 0) && (bcnPower < POWER_CNT)) analogWrite(POWER_PIN, powerTab[bcnPower]);
  digitalWrite(LED_BUILTIN, 0);
}

void rfOff()
{
  si5351.output_enable(SI5351_CLK0, 0);
  analogWrite(POWER_PIN, 0);
  digitalWrite(LED_BUILTIN, 1);
}

void setFreq(double f)
{
  if ((f > 100) && (f < 160000))
  {
    si5351.set_freq(f * 100000ULL, SI5351_CLK0);
    si5351.output_enable(SI5351_CLK0, 0);

    if (f < BAND1_LPF) 
    {
        digitalWrite(BAND1_PIN, 1);
        digitalWrite(BAND2_PIN, 0);
        digitalWrite(BAND3_PIN, 0);
    }
    else if (f < BAND2_LPF)
    {
        digitalWrite(BAND1_PIN, 0);
        digitalWrite(BAND2_PIN, 1);
        digitalWrite(BAND3_PIN, 0);
    }
    else if (f < BAND3_LPF)
    {
        digitalWrite(BAND1_PIN, 0);
        digitalWrite(BAND2_PIN, 0);
        digitalWrite(BAND3_PIN, 1);
    }
  }
}

void setBandOff()
{
   digitalWrite(BAND1_PIN, 0);
   digitalWrite(BAND2_PIN, 0);
   digitalWrite(BAND3_PIN, 0);
}

void handleRoot()
{
  String str;
  String str1;
  uint8_t i, j;
  String wifistat;

  if (!server.authenticate(cfg.common.http_login, cfg.common.http_passwd)) 
  {
     return server.requestAuthentication();
  }

  str = mainForm1;
  
  for (i = 0; i < BEACON_COUNT; i++) // parameters of each beacon
  {
    str += "<tr><td align=center>" + String(i + 1, DEC) + " </td>";
    if (cfg.items[i].enable) str1 = " checked"; else str1 = "";
    str += "<td align=center><input type=\"checkbox\" name=\"beaconena" + String(i, DEC) + "\" value=\"1\"" + str1 + "></td>";
    str += "<td><input type=\"text\" name=\"beaconfreq" + String(i, DEC) + "\" value=\"" + String(cfg.items[i].freq, 3) + "\" size=12></td>";
    str += "<td><input type=\"text\" name=\"beacontxt" + String(i, DEC) + "\" value=\"" + String(cfg.items[i].text) + "\" size=40 maxlength=" + String(BEACON_TEXT_MAX - 1) + "></td>";
    str += "<td><input type=\"text\" name=\"beaconspeed" + String(i, DEC) + "\" value=\"" + String(cfg.items[i].speed) + "\" size=6 maxlength=3></td>";
    str += "<td align=center><select name=\"beaconpower" + String(i, DEC) + "\">\n";
    for (j = 0; j < POWER_CNT; j++)
    {
        if (cfg.items[i].power == j) str1 = " selected"; else str1 = "";
        str += "<option value=" + String(j+1) + str1 + "> " + String(j+1) + " </option>";
    }
    str += "</select></td></tr>";
  }

  str += "</table></td></tr>\n<tr><td><table align=LEFT cellspacing=0 cellpadding=5 bgcolor=#e0e0e0 border=0 width=100%>\n";

  switch (WiFi.status())
  {
      case WL_NO_SSID_AVAIL:
        wifistat = "Configured SSID cannot be reached";
        break;
      case WL_CONNECTED:
        wifistat = "Connection successfully established";
        break;
      case WL_CONNECTION_LOST:
        wifistat = "Connection lost";
        break;
      case WL_CONNECT_FAILED:
        wifistat = "Connection failed";
        break;
      case WL_DISCONNECTED:
        wifistat = "Disconnected";
        break;
      default:
        wifistat = "Unknown";
  }
  
  // common params
  if (cfg.common.enable) str1 = " checked"; else str1 = "";
  str += "<tr><td style=\"width:50%;background-color:#e0f0f0;\" > All beacons enable</td><td style=\"background-color:#e0f0f0;\"><input type=\"checkbox\" name=\"commonena\" value=\"1\"" + str1 + "> </td><td><font size=-1>WiFi client status:</font></td></tr>";
  str += "<tr><td style=\"background-color:#e0f0f0;\"> Pause (0 - 3600 seconds)</td><td style=\"background-color:#e0f0f0;\"><input type=\"text\" name=\"commonpause\" value=\"" + String(cfg.common.pause) + "\" size=9></td><td><font size=-1>" + wifistat + "</font></td></tr>";
  str += "<tr><td style=\"background-color:#e0f0f0;\"> Frequency correction</td><td style=\"background-color:#e0f0f0;\"><input type=\"text\" name=\"commonfreqcorr\" value=\"" + String(cfg.common.freqCorr) + "\" size=12></td><td><font size=-1>Client IP: " + WiFi.localIP().toString() + "</font></td></tr>";
  str += "<tr><td style=\"background-color:#d0e0e0;\"> HTTP login (4 - " + String(HTTP_LOGIN_MAXLEN) + " chars)</td><td style=\"background-color:#d0e0e0;\"><input type=\"text\" name=\"commonlogin\" value=\"" + String(cfg.common.http_login) + "\" size=12 maxlength=" + String(HTTP_LOGIN_MAXLEN) + "></td><td><font size=-1>Local AP SSID: " + WIFILOCALAP + "</font></td></tr>";
  str += "<tr><td style=\"background-color:#d0e0e0;\"> HTTP password (4 - " + String(HTTP_LOGIN_MAXLEN) + " chars, leave blank to not change)</td><td style=\"background-color:#d0e0e0;\"><input type=\"text\" name=\"commonpasswd1\" value=\"\" size=12 maxlength=" + String(HTTP_LOGIN_MAXLEN) + "></td><td><font size=-1>Local AP IP: " + WiFi.softAPIP().toString() + "</font></td></tr>";
  str += "<tr><td style=\"background-color:#e0f0f0;\"> WiFi Client SSID (max " + String(WIFI_MAXLEN) + " chars)</td><td style=\"background-color:#e0f0f0;\"><input type=\"text\" name=\"commonwificlissid\" value=\"" + String(cfg.common.wifiClient_SSID) + "\" size=15 maxlength=" + String(WIFI_MAXLEN) + "></td><td><font size=-1>Connected to AP: " + String(WiFi.softAPgetStationNum()) + " client(s)</font></td></tr>";
  str += "<tr><td style=\"background-color:#e0f0f0;\"> WiFi Client PSK (max " + String(WIFI_MAXLEN) + " chars)</td><td style=\"background-color:#e0f0f0;\"><input type=\"text\" name=\"commonwificlipsk\" value=\"" + String(cfg.common.wifiClient_PSK) + "\" size=15 maxlength=" + String(WIFI_MAXLEN) + "></td><td>&nbsp;&nbsp;</td></tr>";
  str += "<tr><td style=\"background-color:#d0e0e0;\"> WiFi Access Point PSK (8 - " + String(WIFI_MAXLEN) + " chars)</td><td style=\"background-color:#d0e0e0;\"><input type=\"text\" name=\"commonwifiappsk\" value=\"" + String(cfg.common.wifiAP_PSK) + "\" size=15 maxlength=" + String(WIFI_MAXLEN) + "></td><td>&nbsp;&nbsp;</td></tr>";
  
  str += mainForm3;
  server.send(200, "text/html", str);
}


void handleUpdateConfig() 
{
  uint8_t i;
  uint8_t j;
  String  paramName;
  bool    wifiCliUpdated = false;
  bool    wifiAPUpdated = false;

  if (!server.authenticate(cfg.common.http_login, cfg.common.http_passwd)) 
  {
      return server.requestAuthentication();
  }
  
  if (server.method() != HTTP_POST) 
  {
      server.send(405, "text/plain", "Method Not Allowed");
      Serial.println("method != HTTP_POST - Method Not Allowed");
      return;
  }
  
  for (j = 0; j < BEACON_COUNT; j++)
  {
      cfg.items[j].enable = false;
  }
  cfg.common.enable = false;

  for (i = 0; i < server.args(); i++)
  {
      for (j = 0; j < BEACON_COUNT; j++)
      {
          paramName = "beaconena" + String(j);
          if (paramName == server.argName(i)) cfg.items[j].enable = (server.arg(i) == "1");
          paramName = "beaconfreq" + String(j);
          if (paramName == server.argName(i)) cfg.items[j].freq = server.arg(i).toDouble();
          paramName = "beacontxt" + String(j);
          if (paramName == server.argName(i)) server.arg(i).toCharArray(cfg.items[j].text, BEACON_TEXT_MAX);
          paramName = "beaconspeed" + String(j);
          if ((paramName == server.argName(i)) && (server.arg(i).toInt() >= 10) && (server.arg(i).toInt() <= 200)) cfg.items[j].speed = server.arg(i).toInt();
          paramName = "beaconpower" + String(j);
          if ((paramName == server.argName(i)) && (server.arg(i).toInt() > 0) && (server.arg(i).toInt() <= POWER_CNT)) cfg.items[j].power = server.arg(i).toInt() - 1;
      }

      if (server.argName(i) == "commonena") cfg.common.enable = (server.arg(i) == "1");
      if ((server.argName(i) == "commonpause") && (server.arg(i).toInt() >= 0) && (server.arg(i).toInt() <= 3600)) cfg.common.pause = server.arg(i).toInt();
      if (server.argName(i) == "commonfreqcorr") cfg.common.freqCorr = server.arg(i).toInt();

      if ((server.argName(i) == "commonlogin") && (server.arg(i).length() >= 4)) server.arg(i).toCharArray(cfg.common.http_login, HTTP_LOGIN_MAXLEN);
      if ((server.argName(i) == "commonpasswd1") && (server.arg(i).length() >= 4)) server.arg(i).toCharArray(cfg.common.http_passwd, HTTP_LOGIN_MAXLEN);
      if ((server.argName(i) == "commonwificlissid") && (server.arg(i) != "")) 
      {
        if (String(cfg.common.wifiClient_SSID) != server.arg(i)) wifiCliUpdated = true;
        server.arg(i).toCharArray(cfg.common.wifiClient_SSID, WIFI_MAXLEN);
      }
      if ((server.argName(i) == "commonwificlipsk") && (server.arg(i) != ""))
      {
        if (String(cfg.common.wifiClient_PSK) != server.arg(i)) wifiCliUpdated = true;
        server.arg(i).toCharArray(cfg.common.wifiClient_PSK, WIFI_MAXLEN);
      }
      if ((server.argName(i) == "commonwifiappsk") && (server.arg(i).length() >= 8))
      {
        if (String(cfg.common.wifiAP_PSK) != server.arg(i)) wifiAPUpdated = true;
        server.arg(i).toCharArray(cfg.common.wifiAP_PSK, WIFI_MAXLEN);
      }

  }

  cfg.writeEE();
  Serial.println("Configuration updated and written to EEPROM");
  if (!cfg.common.enable) rfOff();
  si5351.set_correction(cfg.common.freqCorr, SI5351_PLL_INPUT_XO);
  handleRoot();
  if (wifiCliUpdated)
  {
    delay(1000);
    Serial.println("WiFi Client reconnect");
    WiFi.disconnect();
    WiFi.begin(cfg.common.wifiClient_SSID, cfg.common.wifiClient_PSK);
  }
  if (wifiAPUpdated)
  {
    delay(100);
    Serial.println("WiFi AP restart");
    Serial.print("Starting soft-AP ... ");
    Serial.println(WiFi.softAP(WIFILOCALAP, cfg.common.wifiAP_PSK, 1, false, 4) ? "Ready" : "Failed!");
  }
}


void handleNotFound()
{
  server.send(404, "text/plain", "File Not Found\n\n");
}


void setup(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(POWER_PIN, OUTPUT);
  pinMode(BAND1_PIN, OUTPUT);
  pinMode(BAND2_PIN, OUTPUT);
  pinMode(BAND3_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 1);
  Serial.begin(115200); // 
  analogWriteRange(255);
  analogWriteFreq(10000);
  setBandOff();
  
  delay(500);
  Serial.printf("\n-- RADIO BEACON ver.%s START --\n", VERSION);
  Serial.printf("Compiled at %s\n", compile_date);

  if(!si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0))
  {
    Serial.println("ERROR! si5351 not found on I2C bus!");
    while(1) {
      digitalWrite(LED_BUILTIN,HIGH);
      delay(200);
      digitalWrite(LED_BUILTIN,LOW);
      delay(100);
    }
  }
  else Serial.println("si5351 found");

  cfg.readEE();
  cfg.dumpToSerial();
  
  si5351.update_status();
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.set_correction(cfg.common.freqCorr, SI5351_PLL_INPUT_XO);
  setFreq(cfg.items[0].freq);
  
  bcn.setParams(rfOn, rfOff);
  bcn.setText(cfg.items[0].text);
  bcnDotDelay = (60.0 / (double)cfg.items[0].speed) * 100;
  bcnPower = cfg.items[0].power;

  WiFi.begin(cfg.common.wifiClient_SSID, cfg.common.wifiClient_PSK);
  delay(1000);

  Serial.print("MDNS responder ... ");
  Serial.println(MDNS.begin("RedioBeacon") ? "Ready" : "Failed!");  

  server.on("/", handleRoot);
  server.on("/updateConfig/", handleUpdateConfig);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(ip_local, ip_gateway, ip_subnet) ? "Ready" : "Failed!");  
  Serial.print("Starting soft-AP ... ");
  Serial.println(WiFi.softAP(WIFILOCALAP, cfg.common.wifiAP_PSK, 1, false, 4) ? "Ready" : "Failed!");
  Serial.printf("Soft-AP IP address = %s\n", WiFi.softAPIP().toString().c_str());
}


void loop(void)
{
  static uint32_t tmr;
  static uint8_t  beaconCurr = 0;
  static bool     inPause = false;


// --------- WiFi client section -------------
  if ((WiFi.status() == WL_CONNECTED) && !wifiConnected)
  {
      WiFi.setAutoReconnect(true);
      WiFi.persistent(true);
      wifiConnected = true;
      Serial.printf("WiFi client connected to: %s\n", cfg.common.wifiClient_SSID);
      Serial.printf("WiFi client IP address: %s/%s gateway: %s\n", WiFi.localIP().toString().c_str(), WiFi.subnetMask().toString().c_str(), WiFi.gatewayIP().toString().c_str());
  }
  if ((WiFi.status() != WL_CONNECTED) && wifiConnected)
  {
      wifiConnected = false;
      Serial.println("WiFi client connection lost");
  }
// --------- WiFi client section -------------


// -------- WEB section ---------------
  server.handleClient();
// -------- WEB section ---------------


// --------- beacon section ----------
  if (!cfg.common.enable) return;

  if ((inPause) && ((millis() - tmr) < (cfg.common.pause * 1000))) return;
  else inPause = false;

  if (bcn.isCompleted())
  {
     beaconCurr++;
     if (beaconCurr >= BEACON_COUNT)
     {  // all beacons done, pause
        beaconCurr = 0;
        inPause = true;
        tmr = millis();
     }
     if (cfg.items[beaconCurr].enable)
     {
        bcn.setText(cfg.items[beaconCurr].text);
        setFreq(cfg.items[beaconCurr].freq);
        bcnPower = cfg.items[beaconCurr].power;
        bcnDotDelay = (60.0 / (double)cfg.items[beaconCurr].speed) * 100;
     }
  }

  if ((millis() - tmr) >= bcnDotDelay)
  {
     tmr = millis();
     bcn.process();
  }
// --------- beacon section ----------
  
}
