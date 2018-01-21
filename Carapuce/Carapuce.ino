#include <SPI.h>
#include <Servo.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <WiFiST.h>
#include <PubSubClient.h>
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "lib_NDEF_URI.h"
#include "lib_NDEF_SMS.h"
#include "lib_NDEF_Text.h"
#include "lib_NDEF_Email.h"
#include "lib_NDEF_Geo.h"
#include "lib_95HFConfigManager.h"
#include "miscellaneous.h"
#include "lib_95HFConfigManager.h"
#include "lib_wrapper.h"
#include "lib_NDEF_URI.h"
#include "drv_spi.h"

#define HIGH_ACCURACY

//NFC CONFIG
#define BULK_MAX_PACKET_SIZE            0x00000040

#define PICC_TYPEA_ACConfigA            0x27
#define PICC_TYPEB_ARConfigD            0x0E
#define PICC_TYPEB_ACConfigA            0x17
#define PICC_TYPEF_ACConfigA            0x17
uint8_t TT2Tag[NFCT2_MAX_TAGMEMORY];
sURI_Info url; 
extern uint8_t NDEF_Buffer []; 
extern DeviceMode_t devicemode;
sRecordInfo_uri RecordStruct;
int8_t TagType = TRACK_NOTHING;
bool TagDetected = false;
bool terminal_msg_flag = false ;
uint8_t status = ERRORCODE_GENERIC;

//WIFI CONFIG
SPIClass SPI_3(PC12, PC11, PC10);
WiFiClass WiFi(&SPI_3, PE0, PE1, PE8, PB13);
VL53L0X sensor;
char ssid[] = "BarbaHotspot";       //  SSID
char pass[] = "1234abcd";    // PASSWORD
int keyIndex = 0;                  // KEY

int Wifistatus = WL_IDLE_STATUS;

//MQTT SERVER
char server[] = "24hducode.spc5studio.com";    // name address for Google (using DNS)

//WIFI CLIENT
WiFiClient Wificlient;

//GLOBAL VARIABLES
String teamCode = "12d13";
String password = "U7202Z96";
char arene[125];
char part1[125];
char part2[125];
char part3[125];
Servo Lservo;
Servo Rservo;
int lineL = D1;
int lineR = D0;
int pinmotorL = D6;
int pinmotorR = D5;
int l = 10;
int motorL = 90;
int motorR = 90;
int buzzer = A5;
int attente = 100;

//MQTT RECEPTION VARIABLES
bool isMsgReceived = false;
char * msgReceived;


//MQTT RECEPTION FUNCTION
void callback(char* topic, byte* payload, unsigned int length) {
  
  Serial.println("Call");
  char msg[length];
  for (int i=0;i<length;i++) {
    msg[i] =(char)payload[i];
    Serial.print((char)payload[i]);
  }
  strcpy(msgReceived, msg);
  isMsgReceived = true;
}

//MQTT CLIENT
PubSubClient client(server, 1883, callback, Wificlient);

//NFC SETUP FUNCTION
void NFCSetup(){
  ConfigManager_HWInit();
  terminal_msg_flag = true;
}

//WIFI CONNECTION FUNCTION
void wifiConnect() {
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi module not present");
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  Serial.print("Firwmare version : ");
  Serial.println(fv);
  if (fv != "C3.5.2.3.BETA9")
  {
    Serial.println("Please upgrade the firmware");
  }
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(2000);
  }
  Serial.println("Connected to wifi");
}

//MQTT CONNECTION FUNCTION
void MQTT_Connect(String teamCode, String psw){
  char pass[psw.length()-1];
  char pass2[psw.length()-1];
  psw.toCharArray(pass,psw.length()+1);
  Serial.println("Starting connection to server...");
  //Serial.println(psw.length());
  //Serial.println(psw.length()-1);
  Serial.println(teamCode);
  /*for (int i = 0; i < psw.length()-1; i++){
    pass2[i] = pass[i];
  }*/
  Serial.println(psw);
  Serial.println(pass);
  while (!client.connect("teamD","Carapuce",pass)){
    Serial.println("MQTT connection failed");
  }
    String str1 = "24hcode/teamD/";
    String str2 = "/broker2device";
    str1.concat(teamCode);
    str1.concat(str2);
    Serial.println(str1);
    Serial.println(str1.length());
    char channel[str1.length()+1];
    str1.toCharArray(channel, str1.length()+1);
    Serial.println(channel);
    /*for (int i = 0; i < str1.length(); i++){
      Serial.println(channel[i]);
    }*/
    while(!client.subscribe(channel));
    Serial.println("Suscribed to channel");
}

