#include "ESPAsyncWebServer.h"
//#include <TimeLib.h> //Για μετατροπή από epoch time υπάρχει στο NTPClient_Generic.h

static void rst(void*);
void callback(String, byte*, unsigned int);
void reconnect(void);
void initNetwork(void);
void startWiFi(void);
void notFound(AsyncWebServerRequest*);
String processor(const String&);
String processor_net(const String&);
String ChartData(byte, bool);
void saveSettings(AsyncWebServerRequest*, char*);
void setSwitch(AsyncWebServerRequest*, String, byte, bool*);
void setSwitchState(byte, bool);
static void testcon(void*);
void onConnect(void*, AsyncClient*);  //Η συνάρτηση καλείται όταν γίνει σύνδεση στον testhost π.χ. www.google.com
void onConnectUpd(void*, AsyncClient*); //Η συνάρτηση καλείται όταν γίνει σύνδεση στον update host
void download(char*); //Κάνει αίτημα για σύνδεση στον Update Host

//Δημιουργία αντικειμένων Server και Clients
AsyncWebServer server(80); //Ο τοπικός web server
AsyncClient* test_client = new AsyncClient; //Δοκιμαστική σύνδεση TCP για έλεγχο σύνδεσης με διαδίκτυο
AsyncClient* update_client = new AsyncClient; //Σύνδεση στον Update Host για κατέβασμα αναβαθμήσεων

int chunks = 0;
bool dl_block = false;
unsigned long dl_delay;

//Ασύγχρονη λήψη αρχείου σε κομμάτια των 536-540 bytes
//Για έλεγχο σωστού κατεβάσματος του αρχείο στο SPIFFS κατεβάζω το αρχείο από τον ενσωματωμένο server (ASYNC WEB SERVER)
//π.χ. στο server.on("/log....., βάζω request->send(SPIFFS, "/update/firmware.bin", "application/octet-stream"); και βάζω το πρόγραμμα colordiff (sudo apt-get install colordiff)
//Μετά γράφω colordiff -y <(xxd firmware.bin) <(xxd firmware2.bin) ώστε να συγκρίνω το αρχικό αρχείο (ftp στον web server) με αυτό που κατέβασα αρχικά στο esp και μετά στον browser.
//Αν είναι ίδια δεν βλέπω κανένα χρώμα. Αν υπάρχει διαφορά την εμφανίζει με χρώματα.
static void handleData(void* arg, AsyncClient* client, const void *data, size_t len) 
{
  //Serial.printf("\n data received from %s \n", client->remoteIP().toString().c_str()); //Debug
  char buffer[550]; //Το κάθε κομμάτι έχει len 536 - 540
  char *b = 0, *ptr;
  char tmp[4];
  int d;
  static int sc, s;
  static unsigned int len_max;
  static File f;
  //Serial.println(len); //Debug
  //Serial.write((uint8_t*)data, len); //Debug
  if (len > 0)
     {
      //Αντέγραψε όλο το τμήμα (540 bytes) στο array buffer
      memcpy(buffer, (uint8_t*)data, len);
      if (chunks == 0) //Αν είναι το 1ο τμήμα
         {
          s=0;
          len_max = len; //Κράτα το αρχικό μήκος του πρώτου πακέτου
          strncpy(tmp, (buffer + 9), 4); //Πάρε το Status Code
          tmp[3] = '\0'; //Τερμάτισε το character array
          sc = atoi(tmp); //Κάνε το Status Code ακέραιο
          //Βρες που τελειώνουν τα HTTP Headers. Χωρίζεται από το κυρίως μήνυμα με \r\n\r\n
          ptr = strstr(buffer, "\r\n\r\n"); //Ο δείκτης δείχνει στην πρώτη εμφάνιση του \r\n\r\n δηλ. στο τέλος του HTTP Header
          b = buffer; //Ο δείκτης b δείχνει στην αρχή του buffer
          d = ptr - b; //Η διαφορά τους είναι το μήκος του header
          len = len - d - 4; //Το νέο μήκος δίχως τα headers. Νέο len μόνο για το 1ο τμήμα
          b = ptr + 4; //Δείξε στην αρχή του σώματος. (4 για τα 4 bytes \r\n\r\n που αρχικά είναι μέσα)
          if (sc == 200) //Αν απάντησε HTTP OK
              f = LittleFS.open(FileName, "w"); //Άνοιξε το αρχείο για εγγραφή. Λειτουργεί και για δυαδικά αρχεία όπως το wb 
         }
      //Αλλιώς δεν είναι το πρώτο οπότε πάρε ολόκληρο το buffer 
      else
          b = buffer; //Δείξε στην αρχή των επόμενων τμημάτων πλην του πρώτου
      chunks++; //Κάθε φορά που καλείται αυτή η συνάρτηση να αυξάνεται το chunks
     }
  if (sc == 200) //Αν απάντησε HTTP OK
     {
      //Αν το αρχείο έχει ανοιχθεί
      if (f) 
         {
          //Αν είναι το 1ο Ή έχουν το ίδιο μήκος με το αρχικό (πριν την αφαίρεση των Headers) π.χ. 536
          if ((len == len_max || chunks == 1) && len_max >= 536)
             {
              f.write((uint8_t*)b, len); //Γράψε το buffer στο αρχείο
              s += len;
              Serial.println(s); //Debug για να βλέπω την πρόοδο λήψης
             }
          //Αλλιώς είναι το τελευταίο τμήμα
          else
             {
              f.write((uint8_t*)b, len); //Γράψε στο αρχείο το τελευταίο τμήμα
              f.close(); //Κλείσε το αρχείο
              dl_block = false; //Ξεμπλόκαρε για την chk_Dl()
              Serial.println(FileName + " - OK");
              //client->stop(); //Δεν χρειάζεται για HTTP 1.0 
             }
         }
     }
}

