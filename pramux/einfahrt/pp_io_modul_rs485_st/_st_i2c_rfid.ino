#include "Wire.h"

#define I2CSCAN         1 
#define I2CSCAN_ADDR    0x08
#define I2CSCAN_INT_PIN 14
#define I2CSCAN_LENGTH  6
#define CHECK_I2CRFID_TIME 250

unsigned long time_check_i2cRfid = 0; //Temp Speicher für RFID Abfrage

int init_i2c_rfid()
{
  memset(rfid_data, 0x0, 64);
  memset(opticon_data, 0x0, 64);
  rfid_data_len = 0;
  opticon_data_len = 0;
  rfid_data_available = false;
  opticon_data_avaiable = false;

  pinMode(I2CSCAN_INT_PIN, INPUT);
  Wire.begin();

  //beep
  Wire.beginTransmission(I2CSCAN_ADDR);
  Wire.write(0x13);
  Wire.write(0x01);
  Wire.endTransmission(true);
}

// scanner task
void loop_i2c_rfid()
{
  if (millis() - time_check_i2cRfid < CHECK_I2CRFID_TIME) {
    return;
  }

  time_check_i2cRfid = millis();
    
  if(digitalRead(I2CSCAN_INT_PIN)) 
  {
    Wire.beginTransmission(I2CSCAN_ADDR);
    Wire.write(0x12);
    uint8_t error = Wire.endTransmission(true);
    Serial.printf("endTransmission: %u\n", error);

    error = Wire.requestFrom(I2CSCAN_ADDR, 6);
    Serial.printf("requestFrom: %u\n", error);
    if(error){
      byte temp[(error * 2)];
      byte temp_read;
      byte temp_lb;
      byte temp_hb;
      //Wire.readBytes(temp, error);

      for (int i = 0; i < (error); i++) 
      {
        temp_read = Wire.read();
        temp_lb = temp_read & 0x0f;
        temp_hb = (temp_read & 0xf0) >> 4; 

        if (temp_hb > 9)
        {
          temp[(i * 2)] = temp_hb + 87;     
        } 
        else {
          temp[(i * 2)] = temp_hb + 48;         
        }

        if (temp_lb > 9)
        {
          temp[(i * 2) + 1] = temp_lb + 87;                
        } 
        else {
          temp[(i * 2) + 1] = temp_lb + 48;         
        }
      }
      
    i2cRfidCallback(&temp[0], error * 2, DATASOURCE_RFID_FEIG);
    }
  }
}

void i2cRfidCallback(byte *result, int len, byte datasource)
{ 
#ifdef _DEBUG
  char message[64];
  memset(message, 0x0, 64);
  sprintf(message, "SCANNER: ");
  memcpy(message + 9, result, len);
  sprintf(message, "%s LEN: %d DATASOURCE: %d", message, len, (uint8_t)datasource);
  log(message);
#endif

  switch (datasource)
  {
    case DATASOURCE_RFID_FEIG:
      {
        memcpy(rfid_data, result, len);
        rfid_data_len = len;
        rfid_data_available = true;
        //set_gpio(21, 1, 150, 0);
        break;
      }
    case DATADOURCE_SCAN_OPTI:
      {
        memcpy(opticon_data, result, len);
        opticon_data_len = len;
        opticon_data_avaiable = true;
        //set_gpio(21, 1, 150, 0);
      }
      break;
  }
}
