// ESP32 WiFi <-> 3x UART Bridge
// by AlphaLima
// www.LK8000.com

// Disclaimer: Don't use  for life support systems
// or any other situations where system failure may affect
// user or environmental safety.

#include "config.h"
#include <esp_wifi.h>
#include <WiFi.h>
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

struct report_data_t {
    uint8_t version_;            /* version de la structure */
    uint8_t max_temp;           /* temperature maximum */
    uint16_t max_speed;         /* vitesse maximum */
    uint32_t time_idle;         /* temps pass=E9 a l'arret */
    uint32_t time_running[50];  /* temps pass=E9 en marche par vitesse */
    uint32_t temp_table[50];    /* temps de fonctionnement par degres */
    unsigned long long picks_counter;     /* nombre totale de duites */
    unsigned long long cycles[28][50];  /* nombre de cycles par lames et par tranche =
de vitesse */
} ;

report_data_t report;


#ifdef BLUETOOTH
#include <BluetoothSerial.h>
BluetoothSerial SerialBT; 
#endif // BLUETOOTH

#ifdef OTA_HANDLER  
#include <ArduinoOTA.h> 
#endif // OTA_HANDLER

HardwareSerial Serial_one(1);
HardwareSerial Serial_two(2);
HardwareSerial* COM[NUM_COM] = {&Serial, &Serial_one, &Serial_two};

#define MAX_NMEA_CLIENTS 4
#ifdef PROTOCOL_TCP
#include <WiFiClient.h>
WiFiServer server_0(SERIAL0_TCP_PORT);
WiFiServer server_1(SERIAL1_TCP_PORT);
WiFiServer server_2(SERIAL2_TCP_PORT);
WiFiServer *server[NUM_COM]={&server_0,&server_1,&server_2};
WiFiClient TCPClient[NUM_COM][MAX_NMEA_CLIENTS];
#endif


uint8_t buf1[NUM_COM][BUFFER_SIZE];
uint16_t i1[NUM_COM]={0,0,0};

uint8_t buf2[NUM_COM][BUFFER_SIZE];
uint16_t i2[NUM_COM]={0,0,0};

uint8_t BTbuf[BUFFER_SIZE];
uint16_t iBT =0;

uint8_t buf[BUFFER_SIZE];

void setup() {

  delay(500);
  
  COM[0]->begin(UART_BAUD0, SERIAL_PARAM0, SERIAL0_RXPIN, SERIAL0_TXPIN);
  COM[1]->begin(UART_BAUD1, SERIAL_PARAM1, SERIAL1_RXPIN, SERIAL1_TXPIN);
  COM[2]->begin(UART_BAUD2, SERIAL_PARAM2, SERIAL2_RXPIN, SERIAL2_TXPIN);
  
  if(debug) COM[DEBUG_COM]->println("\n\nRatiere WiFi serial bridge V1.00");
  #ifdef MODE_AP 
    if(debug) COM[DEBUG_COM]->println("Open ESP Access Point mode");
    // AP mode (phone connects directly to ESP) (no router)
  WiFi.mode(WIFI_AP);
   
  WiFi.softAP(ssid, pw); // configure ssid and password for softAP
  delay(2000); // VERY IMPORTANT
  WiFi.softAPConfig(ip, ip, netmask); // configure ip address for softAP

  #endif // MODE_AP


  #ifdef MODE_STA
    if(debug) COM[DEBUG_COM]->println("Open ESP Station mode");
    // STATION mode (ESP connects to router and gets an IP)
    // Assuming client is also connected to that router
    // from RoboRemo you must connect to the IP of the ESP
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pw);
    if(debug) COM[DEBUG_COM]->print("try to Connect to Wireless network: ");
    if(debug) COM[DEBUG_COM]->println(ssid);
    while (WiFi.status() != WL_CONNECTED) {   
      delay(500);
      if(debug) COM[DEBUG_COM]->print(".");
    }
    if(debug) COM[DEBUG_COM]->println("\nWiFi connected");
  
  #endif // MODE_STA
  
  #ifdef BLUETOOTH
    if(debug) COM[DEBUG_COM]->println("Open Bluetooth Server");  
    SerialBT.begin(ssid); //Bluetooth device name
    #endif // BLUETOOTH
  
  #ifdef OTA_HANDLER  
    ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        if(debug) COM[DEBUG_COM]->println("Start updating " + type);
        })
      .onEnd([]() {
        if(debug) COM[DEBUG_COM]->println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        if(debug) COM[DEBUG_COM]->printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        if(debug) COM[DEBUG_COM]->printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) COM[DEBUG_COM]->println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) COM[DEBUG_COM]->println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) COM[DEBUG_COM]->println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) COM[DEBUG_COM]->println("Receive Failed");
        else if (error == OTA_END_ERROR) COM[DEBUG_COM]->println("End Failed");
      });
      // if DNSServer is started with "*" for domain name, it will reply with
      // provided IP to all DNS request

    ArduinoOTA.begin();
  #endif // OTA_HANDLER    

  #ifdef PROTOCOL_TCP
    
    COM[0]->println("Starting TCP Server 1");  
    if(debug) COM[DEBUG_COM]->println("Starting TCP Server 1");  
    server[0]->begin(); // start TCP server 
    server[0]->setNoDelay(true);
     
    COM[1]->println("Starting TCP Server 2");
    if(debug) COM[DEBUG_COM]->println("Starting TCP Server 2");  
    server[1]->begin(); // start TCP server 
    server[1]->setNoDelay(true);
    
    COM[2]->println("Starting TCP Server 3");
    if(debug) COM[DEBUG_COM]->println("Starting TCP Server 3");  
    server[2]->begin(); // start TCP server   
    server[2]->setNoDelay(true);
    
  #endif
  
  esp_err_t esp_wifi_set_max_tx_power(50);  //lower WiFi Power
}