String files[15];
String filesDel[15];

//Ανοίγει το αρχείο files.txt που κατέβασε και βάζει τα ονόματα αρχείων στον πίνακα files
void readFileNames(byte* x, byte* y)
{
 String line;
 byte i = 0, j = 0;
 byte state = 0;
 File file = LittleFS.open("/upd_files.txt", "r");
 if (!file) 
    {
     Serial.println(F("Failed to open file files.txt."));
     return;
    }
 Version = "";
 while (file.available()) 
       {
        line = file.readStringUntil('\n');
        //Αν είναι κανονική γραμμή με filename
        if ((line != "") && (line.indexOf("[") == -1) && (line.indexOf("#") == -1)) 
           {
            //Αν είναι σε κατάσταση AddFiles
            if (state == 1)
               {
                files[i] = line;
                i++;
               }
            //Αν είναι σε κατάσταση DelFiles
            else if (state == 2)
               {
                filesDel[j] = line;
                j++;
               }
           }
        //Είναι η 1η γραμμή με tag [Version=
        else if (line.indexOf("[Version=") > -1)
           {
            byte Lidx = line.indexOf("[Version=");
            byte Ridx = line.indexOf("]");
            Version = line.substring(Lidx+9, Ridx);
            Serial.println(Version);
            if (Version <= VERSION) //Αν είναι η ίδια ή μικρότερη δεν γίνεται αναβάθμιση
               {
                CanUpdate = false;
                return;
               }
            else //Διαφορετικά γίνεται αναβάθμιση
                CanUpdate = true;
           }
        else if (line.indexOf("[AddFiles]") > -1)
            state = 1;
        else if (line.indexOf("[DelFiles]") > -1)
            state = 2;
       }
 file.close();
 *x = i;
 *y = j;
}

//Καλείται συνεχώς από το loop και ελέγχει αν έχει δωθεί εντολή για κατέβασμα αρχείων από τον update server
void chk_Dl()
{
 static byte i, n, m;
 bool fw_flag = false;
 //Αν η κατάσταση είναι 0 φύγε από εδώ.
 if (Dl_State == 0)
     return;
 //Αρχικά κατεβάζει το αρχείο οδηγιών files.txt
 if (Dl_State == 1 && !dl_block)
    {
     download((char*)("files.txt"));
     Dl_State = 2; //Προετοιμασία για επόμενο στάδιο
     dl_block = true; //Μπλόκαρε μέχρι να ελευθερωθεί από την handleData παραπάνω
    }
 //Διάβασε το περιεχόμενο του αρχείου files.txt
 else if (Dl_State == 2 && !dl_block)
    {
     readFileNames(&n, &m);
     if (CanUpdate)
        {
         Dl_State = 3;
         i = 0;
         dl_delay = millis();
        }
     else
        {
         DLFileName = "Δεν υπάρχει διαθέσιμη ενημέρωση.";
         Dl_State = 0;
        }
     return;
    }
 //Κατέβασε ένα - ένα τα αρχεία
 //Περίμενε 1sec μεταξύ ολοκλήρωσης προηγούμενου και έναρξης επόμενου αρχείου
 else if (Dl_State == 3 && !dl_block && (millis() - dl_delay >= 1500))
    {
     dl_delay = millis(); 
     dl_block = true;   //Μπλόκαρε μέχρι να ελευθερωθεί από την handleData παραπάνω
     download((char*)files[i].c_str()); //Κατέβασε το αρχείο
     if (i < n-1) //Αν δεν έφτασε στο τελευταίο filename τότε
         i++;    //δείξε στο επόμενο για την επόμενη φορά
     else //Έδωσε όλα τα ονόματα για download
         {
          Dl_State = 4; 
          i = 0; //Μηδενίζει για νέα κλήση downloads
         }
     return;
    }
  else if (Dl_State == 4 && !dl_block)
   {
    Dl_State = 0; //Μην ξαναμπείς εδώ αν δεν πατηθεί το link στην σελίδα setup_net.html
    DLFileName += "Επιτυχής μεταφόρτωση"; //Μήνυμα ολοκλήρωσης για σελίδα fw_update.html
    Dl_OK = "true";
   }
  else if (Dl_State == 5)
   {
    Dl_State = 0;
    String src, dest;
    delay(500); //Περίμενε να κλείσει τα αρχεία ιστοσελίδας από τον web server
    for (int j=0; j < n; ++j) //Για όλα τα ονόματα αρχείων στο files[]
        {
         src = "/upd_" + files[j];
         dest = "/" + files[j];
         if (dest != "/firmware.bin") //Δεν θα μετακινήσεις το αρχείο firmware.bin
            {
             //SPIFFS.remove(dest);
             //SPIFFS.rename(src, dest); //Αφήνει σκουπίδια στο FS με αποτέλεσμα να αυξάνει σε μέγεθος κάθε φορά που κάνει update
             cp(src, dest); //Αντέγραψε το αρχείο
             delay(200);
             LittleFS.remove(src); //Σβήσε το αρχείο με πρόθεμα /upd_
             Serial.println("Update file " + dest); //Μήνυμα ολοκλήρωσης
            }
         else
             fw_flag = true; //Έχει κατεβεί αρχείο firmware.bin και πρέπει να γίνει εγκατάσταση παρακάτω
        }
    LittleFS.remove("/upd_files.txt"); //Διαγραφή αρχείου
    for (int j=0; j < m; ++j) //Για όλα τα ονόματα αρχείων στο files[]
        {
         dest = "/" + filesDel[j];
         if (LittleFS.exists(dest))
            {
             LittleFS.remove(dest);
             Serial.println("Delete file " + dest); //Μήνυμα ολοκλήρωσης
            }
        }
    if (fw_flag) //Αν υπάρχει αρχείο firmware.bin τότε να γίνει αναβάθμιση
        updateFirmware();
   }
}

