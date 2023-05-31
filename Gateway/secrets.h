#include <pgmspace.h>
 
#define SECRET
 
//const char WIFI_SSID[] = "ASUS_X00TD";              
//const char WIFI_PASSWORD[] = "NhutHoang0910";           
 
#define THINGNAME "esp8266hufi"
 
int8_t TIME_ZONE = 7; //NYC(USA): -5 UTC
 
const char MQTT_HOST[] = "a1du8opzxgzff8-ats.iot.us-east-1.amazonaws.com";
 
 
static const char rootCA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";
 
 
// Copy contents from XXXXXXXX-certificate.pem.crt here ▼
static const char client_certificate[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)KEY";
 
 
// Copy contents from  XXXXXXXX-private.pem.key here ▼
static const char privatekey[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----

-----END RSA PRIVATE KEY-----
)KEY";