//MQTT SEND MESSAGE FUNCTION
void MQTT_SendMessage(String teamCode, String msg) {
  String str1 = "24hcode/teamD/";
  String str2 = "/device2broker";
  str1.concat(teamCode);
  str1.concat(str2);
  Serial.println(str1);
  Serial.println(str1.length());
  char channel[str1.length()+1];
  str1.toCharArray(channel, str1.length()+1);
  Serial.println(channel);
  /*for (int i = 0; i < str1.length(); i++){
    Serial.println(channel[i]);
  }*/
  char message[msg.length()];
  msg.toCharArray(message, msg.length()+1);
  /*for (int i = 0; i < msg.length(); i++){
    Serial.println(message[i]);
  }*/
  if (WiFi.status() != WL_CONNECTED) {
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print("Attempting to connect to SSID: ");
      Serial.println(ssid);
      status = WiFi.begin(ssid, pass);
      delay(2000);
    }
    MQTT_Connect(teamCode, password);
  }
  Serial.println("'"+String(message)+"'");
  while (!client.publish(channel,message)) {
   Serial.println("MQTT message fail");
  }
   Serial.println("MQTT message published");
}

//MQTT GET MESSAGE CONNECTION
char * MQTT_GetMessage() {
  while(!isMsgReceived);
  isMsgReceived = false;
  return msgReceived;
}

//NFC CHECK TAG FUNCTION
bool NFCCheckTag(){
  devicemode = PCD;
  if (ConfigManager_TagHunting(TRACK_ALL) == TRACK_NFCTYPE2) return true;
  return false;
}

//NFC GET TAG MESSAGE FUNCTION
void NFCGetMsg() {
  devicemode = PCD;
  TagType = ConfigManager_TagHunting(TRACK_ALL);
  status = PCDNFCT2_ReadNDEF();
  memset(url.Information,'\0',400);
  status = ERRORCODE_GENERIC;
  memset(NDEF_Buffer,'\0',20); /* Avoid printing useless characters */
  status = NDEF_IdentifyNDEF( &RecordStruct, NDEF_Buffer);
  if(status == RESULTOK && RecordStruct.TypeLength != 0)
  {
    if (NDEF_ReadURI(&RecordStruct, &url)==RESULTOK) /*---if URI read passed---*/
    {
      int i = 0, j = 0, n = 0;
      char *p;
      *p = '\n';
      while (String(url.URI_Message).substring(i, i + 1) != p) {
        if (String(url.URI_Message).substring(i, i + 1) == ":") {
          if (n == 0) String(url.URI_Message).substring(j, i).toCharArray(arene, 125);
          //else if (n == 1 && arene == "A1") String(url.URI_Message).substring(j, i).toCharArray(password, 25);
          else if (n == 1) String(url.URI_Message).substring(j, i).toCharArray(part1, 125);
          else if (n == 2) String(url.URI_Message).substring(j, i).toCharArray(part2, 125);
          n++;
          j = i + 1;
        }
        i++;
      }
      //if (arene == "A1") String(url.URI_Message).substring(j).toCharArray(teamCode, 25);
      /*else*/ String(url.URI_Message).substring(j).toCharArray(part3, 125);
    }
  }
}

//SERVO FUNCTIONS

void allumer_gauche()
{
  motorL = 99;
  Lservo.write(motorL);
  
}

void allumer_droite()
{
  motorR =    78;
  Rservo.write(motorR);
}



void arriere_gauche()
{
  motorL =    83;
  Lservo.write(motorR);

}

void arriere_droite()
{
  motorR =    98;
  Rservo.write(motorR);
}
void arret_gauche()
{
  motorL =    91;
  Rservo.write(motorL);
}
void arret_droite()
{
  motorR =    89;
  Rservo.write(motorR);
}

void reculer_droite()
{
  motorR =    95;
  Rservo.write(motorR);
}
//SETUP FUNCTION