static os_timer_t restartDelay;

//Επανεκκίνηση του ESP
static void rst(void* arg)
{
 os_timer_disarm(&restartDelay);
 ESP.restart();
}

//------------------------------------------- Node RED -----------------------------------------------------
//  (MQTT IN) -----------------------> (SWITCH) ---------------------------------> (MQTT OUT)
//  Topic : room1/light1_pub           Pass Through msg if payload... : Όχι        Topic : room1/light1_sub
//                                     On payload  : on                            Retain : κενό
//                                     Off payload : off
//                                     Topic : msg.payload
//----------------------------------------------------------------------------------------------------------
//Ενεργεί όταν λάβει μήνυμα mqtt σε κάποιο topic
//Delete retained messages: mosquitto_pub -t room1/light1_sub -u stav -P preveza -n -r -d
//Κάθε φορά που δημοσιεύεται ένα μήνυμα στα topic που έχει κάνει sub παρακάτω κατά την σύνδεση τότε
//καλείται αυτή η συνάρτηση.
void callback(String topic, byte* payload, unsigned int length1)
             {
              //Εμφανίζει το μήνυμα στη κονσόλα
              Serial.print(F("message arrived["));
              Serial.print(topic);
              Serial.println(F("]"));
              String msgg;
              for (unsigned int i = 0; i < length1; i++)
                  {
                   Serial.print((char)payload[i]);
                   msgg += (char)payload[i];
                  }
              Serial.println();
              //Διακόπτης Νο 1
              if (topic == (SetA1_TOPIC + "_sub"))
                 {
                  if (msgg == SetA1_ON)
                     {
                      if (sw1_pre == false) { setSwitchState(1, true); sw1_pre = true; } else { setSwitchState(1, false); sw1_pre = false; }
                     }
                  else if(msgg == SetA1_OFF)
                     {
                      if (sw1_pre == true) { setSwitchState(1, false); sw1_pre = false; } else { setSwitchState(1, true); sw1_pre = true; }
                     }
                 }
              //Διακόπτης Νο 2
              else if (topic == (SetA2_TOPIC + "_sub"))
                 {
                  if (msgg == SetA2_ON)
                     {
                      if (sw2_pre == false) { setSwitchState(2, true); sw2_pre = true; } else { setSwitchState(2, false); sw2_pre = false; }
                     }
                  else if(msgg == SetA2_OFF)
                     {
                      if (sw2_pre == true) { setSwitchState(2, false); sw2_pre = false; } else { setSwitchState(2, true); sw2_pre = true; }
                     }
                 }
               //Διακόπτης Νο 3
              else if (topic == (SetA3_TOPIC + "_sub"))
                 {
                  if (msgg == SetA3_ON)
                     {
                      if (sw3_pre == false) { setSwitchState(3, true); sw3_pre = true; } else { setSwitchState(3, false); sw3_pre = false; }
                     }
                  else if(msgg == SetA3_OFF)
                     {
                      if (sw3_pre == true) { setSwitchState(3, false); sw3_pre = false; } else { setSwitchState(3, true); sw3_pre = true; }
                     }
                 }
             }

//Καλείται μέσα από την loop()
void reconnect()
     {
      bool conn;
      if (MQTT_SRV == "-1")
          MQTT_State = "none";
      else if (InternAvail == "true") //&& (MQTT_SRV != "-1"))
         {
          while (!client.connected() && mqtt_state < 1) //Μία προσπάθεια
                {
                 Serial.println(F("Connecting to MQTT..."));
                 if (MQTT_USER == "")
                     conn = client.connect(MQTT_CLIENT.c_str());
                 else
                    {
                     mqttuser = MQTT_USER.c_str();
                     mqttpass = MQTT_PASS.c_str();
                     conn = client.connect(MQTT_CLIENT.c_str(), mqttuser, mqttpass);
                     //conn = client.connect("ESP8266Lights", mqttuser, mqttpass);
                    }
                 if (conn) 
                    {
                     Serial.println("connected");
                     mqtt_state = 0;
                     MQTT_State = "true";
                     //Συνδρομή στα παρακάτω topics
                     client.subscribe((SetA1_TOPIC + "_sub").c_str());
                     client.subscribe((SetA2_TOPIC + "_sub").c_str());
                     client.subscribe((SetA3_TOPIC + "_sub").c_str());
                    } 
                 else 
                    {
                     Serial.print(F("Failed with state "));
                     Serial.println(client.state());
                     mqtt_state++;
                     MQTT_State = "false";
                    }
                }
         }
     }

uint8_t MAC_array[6];
char MAC_char[19];     

