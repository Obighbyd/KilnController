#include <WiFi.h> // For wifi connectivity.
#include <FS.h> // For filesystem Handling.
#include <AsyncTCP.h> // For Async web traffic.
#include <ESPAsyncWebServer.h> // The Web Server library.
#include <Preferences.h> // For eeprom storage.
#include <Adafruit_MAX31855.h> // For temp sensing.
#include <PID_v1.h> // For temp control processing.
#include "SSD1306.h" // For built in OLED display.

// Set default AP values
#define AP_SSID "KilnControl" // This is our hostname and AP Name.
#define AP_PASSWORD "MyPass" // This is the AP mode password.
#define RESET_PIN 16 // A LOW on this pin at startup resets saved WiFi preferences.

hw_timer_t *watchdogTimer = NULL;
static volatile bool wifi_connected = false;
String wifiSSID, wifiPassword;
unsigned long t_rotate, wifiTOut;
int degC, degF, dispmsg;
bool stAlone, progOn, apRunning; // Are we Standalone AP or WiFi Client, is a kiln program running, is AP running?
const char SETFORM[] PROGMEM = "<html><head><title>Kiln Controller</title><style>body{background-color:black;color:white;width:100%;font-size:large}</style>"
 "</head><body><p>If your Kiln Controller can't connect to the last WiFi network, move within range and restart it, or set it to Stand-Alone, or set up"
 " new network connection info below.</p><form method='post'><label>Mode: </label><br/><input type='radio' name='mode' value='0'/>Stand-Alone<br/>"
 "<input type='radio' name='mode' value='1' />Connect to Wifi below: <br/><label>SSID: </label><input name='ssid' length='32'><br/><label>Password: </label>"
 "<input name='pass' length='64'><br/><input type='submit' value='Save Config'></form><p style='color: deeppink;'>5UNAUTHORIZED USERS MUST CLOSE THIS"
 " PAGE IMMEDIATELY. This is NOT a public accessible web page.</p></body></html>";
const char HOME[] PROGMEM = "<html><head><title>Kiln Controller</title><style>body{background-color:black;color:white;width:100%;font-size:large}</style>"
 "</head><body><p>JnR's Kiln Controller</p><p>There is no program running currently.</p><p style='color: deeppink;'>UNAUTHORIZED USERS MUST CLOSE THIS"
 " PAGE IMMEDIATELY. This is NOT a public accessible web page.</p></body></html>";

Adafruit_MAX31855 thermocouple(15,13,12); //(SPI CLocK, ChipSelect, DataOut);
SSD1306  display(0x3c, 5, 4); // Initialize the OLED display using Wire library.
AsyncWebServer server(80); // Instantiate WWW Server.
Preferences preferences; // Instantiate EEPROM Preferences.

String IP2Str(IPAddress address){
 return String(address[0]) + "." + 
        String(address[1]) + "." + 
        String(address[2]) + "." + 
        String(address[3]);
}

void interruptReboot(){
  Serial.println("REBOOTING - Watchdog limit hit!");
  esp_restart_noos();
}

String handleSetup(String kcMode, String ssid, String pass) {
//  Serial.println("DEBUG-handlesetup sub started.");
//  Serial.println("DEBUG-handlestring mode:" + kcMode +" ssid:"+ ssid +" pass:"+ pass);
  String tmpStr;
  if ((kcMode != "0") && (kcMode != "1")) {
    return "No Mode selected. Go back and correct please.";
  }
  if (kcMode == "0") { // Standalone mode - set net prefs to none.
    ssid = "none";
    pass = "none";
  } else { // Network Client mode - validate and set net prefs.
    ssid.trim();
    pass.trim();
    if ((ssid == "") || (ssid.length() > 32)) {
      return "Bad SSID. Go back and correct please.";
    }
    if ((pass == "") || (pass.length() > 20)) {
      return "Bad password. Go back and correct please.";
    }
  }
//  Serial.println("DEBUG - saving ssid:"+ssid+" pass:"+pass);
  preferences.begin("wifi", false); // Note: Namespace name is limited to 15 chars
  preferences.putString("ssid", ssid);
  preferences.putString("password", pass);
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawStringMaxWidth(0,0,128,tmpStr);
  display.display();
  preferences.end();
  return "Saved. System rebooting.";
}

