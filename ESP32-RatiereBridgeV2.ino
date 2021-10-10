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


/* default configuration */
String ratiere_id = "Unknown"; 
int statutPeriod= 60;
int tempTablePeriod = 60;
int speedTablePeriod = 60;
int lameTablePeriod = 300;
int threshold = 70;


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

   if (debug) COM[DEBUG_COM]->printf ("\n***get_report\n");
 

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
        
        if ((sz!=257)&&(sz!=143)) {
          if (debug)COM[DEBUG_COM]->printf ("sz = %u\n",sz); // cas d'erreur il manque des données.  
          return -1;    
        }
        
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

unsigned long lastsent_statut=0;
unsigned long lastsent_tempTable=0;
unsigned long lastsent_speedTable=0;
unsigned long lastsent_lameTable=0;
 
 
void send_report(unsigned short speed_,unsigned char temp, unsigned char nb_frames, report_data_t *rep){

    unsigned int i,j; 
    char putData[2048];

    char url[200];
    sprintf(url,"http://%s:%d%s",GATEWAY_SERVER,GATEWAY_PORT,PATH);

    if (debug) COM[DEBUG_COM]->printf ("*** send report to url %s\n",url);

    if (WiFi.status()!= WL_CONNECTED){
      COM[DEBUG_COM]->printf("Error in WiFi connection\n");
      return;
      } 
    
    HTTPClient http;   
    unsigned long current_time = millis();
    
    if (((current_time-lastsent_statut)/1000) > statutPeriod || (lastsent_statut > current_time)) {    

        
        DynamicJsonDocument  message(1024);
        message["ApparelId"] =  ratiere_id;
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
        
        if (debug) COM[DEBUG_COM]->printf ("Put Status:\n");
    
        http.begin(url);
        http.addHeader("Content-Type", "text/plain"); 
      
        int httpResponseCode = http.PUT(putData);
        if(httpResponseCode!=200){
            COM[DEBUG_COM]->printf("Error n°:%d on PUT Request:\n%s\n",httpResponseCode,putData);
        }
        else {
            lastsent_statut = current_time;  
        }
        http.end();
            
    }
    
    if (((current_time-lastsent_speedTable)/1000) > speedTablePeriod || (lastsent_speedTable > current_time)) {    

        DynamicJsonDocument  message2(1024);  
        message2["ApparelId"] =  ratiere_id;
        message2["MsgId"] =      MSGID_SPEEDTABLE;
        JsonObject payload2 = message2.createNestedObject("Payload");
         
        JsonArray speed_time = payload2.createNestedArray("SpeedTable");
        for (i=0; i<50; i++) {
          speed_time.add(rep->time_running[i]);
          }
          
        serializeJson(message2,putData);  
        
        if (debug) COM[DEBUG_COM]->printf ("Put SpeedTable:\n");
        
        http.begin(url);
        http.addHeader("Content-Type", "text/plain"); 
    
        int httpResponseCode = http.PUT(putData);
        
        if(httpResponseCode!=200){
            COM[DEBUG_COM]->printf("Error on PUT Request:\n%s\n",putData);
            COM[DEBUG_COM]->printf("%d",httpResponseCode);        
        }
        else {
            lastsent_speedTable = current_time;  
        }
        
        http.end();
    }   

    if (((current_time-lastsent_tempTable)/1000) > tempTablePeriod || (lastsent_tempTable > current_time)) {    

        DynamicJsonDocument  message3(1024);  
        message3["ApparelId"] =  ratiere_id;
        message3["MsgId"] =      MSGID_TEMPTABLE;
        JsonObject payload3 = message3.createNestedObject("Payload");
         
        JsonArray temp_data = payload3.createNestedArray("TempTable");
        for (i=0; i<50; i++) {
          temp_data.add(rep->temp_table[i]);
        }
          
        serializeJson(message3,putData);  
        
        if (debug) COM[DEBUG_COM]->printf ("Put TempTable:\n");
        
        int httpResponseCode = http.PUT(putData);
        
        if(httpResponseCode!=200){
            COM[DEBUG_COM]->printf("Error on PUT Request:\n%s\n",putData);
            COM[DEBUG_COM]->printf("%d",httpResponseCode);        
        }
        else {
            lastsent_tempTable = current_time;  
        }
       
        http.end();
    }

    if (((current_time-lastsent_lameTable)/1000) > lameTablePeriod || (lastsent_lameTable > current_time)) {    

        DynamicJsonDocument  message4(1024);  
        message4["ApparelId"] =  ratiere_id;
        message4["MsgId"] =      MSGID_LAMETABLE;
        JsonObject payload4 = message4.createNestedObject("Payload");
        
        JsonArray lame = payload4.createNestedArray("Lame"); 
    
        unsigned long long total = 0;
        
        for (i=0; i<28; i++) {
          total=0;
          for (j=0; j<50; j++) {  
            total += rep->cycles[i][j];
            }
          lame.add(total);  
        }
        
        serializeJson(message4,putData);
        
        if (debug) COM[DEBUG_COM]->printf ("Put Lame%s\n",putData);
        
        int httpResponseCode = http.PUT(putData);
        
        if(httpResponseCode!=200){
            COM[DEBUG_COM]->printf("Error on PUT Request:\n%s\n",putData);
            COM[DEBUG_COM]->printf("%d",httpResponseCode);
            }
        else {
            lastsent_lameTable = current_time;  
        }
        http.end();  
    }
}