//Αρχικοποίηση υπηρεσιών δικτύου
void initNetwork()
     {
      startWiFi(); //Ξεκίνησε το WiFi
      //Αρχικά κάνε σύνδεση με τον testhost
      test_client->connect(testhost, 80);
      delay(1000); //Περίμενε λίγο //500
      testcon(NULL); //Δες αν έγινε η σύνδεση
      //Ξεκίνησε τον NTP Client
      const char *ntp_name;
      ntp_name = NTP_SRV.c_str(); //Μετατροπή του String σε const char*
      //Άλλαξε το όνομα του Time Server
      timeClient.setPoolServerName(ntp_name);
      timeClient.setTimeOffset(NTP_TZ.toInt() * 3600); //Άλλαξε την ζώνη ώρας  
      timeClient.begin();
      timeClient.setUpdateInterval(SECS_IN_DAY); //Αν καλείται στο Loop θα κάνει update μια φορά την μέρα SECS_IN_DAY ή κάθε ώρα SECS_IN_HR
      //Serial.println(timeClient.getPoolServerName()); //Debug
      bool ntp_OK = false;
      byte ntp_tries = 0;
      if (InternAvail == "true")
         {
          while (!ntp_OK && ntp_tries < 20) //10
                {
                 timeClient.update();
                 delay(100);
                 if (timeClient.updated())
                     ntp_OK = true;
                 ntp_tries++;
                 delay(100);
                }
         }
      if (ntp_OK)
          Serial.println(F("Time Client Updated"));
      else
          Serial.println(F("Time Client Not Updated"));
      //Serial.println("UTC : " + timeClient.getFormattedUTCDateTime());
      Serial.println("LOC : " + timeClient.getFormattedDateTime());
      mqtt_server = MQTT_SRV.c_str();
      mqtt_port = MQTT_PORT.toInt();
      client.setServer(mqtt_server, mqtt_port);
      client.setCallback(callback);
      //=====================================================  Συναρτήσεις χειρισμού του Web Server ======================================================================
      //Ζητήθηκε η αρχική σελίδα /
      server.on("/", HTTP_GET, [](AsyncWebServerRequest * request)
               {
                if (!request->authenticate(LOG_NAME.c_str(), LOG_PASS.c_str()))
                    return request->requestAuthentication();
                else
                    request->send(LittleFS, "/index.html", String(), false, processor); //Χρήση processor αν στο index.html υπάρχει %ΛΕΞΗ%, τότε θα βάλει στη θέση ότι επιστρέψει η processor σαν ASP
                    //request->send(SPIFFS, "/index.html", "text/html"); //Χωρίς χρήση processor
               });
      //Ζητήθηκε η υποσελίδα /setup_wp.html
      server.on("/setup_wp.html", HTTP_GET, [](AsyncWebServerRequest * request)
               {
                if (!request->authenticate(LOG_NAME.c_str(), LOG_PASS.c_str()))
                    return request->requestAuthentication();
                else
                    request->send(LittleFS, "/setup_wp.html", String(), false, processor); //Χρήση processor αν στην HTML υπάρχει %ΛΕΞΗ%, τότε θα βάλει στη θέση ότι επιστρέψει η processor σαν ASP
                    //request->send(SPIFFS, "/setup_wp.html", "text/html"); //Χωρίς χρήση processor
               });
      //Ζητήθηκε η υποσελίδα /setup_net.html
      server.on("/setup_net.html", HTTP_GET, [](AsyncWebServerRequest * request)
               {
                if (!request->authenticate(LOG_NAME.c_str(), LOG_PASS.c_str()))
                    return request->requestAuthentication();
                else
                    request->send(LittleFS, "/setup_net.html", String(), false, processor_net); //Χρήση processor αν στην HTML υπάρχει %ΛΕΞΗ%, τότε θα βάλει στη θέση ότι επιστρέψει η processor σαν ASP
               });
      //Ζητήθηκε η υποσελίδα /log.html
      server.on("/log.html", HTTP_GET, [](AsyncWebServerRequest * request)
               {
                request->send(LittleFS, "/log.html", String(), false, processor); //Χρήση processor αν στην HTML υπάρχει %ΛΕΞΗ%, τότε θα βάλει στη θέση ότι επιστρέψει η processor σαν ASP
               });
      
      //Δεν βρέθηκε αρχείο
      server.onNotFound(notFound);

      //Ζητήθηκε το URL http://ip_address/status και απαντάει με το παρακάτω string σε μορφή text. Αυτό γίνεται κάθε 1sec και ενημερώνει τα στοιχεία span και div.
      server.on("/status", HTTP_GET, [](AsyncWebServerRequest * request)
               {
                String tmp = "<val id=\"Switch1_status\">" + Switch1_status + "</val>" + \
                             "<val id=\"Switch2_status\">" + Switch2_status + "</val>" + \
                             "<val id=\"Switch3_status\">" + Switch3_status + "</val>" + \
                             "<val id=\"time\">" + curTime() + "</val>" + \
                             "<val id=\"up_time\">" + upTime() + "</val>" + \
                             "<val id=\"internet\">" + InternAvail + "</val>" + \
                             "<val id=\"mqtt_state\">" + MQTT_State + "</val>" + \
                             "<val id=\"s_strength\">" + S_Strength + "</val>" + \
                             "<val id=\"filename\">" + DLFileName + "</val>" + \
                             "<val id=\"Dl_OK\">" + Dl_OK + "</val>";
                request->send(200, "text/plain", tmp);
               });

      //Ζητήθηκε το αρχείο JQuery
      server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             request->send(LittleFS, "/jquery.min.js", "text/javascript");
            });

      //Ζητήθηκε το αρχείο Chart για δημιουργία γραφικών παραστάσεων
      server.on("/chart.min.js", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             request->send(LittleFS, "/chart.min.js", "text/javascript");
            });
  
      //Ζητήθηκε το αρχείο CSS
      server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             request->send(LittleFS, "/style.css", "text/css");
            });

      //========================== Ενέργειες με το πάτημα κουμπιών ========================================
      //Ζητήθηκε η υποσελίδα /update της αρχικής σελίδας με GET κάποιες παραμέτρους π.χ. /update?button1=Ok
      server.on("/update", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             //Όνομα διακόπτη στο GET, Αριθμ. Διακόπτη, Μεταβλητή με την προηγούμενη κατάσταση
             setSwitch(request, "SW1", 1, &sw1_pre);
             setSwitch(request, "SW2", 2, &sw2_pre);
             setSwitch(request, "SW3", 3, &sw3_pre);
             //Επέστρεψε πίσω την ιστοσελίδα
             request->send(LittleFS, "/index.html", String(), false, processor);
            });

      //Ζητήθηκε η υποσελίδα /settime της αρχικής σελίδας με GET κάποιες παραμέτρους π.χ. /update?button1=Ok
      server.on("/settime", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             if (request->hasParam("TIME")) //Αν υπάρχει η παράμετρος
                {
                 String t = request->getParam("TIME")->value();
                 //Serial.println(t); //Debug
                 t.remove(t.length() - 3, 3);
                 tim = t.toInt() + (int) BROW_TO.toInt() * 3600;
                 Serial.printf("Set Date to: %4d-%02d-%02d %02d:%02d:%02d\n", year(tim), month(tim), day(tim), hour(tim)+2, minute(tim), second(tim));
                }
             //Επέστρεψε πίσω την ιστοσελίδα
             request->send(LittleFS, "/index.html", String(), false, processor);
            });

      //Ζητήθηκε η υποσελίδα /reset της αρχικής σελίδας με GET κάποιες παραμέτρους π.χ. /update?button1=Ok
      server.on("/reset", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             Serial.println(F("Reset All"));
             setSwitchState(1, false); setSwitchState(2, false); setSwitchState(3, false);
             sw1_pre = false; sw2_pre = false; sw3_pre = false;
             //Επέστρεψε πίσω την ιστοσελίδα
             request->send(LittleFS, "/index.html", String(), false, processor);
            });

      //Ζητήθηκε η υποσελίδα /save_wp με GET κάποιες παραμέτρους για να αποθηκεύει ρυθμίσεις της /setup_wp.html
      server.on("/save_wp", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             saveSettings(request, (char*)("/settings_wp.txt"));
             readSettings_wp(); 
             //Επέστρεψε πίσω την ιστοσελίδα setup_wp.html
             request->send(LittleFS, "/setup_wp.html", String(), false, processor);
            });

      //Ζητήθηκε η υποσελίδα /save_net με GET κάποιες παραμέτρους για να αποθηκεύει ρυθμίσεις της /setup_wp.html
      server.on("/save_net", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             saveSettings(request, (char*)("/settings_net.txt"));
             readSettings_net();
             //startWiFi();
             //Επέστρεψε πίσω την ιστοσελίδα setup_net.html
             request->send(LittleFS, "/setup_net.html", String(), false, processor_net);
             os_timer_setfn(&restartDelay, &rst, NULL);
             os_timer_arm(&restartDelay, 1000, false); //Μετά από 1sec κάνε reset ώστε να φορτώσει τις νέες ρυθμίσεις
            });

      //Ζητήθηκε η υποσελίδα /fw_update
      server.on("/fw_update", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             //Επέστρεψε πίσω την ιστοσελίδα fw_update.html
             request->send(LittleFS, "/fw_update.html", String(), false, processor_net);
             DLFileName = "";
             Dl_OK = "false";
             Dl_State = 1; //Ξεκίνα διαδικασία download
            });
      
      //Ζητήθηκε η υποσελίδα /FlashFW της αρχικής σελίδας με GET κάποιες παραμέτρους π.χ. /FlashFW?flash=yes
      server.on("/FlashFW", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             if (request->hasParam("flash")) //Αν υπάρχει η παράμετρος
                {
                 String t = request->getParam("flash")->value();
                 if (t == "yes")
                    {
                     Serial.println("Flashing Files ..."); //Debug
                     Dl_State = 5;
                    }
                 //Επέστρεψε πίσω την ιστοσελίδα
                 request->send(LittleFS, "/index.html", String(), false, processor_net);
                }        
            });
      
      //Ζητήθηκε η υποσελίδα /log της αρχικής σελίδας με GET κάποιες παραμέτρους π.χ. /log?Erase=yes
      server.on("/log", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             if (request->hasParam("Erase")) //Αν υπάρχει η παράμετρος
                {
                 String t = request->getParam("Erase")->value();
                 if (t == "yes")
                    {
                     //Serial.println("Erasing File ..."); //Debug
                     LittleFS.remove("/log.csv");
                    }
                 //Επέστρεψε πίσω την ιστοσελίδα
                 request->send(LittleFS, "/log.html", String(), false, processor);
                }
             else if (request->hasParam("View")) //Αν υπάρχει η παράμετρος save
                {
                 String t = request->getParam("View")->value();
                 if (t == "yes") //εμφάνισε το αρχείο σε άλλο tab
                    {
                     //request->send(SPIFFS, "/update/firmware.bin", "application/octet-stream");
                     request->send(LittleFS, "/log.csv", "text/plain");
                    }
                 else if (t == "save") //Κατέβασε το αρχείο για αποθήκευση
                    {
                     request->send(LittleFS, "/log.csv", "text/csv");
                    }
                }        
            });

      //Ζητήθηκε η υποσελίδα /chart της αρχικής σελίδας με GET κάποιες παραμέτρους π.χ. /log?Erase=yes
      server.on("/chart", HTTP_GET, [](AsyncWebServerRequest * request)
            {
             if (request->hasParam("Type")) //Αν υπάρχει η παράμετρος
                {
                 String t = request->getParam("Type")->value();
                 if (t == "switch1")
                    {
                     //Επέστρεψε πίσω την ιστοσελίδα
                     request->send(LittleFS, "/chart1.html", String(), false, processor);
                    }
                 else if (t == "switch2")
                    {
                     //Επέστρεψε πίσω την ιστοσελίδα
                     request->send(LittleFS, "/chart2.html", String(), false, processor);
                    }
                 else if (t == "switch3")
                    {
                     //Επέστρεψε πίσω την ιστοσελίδα
                     request->send(LittleFS, "/chart3.html", String(), false, processor);
                    }
                }       
            });
      // Start server
      server.begin();
     }

