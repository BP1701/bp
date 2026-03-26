#include <Wire.h>
#include <SparkFun_Alphanumeric_Display.h>

#define ALLIVE_MISSING_COUNT_MAX 50
#define CCTALK_TIMEOUT_COUNT_MAX 5

#define EMP800_CCTALK_ID  0x02
#define CCTALK_UART 2
#define CCTALK_BAUDRATE 9600

#define KABA_EFE 12     // Freigabe Ein- Auslass

#define ES005_COIN_SIGNAL    34   // Input
//#define ES005_INHIBIT_SIGNAL 12   // Output, nicht verwendet wegen Gatesteuerung 
#define EMR200_SWITCH        35   // Input
#define EMR200_MOTOR         13   // Output
#define E105_ACCEPTANCE      25   // Output
#define E105_RETURN          33   // Output

#define ENTRY_PRICE 200

#define CT10_01  0x01
#define CT20_02  0x02
#define CT50_03  0x03
#define EUR1_04  0x04
#define EUR2_05  0x05
#define WERTM_14 0x0e
#define WERTM_15 0x0f

#define POLL_EMP800_TIME 250
#define AMOUNT_WAIT_MAX 40 //10 sek
#define DISPLAY_WAIT_MAX 8 //20 sek

HT16K33 display;
PP_ST_CCTALK cctalk;
PP_ST_EMP800 emp800;

unsigned long time_poll_emp800 = 0; //Temp Speicher für RFID Abfrage

//uint8_t cctalk_request_buffer[32];

bool emr200_enable = false; //Polling aktivieren
bool emr200_return_coin = false; //Coin rückgabe Motor aktivieren

uint8_t allive_output_count = 0;
uint8_t allive_input_count = 0;
uint8_t cctalk_timeout_count = 0;

uint16_t amount_paid = 0;
uint16_t amount_wait_counter = 0; //counter * POLL_EMP800_TIME = amount wait time

uint16_t display_wait_counter = 0;
String display_string_delayed;

void cctalkReceiveCallback(byte *request_buffer, byte *receive_buffer)
{
  emp800.handleResponse(request_buffer, receive_buffer);
  cctalk_timeout_count = 0;
}

void cctalkTimeoutCallback(byte *request_buffer)
{
  emp800.handleTimeout(request_buffer);
  cctalk_timeout_count++;
}

int emp800RequestCallback(byte *request_buffer)
{
  return cctalk.sendRequest(request_buffer);
}

void init_coins()
{
  Wire.begin();

  vTaskDelay(100 / portTICK_PERIOD_MS); //give the display time to start

  if (display.begin() == false)
  {
    Serial.println("Device did not acknowledge!");
  }

  displayprint(amount_paid);

  emr200_return_coin = true;
  emr200_enable = true;

  display_string_delayed = "";
  
  cctalk.begin(CCTALK_UART, CCTALK_BAUDRATE, cctalkReceiveCallback, cctalkTimeoutCallback);
  emp800.begin(emp800RequestCallback, emp800EventCallback, EMP800_CCTALK_ID, 0x601f);//Nur Münze vom Typ 1 (10CT), Typ 2 (20CT), Typ 3 (50CT), Typ 4 (1EUR), Typ 5 (2EUR) und 14 (Wertmarke groß, auch angelernt auf 15) annehmen 0x601f -> 0x0010000000011111

  pinMode(ES005_COIN_SIGNAL, INPUT);
  pinMode(EMR200_SWITCH, INPUT);

  set_gpio(KABA_EFE, false, 0, 0);
  set_gpio(EMR200_MOTOR, false, 0, 0);
  set_gpio(E105_ACCEPTANCE, false, 0, 0);
  set_gpio(E105_RETURN, false, 0, 0);

  set_gpio(E105_RETURN, true, 500, 0);
  //set_gpio(E105_ACCEPTANCE, true, 500, 0);
}


void loop_coins()
{
  cctalk.readResponse();

  if ((emr200_return_coin == true) || (digitalRead(EMR200_MOTOR))) //Muenzrueckgabe
  {
    if ((emr200_return_coin == true) && (digitalRead(EMR200_SWITCH)))
    {
      set_gpio(EMR200_MOTOR, true, 0, 0);       
    }
    else if(!digitalRead(EMR200_SWITCH)) 
    {
      Serial.println("EMR200 COIN RETURN");
      set_gpio(EMR200_MOTOR, true, 0, 0);
      emr200_return_coin = false;
    }
    else
    {
       set_gpio(EMR200_MOTOR, false, 0, 0);
    }
  }

  if (millis() - time_poll_emp800 < POLL_EMP800_TIME) {
    return;
  }

  time_poll_emp800 = millis();

  if((emr200_enable) && (allive_input_count <= ALLIVE_MISSING_COUNT_MAX))
  {
    emp800.pollBufferedCredit();
  }  

  if (amount_paid > 0) amount_wait_counter++;

  if (amount_wait_counter >= AMOUNT_WAIT_MAX)
  {
        set_gpio(E105_RETURN, true, 1000, 0);
        amount_paid = 0;
        amount_wait_counter = 0;

        displayprint(amount_paid);
  
        Serial.print("return money by amount_wait_counter\n");     
  }  

  if (display_string_delayed.length() > 0)
  {
    if (DISPLAY_WAIT_MAX > display_wait_counter)
    {
      display_wait_counter++;
    }
    else
    {
      displayprint(display_string_delayed);
      display_wait_counter = 0;
      display_string_delayed = "";
    }
  }
}