void setup() {
// Setup Environment.
  Serial.begin(115200);
  Serial.println("RNJ-KilnControl Setting Up");
  display.init();
  display.flipScreenVertically();
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.clear();
  display.drawStringMaxWidth(0,0,128,"RNJ-KilnControl Setting Up.");
  display.display();
  watchdogTimer = timerBegin(0, 80, true); // timer 0, divisor 80
  timerAlarmWrite(watchdogTimer, 10000000, false); // set time in uS for timeout
  timerAttachInterrupt(watchdogTimer, &interruptReboot, true);
  //timerAlarmEnable(watchdogTimer); // enable interrupt with this line, but not until a program is running.

// SSID Reset Pin detect. For changing AP settings or if forgotten.
  pinMode(RESET_PIN, INPUT);
  preferences.begin("wifi", false);
  if (digitalRead(RESET_PIN) == LOW) {
    // Reset Wifi Preferences for SSID, PW.
    preferences.putString("ssid", "none");
    preferences.putString("password", "none");
    display.clear();
    display.drawStringMaxWidth(0,0,128,"Saved WiFi config erased.");
    display.display();
    delay(1000);
  }
  wifiSSID =  preferences.getString("ssid", "none");           //EEPROM key ssid
  wifiPassword =  preferences.getString("password", "none");   //EEPROM key password
  preferences.end();
//  WiFi.onEvent(WiFiEvent); // Link WiFi events to subroutine WiFiEvent.
  
// Start AP or Client.
  if ((wifiSSID == "none") || (wifiSSID == "")) {
    stAlone = true;
    WiFi.mode(WIFI_MODE_AP); // Run as AP only.
    WiFi.softAP(AP_SSID,AP_PASSWORD);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("Access Point Started as ");
    Serial.println(String(AP_SSID));
    Serial.print("Connect To: ");
    Serial.println(myIP);
    display.clear();
    display.drawStringMaxWidth(0,0,128,"Connect To: " + IP2Str(myIP));
    display.display();
  } else {
    stAlone = false;
    Serial.print("WiFi Client to SSID: ");
    Serial.println(wifiSSID);
    WiFi.mode(WIFI_MODE_STA); // Run as client only.
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    WiFi.setHostname(AP_SSID);
    wifiTOut = millis() + 30000; // In 30 seconds, give up on wifi client connection.
  }

// Set up web server handlers.
  server.on("/hello", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Hello World");
//    Serial.println("DEBUG- Hello web response");
  });
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request){
//    Serial.println("DEBUG- setup page called");
    request->send(200, "text/html", SETFORM);
//    Serial.println("DEBUG-setform sent to client.");
  });  
  server.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("DEBUG- setup response page called");
    if (request->hasParam("mode", true)) {
//      Serial.println("DEBUG-mode param found, processing setup info.");
      int numParams = request->params();
      String pmode, pssid, ppass;
//      Serial.println(numParams);
      for (int i=0;i<numParams;i++){
        AsyncWebParameter* p = request->getParam(i);
        if (p->name() == "mode") pmode = p->value().c_str();
        if (p->name() == "ssid") pssid = p->value().c_str();
        if (p->name() == "pass") ppass = p->value().c_str();
      }
//      Serial.println("DEBUG-mode:" + pmode +" ssid:"+ pssid +" pass:"+ ppass);
      String Response = handleSetup(pmode, pssid, ppass);
      request->send(200, "text/plain", Response);
//      Serial.println(Response.substring(0,5));
      if (Response.substring(0,5) == "Saved") {
        Serial.println("Network info saved, rebooting.");
        delay(5000);
        ESP.restart();
      }
    }
  });
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", HOME);
    Serial.println("DEBUG- home page request");
  });  
  server.begin();

// Initialize variables
  t_rotate = millis() + 5000;
  timerAlarmEnable(watchdogTimer); // enable interrupt with this line.
  dispmsg = 0;
}

void loop() {
  timerWrite(watchdogTimer, 0); // Feed the watchdog!
  if (millis() > t_rotate) { // Time to rotate display messages.
    dispmsg++;
    display.clear();
    switch (dispmsg) {
      case 1: {
        String dispStr;
        if (stAlone) {
          // display our IP for users to find us.
          IPAddress myIP = WiFi.softAPIP();
          dispStr = "Connect to " + String(AP_SSID) + " then " + IP2Str(myIP);
        } else {
          if (WiFi.status() == WL_CONNECTED) {
            // if wifi connected, display SSID and IP so they can find us. 
            dispStr = "Connect to " + wifiSSID + " then " + IP2Str(WiFi.localIP());
          } else {
            // If wifi not connected, display connecting to SSID.
            dispStr = "Connecting to " + wifiSSID + "...";
            if (apRunning) {
              dispStr = "WiFi Failed. " + String(AP_SSID) + " running.";
            }
            if ((millis() > wifiTOut) && (!apRunning)) {
              // wifi hasnt connected. Start an AP but retry Wifi.
              WiFi.mode(WIFI_MODE_APSTA); // Run as AP only.
              WiFi.softAP(AP_SSID,AP_PASSWORD);
              apRunning = true;
              dispStr = "Retrying WiFi. AP " + String(AP_SSID) + " Started.";
              WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
            }
          }
        }
//        Serial.println(dispStr);
        display.setFont(ArialMT_Plain_16);
        display.drawStringMaxWidth(0,0,128,dispStr);
        }
      break;
      case 2: {
        display.setFont(ArialMT_Plain_24);
        display.drawStringMaxWidth(0,0,128,"No running program."); 
//        Serial.println("No running program.");
      }
      break;
      case 3: {
        unsigned long allSeconds=millis()/1000;
        int runHours= allSeconds/3600;
        int secsRemaining=allSeconds%3600;
        int runMinutes=secsRemaining/60;
        int runSeconds=secsRemaining%60;
        char buf[21];
        sprintf(buf,"Runtime %02d:%02d:%02d",runHours,runMinutes,runSeconds);
//        Serial.println(buf);
        display.setFont(ArialMT_Plain_16);
        display.drawStringMaxWidth(0,0,128,buf);
      }
      break;
      case 4: {
        dispmsg = 0;
        degF = thermocouple.readFarenheit();
//        Serial.print("Temp: ");
//        Serial.print(degF);
//        Serial.println("F");
        String temp = "Kiln Temp: " + String(degF);
        display.setFont(ArialMT_Plain_24);
        display.drawStringMaxWidth(0,0,128,String(temp));
      }
      break;
      default:
        break;
    }
    display.display();
    t_rotate = millis() + 5000;
  }

  // ToDo - the kiln parts!
  // and redesign the display once i figure out what its capable of. And make web pages look better on phones.
  // I realize this is not a secure web server (yet). I realize the OLED library supports much more sophisticated paging methods.
  // I intend to add the major parts first.. then tweak.. and I constantly refactor and reduce and redesign. 
  // I dont consider it a waste of my time. I enjoy writing the code, testing it, seeing how things work.
  // At work, its all about methodologies and analysis and SCRUM and all that crap.. here, i get to relax and play.
  
}