void startWiFi()
     {
      char hostname[15]; 
      //--- Στατική IP  ------------------------------------
      byte tmp[4];
      split(IP_ADDR, tmp, '.');
      IPAddress local_IP(tmp[0], tmp[1], tmp[2], tmp[3]);
      split(IP_GATE, tmp, '.');
      IPAddress gateway(tmp[0], tmp[1], tmp[2], tmp[3]);
      split(IP_MASK, tmp, '.');
      IPAddress subnet(tmp[0], tmp[1], tmp[2], tmp[3]);
      split(IP_DNS, tmp, '.');
      IPAddress primaryDNS(tmp[0], tmp[1], tmp[2], tmp[3]);   //optional
        
      WiFi.macAddress(MAC_array);
      MAC_char[18] = 0;
      sprintf(MAC_char,"%02x:%02x:%02x:%02x:%02x:%02x", MAC_array[0],MAC_array[1],MAC_array[2],MAC_array[3],MAC_array[4],MAC_array[5]);
      sprintf(hostname, "%s%02x%02x%02x", "LIGHTS-", MAC_array[3], MAC_array[4], MAC_array[5]);
      wifi_station_set_hostname(hostname); //Βάζει hostname αν έχει δυναμική απόδοση διεύθυνης
      Serial.println("MAC: " + String(MAC_char));   //", len=" + String(strlen(MAC_char)) );
      Serial.println("HOSTNAME: " + String(hostname));
      
      //--------------------------------------------------------------------
      byte retry_cnt = 0;
      if (SetSTA == "true")
         {
          WiFi.mode(WIFI_STA);
          if (IP_STATIC == "true")
             {
              //Αν έχει στατική IP βγάζω το σχόλιο κάτω
              WiFi.config(local_IP, gateway, subnet, primaryDNS);
             }
          //--- Λειτουργία client ----------------------------------------------
          char client_ssid[20];
          ST_SSID.toCharArray(client_ssid, 20);
          char client_password[20];
          ST_KEY.toCharArray(client_password, 20);
          WiFi.begin(client_ssid, client_password);
          while (WiFi.status() != WL_CONNECTED && retry_cnt < 30) 
                {
                 delay(1000);
                 Serial.println("Connecting to WiFi..");
                 retry_cnt++;
                }
          Serial.println("Start as Client connected to SSID: " + ST_SSID);
          Serial.print(F("Connect with your browser to "));
          Serial.println(WiFi.localIP());
         }
      else if (SetAP == "true")
         {
          //--- Λειτουργία AP.----------------------------------------------------
          char ap_ssid[20];
          AP_SSID.toCharArray(ap_ssid, 20);
          char ap_password[20];
          AP_KEY.toCharArray(ap_password, 20);
          WiFi.mode(WIFI_AP);
          int chan = AP_CHANNEL.toInt();
          WiFi.softAP(ap_ssid, ap_password, chan, 0, 4); //int channel, int ssid_hidden [0,1], int max_connection [1-4]
          //Δεν λειτουργεί τόσο σωστά με στατική IP. Καλύτερα να αφήνουμε την δυναμική δηλ. 192.168.4.1
          if (IP_STATIC == "true")
              WiFi.softAPConfig (local_IP, gateway, subnet);
          IPAddress IP = WiFi.softAPIP();
          Serial.println("Start as AP with SSID: " + AP_SSID);
          Serial.print(F("Connect with your browser to "));
          Serial.println(IP);
         }
     }

