// ESP32 Ratiere WiFi <-> WiFi cloud Gateway 


// Disclaimer: Don't use  for life support systems
// or any other situations where system failure may affect
// user or environmental safety.

#include "config.h"
//#include <esp_wifi.h>
#include <WiFi.h>
#include <HTTPClient.h>

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

HardwareSerial Serial_one(1);
HardwareSerial Serial_two(2);
HardwareSerial* COM[NUM_COM] = {&Serial, &Serial_one, &Serial_two};

uint8_t buf[BUFFER_SIZE];






void setup() {
  
  delay(500);
  
  COM[0]->begin(UART_BAUD0, SERIAL_PARAM0, SERIAL0_RXPIN, SERIAL0_TXPIN);
  COM[1]->begin(UART_BAUD1, SERIAL_PARAM1, SERIAL1_RXPIN, SERIAL1_TXPIN);
  COM[2]->begin(UART_BAUD2, SERIAL_PARAM2, SERIAL2_RXPIN, SERIAL2_TXPIN);
  
  if(debug) COM[DEBUG_COM]->println("\n\nRatiere WiFi serial bridge V2.00");

  #ifdef MODE_STA
    if(debug) COM[DEBUG_COM]->println("Open ESP Station mode");
    // STATION mode (ESP connects to router and gets an IP)
    // Assuming client is also connected to that router
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
  

  
//  esp_err_t esp_wifi_set_max_tx_power(50);  //lower WiFi Power

 
  
}




/*
* -------------------------------------------------------------------------=
* wait_ack from Ratiere
*--------------------------------------------------------------------------=
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

/*
*--------------------------------------------------------------------------=
* read_data: Read a buffer from Ratiere
*--------------------------------------------------------------------------=
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
* display_buf: Print buffer content on serial
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



/*
*--------------------------------------------------------------------------=
* get_report: get full report from a Ratiere
*--------------------------------------------------------------------------=
*/

