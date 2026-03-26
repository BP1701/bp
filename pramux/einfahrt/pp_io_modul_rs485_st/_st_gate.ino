#define KABA_EFE 12     // Freigabe Ein- Auslass
#define KABA_DME 39     // Drehmeldung Ein- Auslass
#define KABA_READY 36   // Bereit

#define DEBOUNCE_TIME 50 //Entprellzeit in ms

unsigned long time_check_inputs = 0; //Temp Speicher für Startwert für Entprellzeit

//Inputs States (-1 = debounce false2true ,0 = false, 1 = true, 2 = debounce true2false)
int8_t kaba_release = 0;
int8_t kaba_rotation = 0;
int8_t kaba_ready = 0;

// state machine
byte gate_status = 0;
byte gate_move = 0;
byte gate_pass = 0;


byte gate_disable = 1; //Inititial gate status
byte gate_disable_undefined = 1;

uint8_t kaba_state = 0;

//
extern bool time_to_pass_state;

void init_gate() {

  pinMode(KABA_EFE, OUTPUT);
  pinMode(KABA_DME, INPUT);
  pinMode(KABA_READY, INPUT);

  digitalWrite(KABA_EFE, false);

  Serial.println("Init Gate");

  gate_check_ports();

}

void loop_gate()
{
  gate_check_ports();
  kaba_state_machine();
}

void kaba_state_machine() {

  switch (kaba_state) {
    case 0: //wait until KABA is ready

      if (kaba_ready > 0) {
        Serial.printf("KABA is ready\n");
        
        digitalWrite(BEEPER_PIN, true);    
        vTaskDelay(250 / portTICK_PERIOD_MS);
        digitalWrite(BEEPER_PIN, false); 
        
        kaba_state = 1;
      }
      break;
      
    case 1: //wait for client --> if client check -- open gate

      //gate_status = 0;
      if (gate_move == 1) { //trigger für Durchgangsanfrage; RFID ODER SONST WAS

        Serial.println("State go now\n");
        kaba_release = 5; //5x50ms = 250ms
        kaba_state = 2;
        gate_status = 1; // open or moving
        gate_move = 0;
        gate_pass = 0;
      }
      break;

    case 2: // pass client
 
      if (kaba_rotation > 0) { //Durchgang erkannt
        gate_pass = 1;
      }  

      if (!gate_pass && (kaba_ready > 0) && (kaba_release <= 0)) { // Gate ist wieder Bereit, aber es wurde kein Durchgang erkannt. kaba_release giebt dem Gate Zeit vom Status ready auf nicht ready zu wechseln. 

        Serial.printf("u2slow\n");
        
        kaba_state = 3;
        gate_status = 3; // zeit abgelaufen und gate hat sich nicht bewegt
      }

      if (gate_pass && (kaba_ready > 0)) { // Gate ist wieder Bereit und es wurde ein Durchgang erkannt 

        Serial.printf("pass ok\n");

        kaba_state = 3;
        gate_status = 2; // normale passage
      }
      break;
      
    case 3: // info rs485 buffer state
    // do nothing in this state -> state is reset by rs485 -> next state =2 
      break;
      
    default: kaba_state = 0;
  }
}

void gate_check_ports() {

  uint8_t kaba_rotation_port;
  uint8_t kaba_ready_port;

  if (millis() - time_check_inputs < DEBOUNCE_TIME) {
        return;
    }
    
  time_check_inputs  = millis();
  
  if (kaba_release) { //Relay für x mal DEBOUNCE_TIME setzen
    digitalWrite(KABA_EFE, true);
    kaba_release--;
  } else {
    digitalWrite(KABA_EFE, false);
  }

  kaba_rotation_port = !digitalRead(KABA_DME);
  kaba_ready_port    = !digitalRead(KABA_READY);

  if (kaba_rotation_port) {
    if (kaba_rotation  = 0) kaba_rotation = -1; //Entprellzeit false2true
    if ((kaba_rotation  = -1) ||(kaba_rotation  = 2)) kaba_rotation = 1; //(Wechsel zu true) oder (rücksetzen Entprellzeit true2false)  
  } else {
    if (kaba_rotation  = 1) kaba_rotation = 2; 
    if ((kaba_rotation  = -1) ||(kaba_rotation  = 2)) kaba_rotation = 0;    
  }

  if (kaba_ready_port) {
    if (kaba_ready  = 0) kaba_ready = -1; //Entprellzeit false2true
    if ((kaba_ready  = -1) ||(kaba_ready  = 2)) kaba_ready = 1; 
  } else {
    if (kaba_ready  = 1) kaba_ready = 2; 
    if ((kaba_ready  = -1) ||(kaba_ready  = 2)) kaba_ready = 0;    
  } 
}
