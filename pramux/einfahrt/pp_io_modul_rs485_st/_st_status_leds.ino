

#define LED_RED 2
#define LED_BLUE 4


#define LED_RED_BLINK_TIME 500 //Dauer  
#define LED_BLUE_BLINK_TIME 75 //Dauer  
#define STATUS_BLINK_INTERVAL 1000
uint64_t toggle_timer = 0;
uint64_t led_red_timer = 0;
uint64_t led_blue_timer = 0;



void init_status_leds()
{
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
}


void update_status_leds()
{

  if (millis() - toggle_timer > STATUS_BLINK_INTERVAL )
  {
    toggle_timer = millis();
    
    if (BLUE_LED_TRIGGER)
    {
      BLUE_LED_TRIGGER = false;
      digitalWrite(LED_BLUE, HIGH);
      led_blue_timer = millis();
    }
    
    if (RED_LED_TRIGGER)//rfid_data_available || opticon_data_avaiable || iomodul_output_available || input_gpios_data_available || iomodul_input_data_available ||  coin_data_available || pinpad_data_available) // fill any triggers to blink RED LED , for example RFIDdatavailable or Opti or... whatever
    {
      RED_LED_TRIGGER = false;
      digitalWrite(LED_RED, HIGH);
      led_red_timer = millis();
    }
  }




  if (millis() - led_blue_timer > LED_BLUE_BLINK_TIME) {
    digitalWrite(LED_BLUE, LOW);
  }
  if (millis() - led_red_timer > LED_RED_BLINK_TIME) {
    digitalWrite(LED_RED, LOW);
  }
}