void setup() {
  Serial.begin(115200);
  Wire.begin();

  sensor.init();
  sensor.setTimeout(500);
  NFCSetup();
  wifiConnect();
  pinMode(lineL,INPUT);
  pinMode(lineR,INPUT);
  pinMode(motorL,OUTPUT);
  pinMode(motorR,OUTPUT);
  pinMode(buzzer, OUTPUT);
  Lservo.attach(pinmotorL,900,2100);
  Rservo.attach(pinmotorR,900,2100);

  #if defined HIGH_SPEED
    // reduce timing budget to 20 ms (default is about 33 ms)
    sensor.setMeasurementTimingBudget(20000);
  #elif defined HIGH_ACCURACY
    // increase timing budget to 200 ms
    sensor.setMeasurementTimingBudget(1000000);
  #endif
  MQTT_Connect(teamCode, password);
}


//MUSIC

void note(int freq,int temps)
{
  tone(buzzer,freq);
  delay(temps);
  noTone(buzzer);
}

String playMusic(char *musique){
  int i=0;
  int longueur = strlen(musique);
  while(i<longueur-1){
    Serial.print(musique[i]);
    if(musique[i+1] == '#'){
      Serial.print('#');
      jouerNoteDiese(musique[i]);
      i++;
    }
    else{
      jouerNote(musique[i]);
    }
    i++;
  }
}

void jouerNote(char Note){
  if(Note == 'B'){
    note(494,attente);
  }
  else if(Note == 'A'){
    note(440,attente);
  }
  else if(Note == 'G'){
    note(392,attente);
  }
  else if(Note == 'E'){
    note(330,attente);
  }
  else if(Note == 'D'){
    note(293,attente);
  }
  else if(Note == 'C'){
    note(261,attente);
  }
  else if(Note == 'F'){
    note(349,attente);
  }
}