/*-------------------------------------------------------------------------=
----
* wait_ack --
*--------------------------------------------------------------------------=
---
*/
static int wait_ack (unsigned int delay_)
{
    unsigned long init;
    char c;
    init = millis();

    while (1) {
        if (COM[RATIERE_COM]->available()) {
            c = COM[RATIERE_COM]->read();
            if (c == 0x6) {
                return 0;
            }
        }
        else {
            delayMicroseconds(100);
            if ((millis() - init) >= delay_) {
                break;
            }
        }
    }

    if (debug) COM[DEBUG_COM]->println ("ack timeout\n");
    return -1;
}

/*-------------------------------------------------------------------------=
----
* read_data --
*--------------------------------------------------------------------------=
---
*/
static int read_data (unsigned char *buf, unsigned int sz, unsigned int timeout) {
    unsigned long init;
    unsigned long i=0;

  init = millis();

  while (sz) {
    while (COM[RATIERE_COM]->available()) {
         *buf=COM[RATIERE_COM]->read();
         sz -= 1;
         i+=1;
         buf += 1;
         }
     delayMicroseconds(10);
     if ((millis() - init) >= timeout) break;
     }
  if (i==0) {
        return(-1);
     }
  return(i);
  }


/*-------------------------------------------------------------------------=
----
* display_buf --
*--------------------------------------------------------------------------=
---
*/
static void display_buf (unsigned char *buf, int sz)
{
  int i;

    for (i=0; i<sz; i++) {
        COM[DEBUG_COM]->print(buf[i],HEX);
        COM[DEBUG_COM]->print(",");

    }
    COM[DEBUG_COM]->println();
}



/*-------------------------------------------------------------------------=
----
* get_report --
*--------------------------------------------------------------------------=
---
*/

