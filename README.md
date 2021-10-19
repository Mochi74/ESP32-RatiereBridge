# ESP32-Ratiere-Bridge

Bridge beetween CS5 ratiere controller and Edge gateway

CS5 in linked to RS232 (COM1) of ESP32 (though a 3.3V MAX32 voltage adaptor)  
RX pin x
TX pin x


Bridge read frequently (5s) CS5 status and build a report block.
Bridge get connected on local WiFi using SSID and PASS of conf file. 

Bridge request configuration from Edge Gateway using HTTP request 
get http://gatewayserver:port/config?value=bridge_ip_address  

bridge receive :
apparelID, and Period in sec for each message type
get config from url http://192.168.1.35:1880/config?value=192.168.1.238
HTTP Response code:200, Body
{"ApparelID":"Ratiere5","PeriodeStatut":60,"PeriodeTempTable":120,"PeriodeSpeedTable":120,"PeriodeLame":300}


===============================================================

Used Libraries: (must be installed in the arduino IDE):

https://github.com/espressif/arduino-esp32
HTTPCLient
ArduinoJson
EEPROM

Arduino hardware configuration:

# Hardware
Bridge use ESP32Dev Kit Rev C (ESP32-WROOM-32) version E et EU
here is the wiring diagram recomendation:
https://raw.githubusercontent.com/AlphaLima/ESP32-Serial-Bridge/master/ESP32-SerialBridge.jpg             
Pinning                                                                                     
COM0 Rx <-> GPIO21                                                                               
COM0 Tx <-> GPIO01                                                                                 
COM1 Rx <-> GPIO16                                                                               
COM1 Tx <-> GPIO17                                                                              
                                                                       

NOTE: The PIN assignment has changed and may not look straigt forward (other PINs are marke as Rx/Tx), but this assignment allows to flash via USB also with hooked MAX3232 serial drivers.

I recomend to start your project with a Node32s or compatible evaluation board. For a TTL to RS232 level conversion search google for "TTL RS3232 Converter"



