// config: ////////////////////////////////////////////////////////////
// 

#define MODE_STA

#define NUM_COM   3                 // Total number of COM Ports
#define DEBUG_COM 0                 // Debug output to COM0
#define RATIERE_COM 2               // Ratiere on COM2

#define UART_BAUD0      19200
#define SERIAL_PARAM0   SERIAL_8N2
#define SERIAL0_RXPIN   21
#define SERIAL0_TXPIN   1
#define UART_BAUD1      19200
#define SERIAL_PARAM1   SERIAL_8N2
#define SERIAL1_RXPIN   16
#define SERIAL1_TXPIN   17
#define UART_BAUD2      19200
#define SERIAL_PARAM2   SERIAL_8N2
#define SERIAL2_RXPIN   15
#define SERIAL2_TXPIN   4

bool debug = true;
bool bouchon = false;

#define VERSION "2.10"


const char *ssid = "Linksys12063";    // You will connect your phone to this Access Point
const char *pw = "Lugulu74";          // and this is the password
//const char *ssid = "MochiHome";    // You will connect your phone to this Access Point
//const char *pw = "Fannynette";          // and this is the password

// define gateway IP address  
#define GATEWAY_SERVER "192.168.0.113"
//#define GATEWAY_SERVER "192.168.1.233"
#define GATEWAY_PORT 1880
#define PATH "/gateway/in/"

IPAddress netmask(255, 255, 255, 0);

#define RATIEREID "Ratiere1"
#define MSGID_GENERAL  "Statut"
#define MSGID_SPEEDTABLE "SpeedTable"
#define MSGID_TEMPTABLE  "TempTable"
#define MSGID_LAMETABLE  "LameTable"
#define MSGID_ALERTTEMP "TemperatureAlert" 
#define MSGID_ALERTSPEED "SpeedAlert" 
#define MSGID_EDGESTATUS "EdgeStatus"

#define NB_RETRY 5
#define BUFFER_SIZE 1024
//////////////////////////////////////////////////////////////////////////
