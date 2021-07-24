
/*** Node Red ***
     MQTT IN Node           SWITCH Node           MQTT OUT Node
 --------------------     --------------      --------------------
 | room1/light1_pub |---->|            |----->| room1/light1_sub |
 --------------------     --------------      --------------------
Switch - Pass through msg if payload matches new state: όχι 
Switch - Indicator: Switch icon shows state of the input 
MQTT OUT Node: QOS - κενό, retain - κενό

**** Virtuino ***
Pub-topic: room1/light1_sub όχι retain
Sub-topic: room1/light1_pub για να διαβάζει εντολές από Sonoff.
Value Converter: off<-0, on<-1, off->0, on->1
Switches - Buttons
Value type: Number
Switch On: 1
Seitch Off: 0 
*/
#include <Arduino.h>
#include <PubSubClient.h>
#include "LittleFS.h"
#include "definitions.h"
#include "functions.h"
#include "network.h"
#include "buttons.h"

//Ορισμός Timer
static os_timer_t intervalTimer;
static os_timer_t mqttTimer;

byte time2next; //Μετρητής σε sec που μετράει αντίστροφα μέχρι το 0.

void setup() 
{
 pinMode(BUILTIN_LED, OUTPUT); //pin 2
 pinMode(OUT1_PIN, OUTPUT);  //Ρελέ για 1ο φως
 pinMode(OUT2_PIN, OUTPUT);  //Ρελέ για 2ο φως
 pinMode(OUT3_PIN, OUTPUT);  //Ρελέ για 3ο φως
 digitalWrite(BUILTIN_LED, HIGH); //Σβήσε το LED
 digitalWrite(OUT1_PIN, LOW);
 digitalWrite(OUT2_PIN, LOW);
 digitalWrite(OUT3_PIN, LOW);
 pinMode(IN1_PIN, INPUT); //Μετασχ. ρεύματος για έλεγχο της λάμπας 1
 pinMode(IN2_PIN, INPUT); //Μετασχ. ρεύματος για έλεγχο της λάμπας 2
 pinMode(IN3_PIN, INPUT); //Μετασχ. ρεύματος για έλεγχο της λάμπας 3
 Serial.begin(115200);
 Serial.println();
 Serial.println(F("--- ESP Lights Control (c)2021 by Stavros S. Fotoglou ---"));
 //Προετοιμασία συστήματος αρχείων 3 Mbyte
 if (!LittleFS.begin()) 
    {
     Serial.println(F("LittleFS initialisation failed!"));
     while (1) yield(); // Stay here twiddling thumbs waiting
    }
 Serial.println(F("\r\nLittleFS available!"));
 LittleFS.begin();
 //Αν ξεκίνησε το σύστημα αρχείων στην εσωτερική Flash 4Mbyte
 //Εμφάνισε ελεύθερο χώρο από το σύστημα SPIFFS
 FSInfo fs_info;
 LittleFS.info(fs_info);
 float fileTotalKB = (float)fs_info.totalBytes / 1024.0; 
 float fileUsedKB = (float)fs_info.usedBytes / 1024.0; 
 char tmp[40];
 sprintf(tmp, "Used %.1fKb of total %.1fKb", fileUsedKB, fileTotalKB);
 Serial.println(tmp);
 readSettings_net(); //Διάβασε ρυθμίσεις δικτύου από αρχείο
 //Ρουτίνα εξυπηρέτησης αν γίνει σύνδεση TCP με τον server π.χ. www.google.com 
 test_client->onConnect(&onConnect, test_client); //Στο network.h 
 //Ρουτίνα εξυπηρέτησης αν γίνει σύνδεση TCP με τον Update server π.χ. users.sch.gr 
 update_client->onConnect(&onConnectUpd, update_client); //Στο network.h 
 //Ρουτίνα εξυπηρέτησης αν υπάρχουν δεδομένα μετά από αίτημα ανάκτησης αρχείου στον Update Server π.χ. users.sch.gr 
 update_client->onData(&handleData, update_client); //Στο network.h 
 led_pat = 0b0111111111; //Αρχική κατάσταση
 initNetwork();  //Αρχικοποίηση δικτύου WiFi, IP, NTP και WebServer
 initBUTTONS();  //Αρχικοποίηση Push Button
 blink_per = millis();
 readSettings_wp(); //Διάβασε ρυθμίσεις λειτουργίας όπως ονόματα χώρων, mqtt topic, καταστάσεις του κάθε διακόπτη
 readSettings();
 //Αρχική σύνδεση στον mqtt broker
 reconnect();
 //Πόσο είναι το μέγεθος του αρχείου log σε bytes
 chkLogSize();
 //Κάθε 30 sec κάνε έλεγχο σύνδεσης
 os_timer_disarm(&intervalTimer); //Απενεργοποίηση Timer
 os_timer_setfn(&intervalTimer, &testcon, NULL); //Ορισμός ρουτίνας εξυπηρέτησης χωρίς ορίσματα στο network.h
 os_timer_arm(&intervalTimer, 30000, true);  //Ενεργοποίηση με επανάληψη κλήσης κάθε 30 sec

 //Κάθε 5 min κάνε έλεγχο σύνδεσης στον mqtt broker
 os_timer_disarm(&mqttTimer); //Απενεργοποίηση Timer
 os_timer_setfn(&mqttTimer, &testmqtt, NULL); //Ορισμός ρουτίνας εξυπηρέτησης χωρίς ορίσματα στο network.h
 os_timer_arm(&mqttTimer, 300000, true);  //Ενεργοποίηση με επανάληψη κλήσης ώστε να κάνει έλεγχο κάθε 5 λεπτά
 lastChktime = millis();
 //Αρχικοποίηση τιμών κατά το ξεκίνημα
 Switch1_status = Switch2_status = Switch3_status = "IsOff";
 Switch1_status_pre = Switch1_status;
 Switch2_status_pre = Switch2_status;
 Switch3_status_pre = Switch3_status;
 chk_SW_status(); //Έλεγχος των καταστάσεων των διακοπτών 
 //listFS(); //Εμφάνισε τα αρχεία όλου του FS
 uptime = 0;
}