//Αν δεν υπάρχει κάποιο αρχείο στο σύστημα αρχείων τότε να εμφανίσει 404
void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/html", "<!DOCTYPE html><HTML><HEAD><TITLE>CLIMA CONTROL : File Not Found</TITLE></HEAD><BODY><H1>Not found</H1></BODY></HTML>");
}

//Ψάχνει για placeholders μέσα στο αρχείο HTML με σύνταξη %xxx%, όπως η ASP
String processor(const String& var)
{
  //Serial.println(var); //Debug
  //Σελίδα setup_wp.html
  if (var == "VERSION") //Αν το βρήκες
      return String(VERSION); //Γύρνα μια τιμή
  else if (var == "A1_LABL")
      return SetA1_LABL;
  else if (var == "A1_TOPIC")
      return SetA1_TOPIC;
  else if (var == "A1_ON")
      return SetA1_ON;
  else if (var == "A1_OFF")
      return SetA1_OFF;
  else if (var == "A2_LABL")
      return SetA2_LABL;
  else if (var == "A2_TOPIC")
      return SetA2_TOPIC;
  else if (var == "A2_ON")
      return SetA2_ON;
  else if (var == "A2_OFF")
      return SetA2_OFF;
  else if (var == "A3_LABL")
      return SetA3_LABL;
  else if (var == "A3_TOPIC")
      return SetA3_TOPIC;
  else if (var == "A3_ON")
      return SetA3_ON;
  else if (var == "A3_OFF")
      return SetA3_OFF;
  //Σελίδα chartXX.html
  else if (var == "ChartDataLabl")
      return ChartData(0, true);
  else if (var == "ChartData1")
      return ChartData(1, false);
  else if (var == "ChartData2")
      return ChartData(2, false);
  else if (var == "ChartData3")
      return ChartData(3, false);
  return String(); //Αν δεν το βρήκες γύρνα κενό string
}