void check_alert(unsigned short speed_,unsigned char temp){

  HTTPClient http;   
  char putData[2048];

  boolean alert = true;
   
  if (alert) {    
    char url[200];
    sprintf(url,"http://%s:%d%s",GATEWAY_SERVER,GATEWAY_PORT,PATH);

    if (debug) COM[DEBUG_COM]->printf ("*** send Alert to url %s\n",url);
    
 
    if (WiFi.status()!= WL_CONNECTED){
      COM[DEBUG_COM]->printf("Error in WiFi connection\n");
      return;
    } 

    http.begin(url);
    
    DynamicJsonDocument  message(1024);  
    message["ApparelId"] =  ratiere_id;
    message["MsgId"] =      MSGID_ALERTTEMP;
    JsonObject payload = message.createNestedObject("Payload");
    payload["AlertSource"] = "Temperature";
    payload["AlertThreshold"] = threshold;      
    payload["AlertValue"] = temp;      
          
    serializeJson(message,putData);
        
    if (debug) COM[DEBUG_COM]->printf ("Put Alert%s\n",putData);
        
    int httpResponseCode = http.PUT(putData);
   
    if(httpResponseCode!=200){
        COM[DEBUG_COM]->printf("Error on PUT Request:\n%s\n",putData);
        COM[DEBUG_COM]->printf("%d",httpResponseCode);
    }
    http.end();  
  }
   
  return;
}

void get_config() { 
    
    if (WiFi.status()!= WL_CONNECTED){
      COM[DEBUG_COM]->printf("Error in WiFi connection");
      return;
      } 
        
    char url[200];
    sprintf(url,"http://%s:%d/config?value=%s",GATEWAY_SERVER,GATEWAY_PORT,WiFi.localIP().toString().c_str());

    HTTPClient http;   
    http.addHeader("Content-Type", "text/plain"); 
    if (debug) COM[DEBUG_COM]->printf ("get config from url %s\n",url);
   
    http.begin(url);
    
    int httpResponseCode = http.GET();
    if (httpResponseCode==200) {
        if (debug) COM[DEBUG_COM]->printf ("HTTP Response code:%i\n",httpResponseCode);
        String payload = http.getString();
        String quotes = "&quot;";
        payload.replace(quotes,"\"");
        setconfig(payload);     
    }
    else {
        if (debug) COM[DEBUG_COM]->printf("Error code:%i\n",httpResponseCode);
        String payload = http.getString();
        if (debug) COM[DEBUG_COM]->printf("%s\n",payload.c_str());
    }    
    // Free resources
    http.end();
    return;    
}


void setconfig(String json){
   
  StaticJsonDocument<200> configDoc;
  DeserializationError err = deserializeJson(configDoc,json.c_str());

  if (err) {
    COM[DEBUG_COM]->printf("deserializeJson() failed with code :%s\n",err.c_str());
  }

  JsonObject configObject = configDoc.as<JsonObject>();
  
  JsonVariant variant = configObject.getMember("ApparelID");
  if (variant.isNull()) {
    COM[DEBUG_COM]->printf("ApparelId not found in config\n");
    return;
  } else {
    ratiere_id = variant.as<String>();
    if (debug) COM[DEBUG_COM]->printf("ratiere_id set to %s\n",ratiere_id.c_str());
  }

  variant = configObject.getMember("PeriodeStatut");
  if (variant.isNull()) {
    COM[DEBUG_COM]->printf("PeriodeStatut not found in config\n");
  } else {
    statutPeriod = variant.as<int>(); 
    if (debug) COM[DEBUG_COM]->printf("statutPeriod set to :%d\n",statutPeriod);
  }
  
  variant = configObject.getMember("PeriodeTempTable");
  if (variant.isNull()) {
    COM[DEBUG_COM]->printf("PeriodeTempTable not found in config\n");
  } else {
    tempTablePeriod = variant.as<int>(); 
    if (debug) COM[DEBUG_COM]->printf("tempTablePeriod set to :%d\n",tempTablePeriod);
  }

  variant = configObject.getMember("PeriodeSpeedTable");
  if (variant.isNull()) {
    COM[DEBUG_COM]->printf("PeriodeSpeedTable not found in config\n");
  } else {
    speedTablePeriod = variant.as<int>(); 
    if (debug) COM[DEBUG_COM]->printf("speedTablePeriod set to :%d\n",speedTablePeriod);
  }
  
  variant = configObject.getMember("PeriodeLame");
  if (variant.isNull()) {
    COM[DEBUG_COM]->printf("PeriodeSpeedTable not found in config\n");
  } else {
    lameTablePeriod = variant.as<int>(); 
    if (debug) COM[DEBUG_COM]->printf("lamePeriod set to :%d\n",lameTablePeriod);
  }  
 
 variant = configObject.getMember("Threshold");
  if (variant.isNull()) {
    COM[DEBUG_COM]->printf("Threshold not found in config\n");
  } else {
    threshold = variant.as<int>(); 
    if (debug) COM[DEBUG_COM]->printf("threshold set to :%d\n",lameTablePeriod);
  }  
  
  
  return;
}




// Main
void loop() 
{  
  unsigned short speed_;
  unsigned char temp, nb_frames;
  int ret=-1;
  unsigned int retry;
   
  if (debug) COM[DEBUG_COM]->print("\n******** New loop ********\n");

  get_config();

  // get report from connected ratiere on COM1 //     
  retry=0;
  do {
     ret = get_report(&speed_, &temp, &nb_frames, &report);
     if (bouchon) ret = 0;
     retry++;
  } while ((ret!=0) && (retry<NB_RETRY)); 
 
  if (ret==0) {
    check_alert(speed_,25);
    send_report(speed_,temp,nb_frames,&report);
    }
    
  // Send report to server //       
  else {
    if (debug)COM[DEBUG_COM]->print ("get_report failed\n");
    }

  delay(3000); 

}
