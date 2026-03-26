#define BEEPER_PIN  22

uint32_t time_check_beep = 0; 
uint16_t _beep_time = 250;

void init_beeper()
{
  pinMode(BEEPER_PIN, OUTPUT);
  digitalWrite(BEEPER_PIN, true);    
  time_check_beep = millis();
}

void loop_beeper()
{
  if (millis() - time_check_beep > _beep_time)
  {
    digitalWrite(BEEPER_PIN, LOW);    
  }
}

void beep()
{
  beep(250);
}

void beep(uint16_t beep_time)
{
  digitalWrite(BEEPER_PIN, true);    
  time_check_beep = millis();  
  _beep_time = beep_time;
}