//Ψάχνει για placeholders μέσα στο αρχείο HTML με σύνταξη %xxx%, όπως η ASP
String processor_net(const String& var)
{
  //Serial.println(var); //Debug
  //Σελίδα setup_net.html
  if (var == "VERSION") //Αν το βρήκες
      return String(VERSION); //Γύρνα μια τιμή
  else if (var == "SetAP")
      return SetAP;
  else if (var == "AP_SSID")
      return AP_SSID;
  else if (var == "AP_CHANNEL")
      return AP_CHANNEL;
  else if (var == "AP_ENCRYPT")
      return AP_ENCRYPT;
  else if (var == "AP_KEY")
      return AP_KEY;
  else if (var == "SetSTA")
      return SetSTA;
  else if (var == "ST_SSID")
      return ST_SSID;
  else if (var == "ST_KEY")
      return ST_KEY;
  else if (var == "IP_STATIC")
      return IP_STATIC;
  else if (var == "IP_ADDR")
      return IP_ADDR;
  else if (var == "IP_MASK")
      return IP_MASK;
  else if (var == "IP_GATE")
      return IP_GATE;
  else if (var == "IP_DNS")
      return IP_DNS;
  else if (var == "NTP_TZ")
      return NTP_TZ;
  else if (var == "BROW_TO")
      return BROW_TO;
  else if (var == "NTP_SRV")
      return NTP_SRV;
  else if (var == "MQTT_SRV")
      return MQTT_SRV;
  else if (var == "MQTT_PORT")
      return MQTT_PORT;
  else if (var == "MQTT_USER")
      return MQTT_USER;
  else if (var == "MQTT_PASS")
      return MQTT_PASS;
  else if (var == "MQTT_CLIENT")
      return MQTT_CLIENT;
  else if (var == "LOG_NAME")
      return LOG_NAME;
  else if (var == "LOG_PASS")
      return LOG_PASS;
  return String(); //Αν δεν το βρήκες γύρνα κενό string
}

//Βάζει μέσα σε δεδομένα του Chart.js για την δημιουργία διαγραμμάτων
//Δέχεται στήλη (η 1η είναι 0) και αν είναι string για Labels 
String ChartData(byte c, bool str)
{
  String buffer;
  String out, tmp;
  byte col;
  File file = LittleFS.open("/log.csv", "r");
  out = "";
  if (file) 
     {
      //Serial.println("Found"); //Debug
      while (file.available()) 
       {
        col = 0;
        buffer = file.readStringUntil('\n');
        if (buffer.indexOf(";") > 0)
           {
            tmp = "";
            for (unsigned short i = 0; i < buffer.length(); i++) 
                { 
                 if (buffer.substring(i, i+1) != ";" && i < buffer.length()-1)
                     tmp += buffer.substring(i, i+1);
                 else
                    {
                     if (col == c)
                        {
                         if (str == true)
                             out += "'" + tmp + "',";
                         else
                             out += tmp + ",";
                         break;
                        }
                     else
                        {
                         col++;
                         tmp = "";
                        }
                    }
                }
           }
       }
      out = out.substring(0, out.length()-1); //Αφαίρεσε το τελευταίο ,
      //Serial.println(out); //Debug
      file.close();
     }
  return out;
}

//Αποθηκεύει ρυθμίσεις της αρχικής οθόνης που είναι σε μορφή GET
void saveSettings(AsyncWebServerRequest *request, char *Filename)
     {
      File file = LittleFS.open(Filename, "w");
      if (!file) 
         {
          Serial.println("Error opening file for writing");
          return;
         }
      file.print("#Automatic created from script\n");
      int args = request->args();
      for (int i = 0; i < args; i++)
          {
           file.print(String(request->argName(i).c_str()) + "::" + String(request->arg(i).c_str()) + "\n");
          }
      file.close();
     }