void jouerNoteDiese(char Note){
  if(Note == 'C'){
    note(277,attente);
  }
  else if(Note == 'A'){
    note(466,attente);
  }
  else if(Note == 'G'){
    note(415,attente);
  }
  else if(Note == 'F'){
    note(370,attente);
  }
  else if(Note == 'D'){
    note(311,attente);
  }
}
// Epreuve 5 César
String decryptCesar(char *message)
{
    int longueur = strlen(message);
    char alphabet[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int indice;
    int i,j=0;
    int cle = 18;
    char message_decrypt[strlen(message)]; 
    for(i=0;i<longueur;i++){
        for(j=0;j<26;j++){
            if(message[i] == alphabet[j]){

                indice = j;
                indice -= cle;
                if(indice < 0){
                    indice += 26;
                }
                message_decrypt[i] = alphabet[indice];
            }
        }
    }
    /*for(i=0;i<longueur;i++){
        Serial.println(message_decrypt[i]);
    }*/
    String str = String(message_decrypt);
    Serial.println(str.substring(0,strlen(message)));
    Serial.println(str.length());
    return String(str.substring(0,strlen(message)));
    //return message_decrypt;
}

String exemple7(String message)
{
   // put your main code here, to run repeatedly:
 
  int i;
  int longueurMessage = message.length() ;
  String listeNote;
  for(i = 0;i<longueurMessage;i++)
  {
    switch(message[i])
    {
      case 'A' :
        listeNote = listeNote + "FF";
        break;
      case 'B' :
        listeNote = listeNote + "AA";
        break;
      case 'C' :
        listeNote = listeNote + "GG";
        break;
      case 'D' :
        listeNote = listeNote + "CCC";
        break;      
      case 'E' :
        listeNote = listeNote + "B";
        break;
      case 'F' :
        listeNote = listeNote + "EEE";
        break;
      case 'G' :
        listeNote = listeNote + "GGG";
        break;
      case 'H' :
        listeNote = listeNote + "BBB";
        break;
      case 'I' :
        listeNote = listeNote + "EE";
        break;
      case 'J' :
        listeNote = listeNote + "DDD";
        break;
      case 'K' :
        listeNote = listeNote + "G";
        break;
      case 'L' :
        listeNote = listeNote + "DD";
        break;
      case 'M' :
        listeNote = listeNote + "D";
        break;
      case 'N' :
        listeNote = listeNote + "F";
        break;
      case 'O' :
        listeNote = listeNote + "E";
        break;
      case 'P' :
        listeNote = listeNote + "C";
        break;
      case 'Q' :
        listeNote = listeNote + "FFF";
        break;
      case 'R' :
        listeNote = listeNote + "BB";
        break;
      case 'S' :
        listeNote = listeNote + "A";
        break;
      case 'T' :
        listeNote = listeNote + "CC";
        break;
      case 'U' :
        listeNote = listeNote + "AAA";
        break;
      case 'V' :
        listeNote = listeNote + "DDD";
        break;
      case 'W' :
        listeNote = listeNote + "CCCC";
        break;
      case 'X' :
        listeNote = listeNote + "EEEE";
        break;
      case 'Y' :
        listeNote = listeNote + "GGGG";
        break;
      default :
        listeNote = listeNote + "BBBB";
        break;
     
      
       
    }
  }
  return listeNote;
}

void repos(int nombre)
{ 
  int caresses = 0;
  while(caresses < nombre)
  {

    if(sensor.readRangeSingleMillimeters()== 0000)
     {
      caresses++;
      while(!sensor.readRangeSingleMillimeters());
     }
    if (sensor.timeoutOccurred()) { Serial.print(" TIMEOUT"); }
    Serial.println();
    delay(500);
  }
  Serial.print("RonRon");
  delay(5000);
}



//LOOP FUNCTION

void loop() {
  
  int i =0;
     int NbTout = 4;
  while(NbTout >0)
  {
  arriere_droite();
  allumer_gauche();
    if(digitalRead(lineL))
    {
      while(digitalRead(lineL));
      i++;
      Serial.print(i);
      Serial.print("\n");
    }
  }
  if(NFCCheckTag()){
    arret_gauche();
    arret_droite();
    NFCGetMsg();
    Serial.println("TAG NFC : "+(String)arene+" / "+(String)part1+" / "+(String)part2+"");
    if ((String) arene == (String)"A1") {
      /*teamCode = String(part2);
      password = String(part1);
      Serial.println("teamCode : "+teamCode+" / "+password);
      MQTT_Connect(teamCode, password);*/
      MQTT_SendMessage(teamCode, (String)arene+(String)":Hello 24h du code!");
      //Serial.println(MQTT_GetMessage());
    } else if ((String) arene == (String) "A2") {
      MQTT_SendMessage(teamCode, (String)arene+(String)":"+(String)part2);
      //Serial.println(MQTT_GetMessage());
    } else if ((String) arene == (String) "A3a") {
      //for (int i = 0; i < String(part2).length(); i++) Serial.println(part2[i]);
      playMusic(part2);
      MQTT_SendMessage(teamCode, (String)arene+(String)":"+(String)part3);
      //Serial.println(MQTT_GetMessage());
    } else if ((String) arene == (String) "A4") {
      //for (int i = 0; i < String(part2).length(); i++) Serial.println(part2[i]);
      MQTT_SendMessage(teamCode, (String)arene+(String)":"+(String)part2);
      //Serial.println(MQTT_GetMessage());
    } else if ((String) arene == (String) "A5a") {
      //for (int i = 0; i < String(part2).length(); i++) Serial.println(part2[i]);
      MQTT_SendMessage(teamCode, (String)arene+(String)":"+(String)decryptCesar(part3));
      //Serial.println(MQTT_GetMessage());
    } else if ((String) arene == (String) "A6") {
      //for (int i = 0; i < String(part2).length(); i++) Serial.println(part2[i]);
      repos(String(part2).toInt());
      MQTT_SendMessage(teamCode, (String)arene+(String)":"+(String)part2[0]);
      //Serial.println(MQTT_GetMessage());
    } else if ((String) arene == (String) "A7a") {
      //for (int i = 0; i < String(part2).length(); i++) Serial.println(part2[i]);
      String newMusic = exemple7(part2);
      Serial.println(newMusic);
      char newMusicArray[newMusic.length()-1];
      newMusic.toCharArray(newMusicArray, newMusic.length()-1);
      playMusic(newMusicArray);
      MQTT_SendMessage(teamCode, (String)arene+(String)":"+newMusic);
      //Serial.println(MQTT_GetMessage());
    }  else if ((String) arene == (String) "A9") {
      //for (int i = 0; i < String(part2).length(); i++) Serial.println(part2[i]);
      MQTT_SendMessage(teamCode, (String)arene+(String)":"+(String)decryptCesar(part2));
      //Serial.println(MQTT_GetMessage());
    } else if ((String) arene == (String) "A11") {
      //for (int i = 0; i < String(part2).length(); i++) Serial.println(part2[i]);
      MQTT_SendMessage(teamCode, (String)arene+(String)":"+(String)"Victory");
      //Serial.println(MQTT_GetMessage());
    }
    client.loop();
  }
}
