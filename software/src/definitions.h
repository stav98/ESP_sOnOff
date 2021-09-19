#define VERSION       "1.01"
#define ADC_PIN       A0
#define OUT1_PIN      15  //Ρελέ για 1o φως
#define OUT2_PIN      13  //Ρελέ για 2o φως
#define OUT3_PIN      12  //Ρελέ για 3o φως
#define IN1_PIN        5  //Αισθητήρας για 1o φως
#define IN2_PIN        4  //Αισθητήρας για 2o φως
#define IN3_PIN       14  //Αισθητήρας για 3o φως
#define BUILTIN_LED    2

#define out1_on()         {digitalWrite(OUT1_PIN, HIGH); delay(10);}  //Ανάβει 1ο φως
#define out1_off()        {digitalWrite(OUT1_PIN, LOW);  delay(10);}  //Σβήνει 1ο φως
#define out2_on()         {digitalWrite(OUT2_PIN, HIGH); delay(10);}  //Ανάβει 2ο φως
#define out2_off()        {digitalWrite(OUT2_PIN, LOW);  delay(10);}  //Σβήνει 2ο φως
#define out3_on()         {digitalWrite(OUT3_PIN, HIGH); delay(10);}  //Ανάβει 3ο φως
#define out3_off()        {digitalWrite(OUT3_PIN, LOW);  delay(10);}  //Σβήνει 3ο φως

#define PERIOD                1000 //Περίοδος μετρήσεων

//--------------------- Μεταβλητές σύνδεσης στον MQTT broker ---------------------------------------------
const char *mqtt_server;          //Όνομα του mqtt server π.χ. example.com
int mqtt_port;                    //Πόρτα π.χ. 1883
const char *mqttuser;             //"user"
const char *mqttpass;             //"password"
byte mqtt_state = 0;

//--------------------- Όνομα του Server για να κάνει έλεγχο σύνδεσης ------------------------------------
const char *testhost = "www.google.com";  //Η διεύθυνση για να κάνει έλεγχο σύνδεσης στο διαδίκτυο
const char *update_host = "users.school.com";
String Dl_Path = "/user/download/lights/";
String Dl_Filename; //Η πλήρης διαδρομή του αρχείου π.χ. /user/download/lights/test.txt δημιουργείται μέσα στην download() στο network.h
String FileName, Version; //Το όνομα του αρχείου στο SPIFFS
byte Dl_State = 0;
bool CanUpdate;

//---------------------------- Μεταβλητές αρχικής σελίδας ------------------------------------------------
String Switch1_status, Switch2_status, Switch3_status, Switch1_status_pre, Switch2_status_pre, Switch3_status_pre; //Οι τιμές αυτές διαβάζονται από τους αισθητήρες ρεύματος
String SetSw1, SetSw2, SetSw3; //Οι τιμές που αποθηκεύονται στο αρχείο για επαναφορά μετά από διακοπή ρεύματος
bool sw1_pre = false, sw2_pre = false, sw3_pre = false; 

//----------------------------- Μεταβλητές ρυθμίσεων λειτουργίας -----------------------------------------
String SetA1_LABL, SetA1_TOPIC, SetA1_ON, SetA1_OFF, SetA2_LABL, SetA2_TOPIC, SetA2_ON, SetA2_OFF, 
       SetA3_LABL, SetA3_TOPIC, SetA3_ON, SetA3_OFF;

//-------------------------- Μεταβλητές σύνδεσης μέσω WiFi -----------------------------------------------
String SetAP, AP_SSID, AP_CHANNEL, AP_ENCRYPT, AP_KEY, SetSTA, ST_SSID, ST_KEY, IP_STATIC, IP_ADDR, IP_MASK,
       IP_GATE, IP_DNS, NTP_TZ, BROW_TO, NTP_SRV, MQTT_SRV, MQTT_PORT, MQTT_USER, MQTT_PASS, MQTT_CLIENT, LOG_NAME, LOG_PASS;
String InternAvail = "false", MQTT_State = "false", DLFileName = "", Dl_OK = "false", S_Strength = "";
bool conn = false;

//----------------------------- Χρονιστές και άλλες μεταβλητές -------------------------------------------
unsigned long blink_per; //Ο χρονιστής για το LED
unsigned long tim; //Χρονιστής epoch αν δεν έχει σύνδεση διαδικτύου
unsigned long uptime; //Χρονιστής uptime για έλεγχο συνεχόμενης λειτουργίας
unsigned short led_pat = 0;
unsigned long lastChktime;