void loop() 
{
 if (InternAvail == "true")
     timeClient.update(); //Όταν φτάσει η στιγμή ανανέωσε την ώρα από τον NTP Server
 butn_1.CheckBP(); //Έλεγξε αν πατήθηκε το κουμπί (εσωτερικό button)
 blink_led(BUILTIN_LED); //Αναβόσβησε το BuiltIn Led
 //===================== Κάθε 1 sec έλεγξε την κατάσταση των διακοπτών =====================================
 if ((millis() - lastChktime) >= PERIOD)
    {
     lastChktime = millis();
     tim += 1; //Ενημέρωσε το epoch time κατά 1 sec αν δεν λειτουργεί το NTP
     uptime += 1; //Ενημέρωσε το uptime κατά 1 sec
     chk_SW_status(); //Έλγξε κατάσταση φωτισμού από αισθητήρες ρεύματος και ενημέρωσε σελίδα και mqtt - functions.h
     //Serial.printf("Time: %02d:%02d:%02d\n", hour(tim)+2, minute(tim), second(tim)); //Debug Εμφάνισε ώρα από epoch local
     //Κάθε 5 sec
     if (time2next < 1) //Αν έφτασε ο χρόνος τότε
        {
         time2next = 5; //Φόρτωσε πάλι την τιμή για τον επόμενο κύκλο
         if (WiFi.status() == WL_CONNECTED)
             S_Strength = String(WiFi.RSSI()) + "dBm"; //Υπολόγισε ισχύ σήματος
         else
             S_Strength = "";
        }
     else
        {
         --time2next; //Κάθε 1 sec
        }
    }
 //===================================================================================================
 //Αν υπάρχει σύνδεση στο διαδίκτυο τότε
 if (InternAvail == "true")
    {
     if (!client.connected()) //Αν δεν έχει συνδεθεί στον mqtt broker
         reconnect();         //Προσπάθησε να κάνεις σύνδεση
     if (client.connected())  //Αν υπάρχει σύνδεση στον broker
         client.loop();       //Έλεγξε για μηνύματα
    }
 chk_Dl();
}