static int get_report (unsigned short *speed_,unsigned char *temp, unsigned char *nb_frames, report_data_t *rep)
  {
    unsigned int cpy;
    const char get_report_cmd[] = "\007RECREP\r";
    unsigned int data_sz;
    char *ptr;

 

    COM[RATIERE_COM]->begin(4800,SERIAL_8N2);
    
    if (COM[RATIERE_COM]->write(get_report_cmd, sizeof (get_report_cmd) - 1) != sizeof (get_report_cmd) - 1) {
        if (debug) COM[DEBUG_COM]->println ("Write command fail");
        //COM[1]->end();
        return -1;
    }
    if (debug) COM[DEBUG_COM]->println ("Write done");

    if (wait_ack (80)) {
        if (debug) COM[DEBUG_COM]->println ("ACK1 not received");
        //COM[1]->end();
        return -1;
    }
    
    // changement de vitesse
    if (debug) COM[DEBUG_COM]->println("Change BR to 115200");
    COM[RATIERE_COM]->begin(115200,SERIAL_8N2);
    delayMicroseconds(10000);

    // purge du buffer
    while (COM[RATIERE_COM]->available()) COM[RATIERE_COM]->read();

    // envoi de ack 
    if (COM[RATIERE_COM]->write(0x6) != 1) {
        if (debug) COM[DEBUG_COM]->println ("write ack fail");
        //COM[1]->end();
        return -1;
    }

    // attend ack et block A 
    if ((read_data(buf, 12, 70)!=12) || (buf[0] != 0x6) ||
            (buf[1] != 'A') || (buf[2] != 9) || (buf[3] != 2) ) {
        if (debug) COM[DEBUG_COM]->println ("ACK2 not received");
        //COM[1]->end();
        return -1;
        }
    
    /*if (debug) { 
      COM[DEBUG_COM]->println("First Block received");
      display_buf (buf, 11);
      }*/
      
    data_sz = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 8);
    *speed_ = buf[8] | (buf[9] << 8);
    *temp = buf[10];
    *nb_frames = buf[11];
    
    // if (debug) COM[DEBUG_COM]->printf ("data_sz = %u\n",data_sz);
    
    // envoi de ack 
    if (COM[RATIERE_COM]->write (0x6) != 1) {
        if (debug) COM[DEBUG_COM]->println ("write ack fail");
        //Serial1.end();
        return -1;
        }
    ptr = (char *)rep;
    cpy = 0;
    
    while (data_sz > 0) {
        unsigned int sz;
   
        //if (debug) COM[DEBUG_COM]->printf ("data_sz = %u\n",data_sz);
        
        sz=read_data(buf,sizeof buf,55);
 
        if ( sz == -1 ) {
            //COM[1]->end();
            return -1;
        }
        //if (debug) COM[DEBUG_COM]->printf ("sz = %u\n",sz);
        
        if (cpy + sz-2 <= sizeof (report_data_t)) {
            memcpy (ptr, &buf[2], sz-2);
            ptr += sz-2;
            cpy += sz-2;
        }

        // envoi de ack 
        if (COM[RATIERE_COM]->write (0x6) != 1) {
            if (debug) COM[DEBUG_COM]->println ("write ack fail");
            //COM[1]->end();
            return -1;
        }
        data_sz -= sz-2; // -2 for header
    }

    delayMicroseconds(1000);
     //COM[RATIERE_COM]->end();
    return 0;
}



void send_report( WiFiClient client_, unsigned short speed_,unsigned char temp, unsigned char nb_frames, report_data_t *rep){

    unsigned int i,j; 
    
    if (debug) COM[DEBUG_COM]->printf ("send report\n");
        
    const int capacity1 = JSON_OBJECT_SIZE(10);
    StaticJsonDocument<capacity1> jDoc1;
    
    jDoc1["Version"]= rep->version_;
    jDoc1["Temperature"]= temp;
    jDoc1["TemperatureMax"]= rep->max_temp;
    jDoc1["Speed"]= speed_;
    jDoc1["SpeedMax"]= rep->max_speed;
    jDoc1["Frame"]= nb_frames;
    jDoc1["idle"]= rep->time_idle;
    jDoc1["TotalCycle"]= rep->picks_counter;

    serializeJson(jDoc1,client_);
    client_.println();
    
    const int capacity2 = JSON_OBJECT_SIZE(29);
    StaticJsonDocument<capacity2> jDoc2;
     
    JsonArray speed_time = jDoc2.createNestedArray("SpeedTable");
    for (i=0; i<50; i++) {
      speed_time.add(rep->time_running[i]);
      }
    serializeJson(jDoc2,client_);
    client_.println();
    
    const int capacity3 = JSON_OBJECT_SIZE(29);
    StaticJsonDocument<capacity3> jDoc3;
    /*jDoc3["Type"]= "temp_table";*/
    JsonArray temp_data = jDoc3.createNestedArray("TempTable");
    for (i=0; i<50; i++) {
      temp_data.add(rep->temp_table[i]);
      }    
    serializeJson(jDoc3,client_);
    client_.println();
    
    
    const int capacity4 = JSON_OBJECT_SIZE(28);
    StaticJsonDocument<capacity4> jDoc4;
    
    JsonArray lame = jDoc4.createNestedArray("Lame"); 

    unsigned long long total = 0;
    
    for (i=0; i<28; i++) {
      total=0;
      /*JsonArray cycle = lame.createNestedArray("Cycles");*/
      for (j=0; j<50; j++) {  
        total += rep->cycles[i][j];
        /*cycle.add(rep->cycles[i][j]);*/
        }
      lame.add(total);  
    }
    
    serializeJson(jDoc4,client_);
    client_.println();
}



