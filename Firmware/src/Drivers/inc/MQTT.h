
// -------------------------   Blynk_to_MQTT  -----------------------------------
//
// This routine receives the MQTT command data from the WEB Application
// via the ESP8266, parses the data and feeds the commands to the TM4C.
//
void MQTT_to_TM4C(void);


// -------------------------- Virtual Pin (VP) Parser --------------------------
//
void Parser(void); 


// -------------------------   MQTT_to_TM4C  -----------------------------------
//
// This routine reads all of the sensor data and sends it back to the MQTT Web Application.
//
void TM4C_to_MQTT(void);