//Ενέργειες διακοπτών βάσει των αιτήσεων GET
void setSwitch(AsyncWebServerRequest *request, String Switch, byte num, bool *pre_state)
{
 if (request->hasParam(Switch)) //Αν υπάρχει η παράμετρος
    {
     if (request->getParam(Switch)->value() == "On")
        {
         if (*pre_state == false)
            {
             //Serial.println(Switch + " is ON");
             setSwitchState(num, true);
             *pre_state = true;
            }
         else
            {
             //Serial.println(Switch + " is OFF");
             setSwitchState(num, false);
             *pre_state = false;
            }
        }
     else if (request->getParam(Switch)->value() == "Off")
        {
         if (*pre_state == true)
            {
             //Serial.println(Switch + " is OFF");
             setSwitchState(num, false);
             *pre_state = false;
            }
         else
            {
             //Serial.println(Switch + " is ON");
             setSwitchState(num, true);
             *pre_state = true;
            }
        }
    }
}

//Θέτει τον διακόπτη στο num σε κατάσταση ON ή OFF
void setSwitchState(byte num, bool state)
{
 switch (num)
   {
    case 1:
       (state)?((out1_on()),(SetSw1="on")):((out1_off()),(SetSw1="off"));
       break;
    case 2:
       (state)?((out2_on()),(SetSw2="on")):((out2_off()),(SetSw2="off"));
       break;
    case 3:
       (state)?((out3_on()),(SetSw3="on")):((out3_off()),(SetSw3="off"));
       break;    
   }
 //Αποθηκεύει την κατάσταση του διακόπτη σε περίπτωση που γίνει διακοπή να επανέρθει
 File file = LittleFS.open("/settings.txt", "w");
 if (!file) 
    {
     Serial.println(F("Error opening file for writing"));
     return;
    }
 file.print("#Automatic created from script\n");
 file.print("S1::" + SetSw1 + "\n");
 file.print("S2::" + SetSw2 + "\n");
 file.print("S3::" + SetSw3 + "\n");
 file.close();
}

//Κάθε 30 sec στείλε αίτημα σύνδεσης 
static void testcon(void* arg)
{
 test_client->connect(testhost, 80); //Στείλε
 if (conn) //Αν το conn έγινε true μέσα στην onConnect υπάρχει σύνδεση
    {
     conn = false; //Άλλαξέ το για την επόμενη προσπάθεια
     InternAvail = "true"; //Θέσε μεταβλητή που στέλνει στο web interface
     //Serial.println("Internet OK");
    }
 else //Αλλιώς δεν υπάρχει σύνδεση
    {
     InternAvail = "false";
     //Serial.println("Internet BAD");
    }
 //Κάνει έλεγχο για τον τρόπο αναβοσβήματος του status LED (μπλέ)
 if (SetSTA == "true") //Αν συνδέεται σαν client
    {
     if (WiFi.status() == WL_CONNECTED  && InternAvail == "true" && MQTT_State == "true")
         led_pat = 0; //Αναμμένο συνέχεια
     else if (WiFi.status() == WL_CONNECTED && InternAvail == "true" && MQTT_State == "false")
         led_pat = 0b0000011111; //Duty cycle 50%
     else if (WiFi.status() == WL_CONNECTED && InternAvail == "false" && MQTT_State == "false") 
         led_pat = 0b0101111111; //Δύο σύντομες αναλαμπές
     else if (WiFi.status() != WL_CONNECTED)
        {
         led_pat = 0b0111111111; //Μία σύντομη αναλαμπή
         char client_ssid[20];
         ST_SSID.toCharArray(client_ssid, 20);
         char client_password[20];
         ST_KEY.toCharArray(client_password, 20);
         WiFi.begin(client_ssid, client_password);
         Serial.println("Trying to reconnected as client to SSID: " + ST_SSID);
        }
    }
 else if (SetAP == "true") //Αν λειτουργεί σαν AP
     led_pat = 0b0000100001; //Δύο σύντομες διακοπές
}

static void testmqtt(void* arg)
{
 mqtt_state = 0;
 //Το αρχείο Log που βρίσκεται στην SPFFS Flash πρέπει να έχει μέχρι 720 γραμμές και μέγεθος μέχρι
 //32Kb. Αυτό γιατί το chartJS δεν υποστηρίζει περισσότερες τιμές. Γράφω 1 τιμή ανά ώρα δηλαδή 24 ανά ημέρα.
 //Η flash έχει δυνατότητα 100.000 εγγραφών, δηλαδή περίπου 11 χρόνια. 
}

//Αν έγινε σύνδεση με τον testhost
void onConnect(void* arg, AsyncClient* client) 
{
  //Serial.println("client has been connected");
  if (client->connected()) //Αν είναι συνδεμένο
     {
      conn = true; //Θέσε το conn ώστε να ανιχνευθεί στην testcon κατά την επόμενη προσπάθεια
      client->stop(); //Αποσύνδεση
     }
}

//Αν έγινε σύνδεση με τον updatehost
void onConnectUpd(void* arg, AsyncClient* client) 
{
  //Serial.println("client has been connected");
  if (client->connected()) //Αν είναι συνδεμένο
     {
      Serial.println("Connected to Host");
      chunks = 0;
      //Σημαντικό να είναι HTTP/1.0 και όχι 1.1
      String tmp = "GET " + Dl_Filename + " HTTP/1.0\r\nHost: " + update_host + "\r\nUser-Agent: IOT/ESP8266\r\nAccept-Encoding: gzip, deflate\r\n\r\n";
      client->write(tmp.c_str());
     }
}

void download(char* dl_filename)
{
 Dl_Filename = Dl_Path + String(dl_filename);
 FileName = "/upd_" + String(dl_filename);
 update_client->connect(update_host, 80); //Στείλε
 Serial.print("Trying ...");
 Serial.println(FileName);
 DLFileName += String(dl_filename) + "&lt;br&gt;";
}