// Main
void loop() 
{  
  unsigned short speed_;
  unsigned char temp, nb_frames;
  int ret=-1;
   
#ifdef OTA_HANDLER  
  ArduinoOTA.handle();
#endif // OTA_HANDLER
  
#ifdef BLUETOOTH
  // receive from Bluetooth:
  if(SerialBT.hasClient()) 
  {
    while(SerialBT.available())
    {
      BTbuf[iBT] = SerialBT.read(); // read char from client (LK8000 app)
      if(iBT <bufferSize-1) iBT++;
    }          
    for(int num= 0; num < NUM_COM ; num++)
      COM[num]->write(BTbuf,iBT); // now send to UART(num):          
    iBT = 0;
  }  
#endif // BLUETOOTH

  if (debug) COM[DEBUG_COM]->print("\n******** loop ********\n");
  // get report from COM[1]


#ifdef PROTOCOL_TCP
  // Look for new client 
  for(int num= 0; num < NUM_COM ; num++){
    if (server[num]->hasClient()){
      if (debug) COM[DEBUG_COM]->printf("new client detected for server %u\n",num);      
      for(byte i = 0; i < MAX_NMEA_CLIENTS; i++){
        //find free/disconnected spot
        if (!TCPClient[num][i] || !TCPClient[num][i].connected()){
          if(TCPClient[num][i]) TCPClient[num][i].stop();
          TCPClient[num][i] = server[num]->available();
          if(debug) {
            COM[DEBUG_COM]->printf("New client nb %u for COM %u \n",i,num); 
            }
          continue;
        }
      }
      //no free/disconnected spot so reject
      WiFiClient TmpserverClient = server[num]->available();
      TmpserverClient.stop();
      if (debug) COM[DEBUG_COM]->print("Sorry no room for new client\n");      
    }
  }
#endif

   
  ret = get_report(&speed_, &temp, &nb_frames, &report);
  if (ret!=0) {
    if (debug) COM[DEBUG_COM]->print ("get_report failed\n");
    //delay(3000);
    }
    



  /* send buffer to client
  for(int num= 0; num < NUM_COM ; num++) {
    if(COM[num]!= NULL) {
      // from client to uart
      for(byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {               
        if(TCPClient[num][cln]) {
          while(TCPClient[num][cln].available()){
            buf1[num][i1[num]] = TCPClient[num][cln].read(); // read char from client (LK8000 app)
            if(i1[num]<BUFFER_SIZE-1) i1[num]++;
            } 

          COM[num]->write(buf1[num], i1[num]); // now send to UART(num):
          i1[num] = 0;
        }
      }
    }
  } 
  */ 
  // From uart to client
      
  if(ret == 0) {
     
     /// now send to report to wifi client:
     for(byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++){   
        if(TCPClient[2][cln]) send_report(TCPClient[2][cln],speed_,temp,nb_frames,&report);
     }
  }
        
#ifdef BLUETOOTH        
        // now send to Bluetooth:
        if(SerialBT.hasClient()) SerialBT.write(buf2[num], i2[num]);               
#endif //BLUETOOTH 
        i2[2] = 0;
        
  delay(2000);  
      
}