static int get_report (unsigned short *speed_,unsigned char *temp, unsigned char *nb_frames, report_data_t *rep)
  {
    unsigned int cpy;
    const char get_report_cmd[] = "\007RECREP\r";
     int data_sz;
    char *ptr;

 

    COM[RATIERE_COM]->begin(4800,SERIAL_8N2);
    
    if (COM[RATIERE_COM]->write(get_report_cmd, sizeof (get_report_cmd) - 1) != sizeof (get_report_cmd) - 1) {
        if (debug) COM[DEBUG_COM]->println ("Write command fail");
        return -1;
    }
    if (debug) COM[DEBUG_COM]->println ("Write done");

    if (wait_ack(200)==-1) {
        if (debug) COM[DEBUG_COM]->println ("ACK1 not received");
        return -1;
    }
    
    // changement de vitesse
    if (debug) COM[DEBUG_COM]->println("Change BR to 115200");
    COM[RATIERE_COM]->begin(115200,SERIAL_8N2);
    delayMicroseconds(20000);

    // purge du buffer
    while (COM[RATIERE_COM]->available()) COM[RATIERE_COM]->read();

    // envoi de ack 
    if (COM[RATIERE_COM]->write(0x6) != 1) {
        if (debug) COM[DEBUG_COM]->println ("write ack fail");
        return -1;
    }

    // attend ack et block A 
    if ((read_data(buf, 12, 200)!=12) || (buf[0] != 0x6) ||
            (buf[1] != 'A') || (buf[2] != 9) || (buf[3] != 2) ) {
        if (debug) COM[DEBUG_COM]->println ("ACK2 not received");
        return -1;
        }
    
      
    data_sz = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 8);
    *speed_ = buf[8] | (buf[9] << 8);
    *temp = buf[10];
    *nb_frames = buf[11];
    
    // if (debug) COM[DEBUG_COM]->printf ("data_sz = %u\n",data_sz);
    
    // envoi de ack 
    if (COM[RATIERE_COM]->write (0x6) != 1) {
        if (debug) COM[DEBUG_COM]->println ("write ack fail");
        return -1;
        }
    
    ptr = (char *)rep;
    cpy = 0;
    
    while (data_sz > 0) {
        unsigned int sz;
   
        sz=read_data(buf,sizeof buf,55); 
        if ( sz == -1 ) {
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
    return 0;
  }


/*
*--------------------------------------------------------------------------=
* send_report: send full report from a Ratiere to HTTP 
*--------------------------------------------------------------------------=
*/
void send_report(unsigned short speed_,unsigned char temp, unsigned char nb_frames, report_data_t *rep){

    unsigned int i,j; 
    char putData[2048];

    char url[200] = ("http://");
    strcat(url,GATEWAY_SERVER);
    strcat(url,":");
    strcat(url,GATEWAY_PORT);
    strcat(url,PATH);

    if (debug) COM[DEBUG_COM]->printf ("send report to url %s\n",url);

    if (WiFi.status()!= WL_CONNECTED){
      Serial.println("Error in WiFi connection");
      return;
      } 
    
    HTTPClient http;   
    http.begin(url);
    http.addHeader("Content-Type", "text/plain"); 

      
    //const int capacity1 = JSON_OBJECT_SIZE(12);
  
    DynamicJsonDocument  message(1024);
    message["ApparelId"] =  RATIEREID;
    message["MsgId"] =      MSGID_GENERAL;
  
    JsonObject payload = message.createNestedObject("Payload");
  
    payload["Version"]= rep->version_;
    payload["Temperature"]= temp;
    payload["TemperatureMax"]= rep->max_temp;
    payload["Speed"]= speed_;
    payload["SpeedMax"]= rep->max_speed;
    payload["Frame"]= nb_frames;
    payload["idle"]= rep->time_idle;
    payload["TotalCycle"]= rep->picks_counter;

    serializeJson(message,putData);
    
    if (debug) COM[DEBUG_COM]->printf ("Put Status:\n%s\n",putData);
      
    int httpResponseCode = http.PUT(putData);
    if(httpResponseCode!=200){
        Serial.print("Error on sending PUT Request: ");
        Serial.println(httpResponseCode);
        }
 
    http.end();
    
    http.begin(url);
    http.addHeader("Content-Type", "text/plain"); 

    DynamicJsonDocument  message2(1024);  
    message2["ApparelId"] =  RATIEREID;
    message2["MsgId"] =      MSGID_SPEEDTABLE;
    JsonObject payload2 = message2.createNestedObject("Payload");
     
    JsonArray speed_time = payload2.createNestedArray("SpeedTable");
    for (i=0; i<50; i++) {
      speed_time.add(rep->time_running[i]);
      }
      
    serializeJson(message2,putData);  
    
    if (debug) COM[DEBUG_COM]->printf ("Put SpeedTable:\n%s\n",putData);
    
    httpResponseCode = http.PUT(putData);
    if(httpResponseCode!=200){
        Serial.print("Error on sending PUT Request: ");
        Serial.println(httpResponseCode);
        }
 
    http.end();
    /*
    const int capacity3 = JSON_OBJECT_SIZE(29);
    StaticJsonDocument<capacity3> jDoc3;
    JsonArray temp_data = jDoc3.createNestedArray("TempTable");
    for (i=0; i<50; i++) {
      temp_data.add(rep->temp_table[i]);
      }    
    serializeJson(jDoc3,putData);
    if (debug) COM[DEBUG_COM]->printf ("making TempTable PUT request\n");
    client.put("/ratiere/in", contentType, putData);
 
    
    const int capacity4 = JSON_OBJECT_SIZE(28);
    StaticJsonDocument<capacity4> jDoc4;
    
    JsonArray lame = jDoc4.createNestedArray("Lame"); 

    unsigned long long total = 0;
    
    for (i=0; i<28; i++) {
      total=0;
      // JsonArray cycle = lame.createNestedArray("Cycles");
      for (j=0; j<50; j++) {  
        total += rep->cycles[i][j];
      //    cycle.add(rep->cycles[i][j]);
        }
      lame.add(total);  
    }
    
    serializeJson(jDoc4,putData);
    if (debug) COM[DEBUG_COM]->printf ("making TempTable PUT request\n");
    client.put("/ratiere/in", contentType, putData);
  */
}



// Main
void loop() 
{  
  unsigned short speed_;
  unsigned char temp, nb_frames;
  int ret=-1;
   
  if (debug) COM[DEBUG_COM]->print("\n******** New loop ********\n");

 
  

  // get report from connected ratiere on COM1 //     
  ret = get_report(&speed_, &temp, &nb_frames, &report);
  if (bouchon) ret = 0;
 
  if (ret==0) {
    send_report(speed_,temp,nb_frames,&report);
    delay(3000); 
    }
    
  // Send report to server //       
  else {
    if (debug)COM[DEBUG_COM]->print ("get_report failed\n");
    }     
}