void emp800EventCallback(uint8_t coin_id, uint8_t error_code)
{
  byte temp_lb;
  byte temp_hb;
        
  if (error_code == 0)
  {

    if((coin_id == EUR2_05) || (coin_id == EUR1_04) || (coin_id == CT50_03) || (coin_id == CT20_02) || (coin_id == CT10_01) || (coin_id == WERTM_14)|| (coin_id == WERTM_15)) 
    { 
    
      Serial.println("allowed Coin inserted");

      if (coin_id == WERTM_15) coin_id = WERTM_14; //Coind ID 14 auch auf Kanal 15 angelernt

      amount_wait_counter = 0;

      //Begin MQTT HEX conversion
      temp_lb = coin_id & 0x0f;
      temp_hb = (coin_id & 0xf0) >> 4; 

      if (temp_hb > 9) 
        {
          coin_data[0] = temp_hb + 87;     
        } 
        else
        {
          coin_data[0] = temp_hb + 48;         
        }

      if (temp_lb > 9)
        {
          coin_data[1] = temp_lb + 87;                
        } 
        else
        {
           coin_data[1] = temp_lb + 48;         
        }

      coin_data_len = 2;
      coin_data_available = true;
      
      //END MQTT HEX conversion
      
      if(coin_id == EUR2_05) amount_paid = amount_paid + 200;
      if(coin_id == EUR1_04) amount_paid = amount_paid + 100;
      if(coin_id == CT50_03) amount_paid = amount_paid +  50;
      if(coin_id == CT20_02) amount_paid = amount_paid +  20;
      if(coin_id == CT10_01) amount_paid = amount_paid +  10;     


      if (ENTRY_PRICE == amount_paid)
      { 
        displayprint(amount_paid);
        display_string_delayed = "   0";

        counter_value[0] = counter_value[0] + amount_paid;

        Serial.print("Counter Cash: ");
        Serial.println(counter_value[0]);
        
        counter_update_flag |= 0x01;
        counter_data_available = true;        
        
        set_gpio(E105_ACCEPTANCE, true, 1000, 0);  
        set_gpio(KABA_EFE, true, 250, 0);
        led_set(1, 0, 128, 0, 5000, 0);

        amount_paid = 0;      
        Serial.print("gate opened by cash\n");  
      }
      else if (coin_id == WERTM_14)
      {
        displayprint("COIN");
        display_string_delayed = "   0";
        
        counter_value[1] = counter_value[1] + 1;
        counter_update_flag |= 0x02;
        counter_data_available = true;        
        
        set_gpio(E105_ACCEPTANCE, true, 1000, 0);  
        set_gpio(KABA_EFE, true, 250, 0);
        led_set(1, 0, 128, 0, 5000, 0);

        amount_paid = 0;      
        Serial.print("gate opened by coin\n");
      }
      else if ((ENTRY_PRICE > amount_paid) && (0 <  amount_paid))
      {       
        displayprint(amount_paid);
        Serial.print("amount paid = ");
        Serial.println(amount_paid);
      } 
      else
      { 
        displayprint(amount_paid);
        display_string_delayed = "   0";
        set_gpio(E105_RETURN, true, 1000, 0);  
        amount_paid = 0;
      } 
    }
  }
}

void displayprint(uint16_t amount)
{
  char display_values[] = {' ', ' ', ' ', ' '};
  String amount_string = String(amount);

  if (amount >= 1000)
  {
    display_values[0] = amount_string.charAt(0); 
    display_values[1] = amount_string.charAt(1);
    display_values[2] = amount_string.charAt(2);
    display_values[3] = amount_string.charAt(3);
  }
  else if ((amount < 1000) && (amount >= 100))
  {
    display_values[1] = amount_string.charAt(0);
    display_values[2] = amount_string.charAt(1);
    display_values[3] = amount_string.charAt(2);    
  }
  else if ((amount < 100) && (amount >= 10))
  {
    display_values[2] = amount_string.charAt(0);
    display_values[3] = amount_string.charAt(1);     
  }
  else 
  {
    display_values[3] = amount_string.charAt(0);
  }

  display.clear();
  display.printChar(display_values[0], 0);
  display.printChar(display_values[1], 1);
  display.printChar(display_values[2], 2);
  display.printChar(display_values[3], 3);      
  display.updateDisplay();
}

void displayprint(String text)
{
  char display_values[] = {' ', ' ', ' ', ' '};

  if (text.length() == 4)
  {
    display_values[0] = text.charAt(0); 
    display_values[1] = text.charAt(1);
    display_values[2] = text.charAt(2);
    display_values[3] = text.charAt(3);
  
    display.clear();
    display.printChar(display_values[0], 0);
    display.printChar(display_values[1], 1);
    display.printChar(display_values[2], 2);
    display.printChar(display_values[3], 3);      
    display.updateDisplay();
  }
}
