#include <FastLED.h>

#define NUM_PIXELS_MAX   118 //Maximal Anzahl Pixel 
#define CHECK_LEDS_TIME  50 //Dauer Prüfzyklus LEDs
#define RGB_LED 0 //RGB-LED
#define GRB_LED 1 //GRB-LED

unsigned long time_check_leds = 0; //Temp Speicher für Startwert für Port check

// Define the array of leds
CRGB pixels[NUM_PIXELS_MAX];

unsigned long time_leds_current[] = {0, 0, 0, 0, 0, 0}; //Temp Speicher für Startwert Port dauer

uint8_t  flash_counter = 0;

struct _led_groups
{
  uint8_t start;
  uint8_t end;
  bool overlap;
  uint8_t color_arrangement;
};

/****** Einfahrt Camoingplatz NDD **
struct _led_groups led_groups[] = 
{
  { .start = 0, .end = 12, .overlap = false, .color_arrangement = GRB_LED },       // group 0 
  { .start = 13, .end = 35, .overlap = false, .color_arrangement = GRB_LED },     // group 1
  { .start = 0, .end = 35, .overlap = false, .color_arrangement = GRB_LED },       // group 2
  { .start = 0, .end = 0, .overlap = false, .color_arrangement = GRB_LED },      // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },         // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }          // group 5
};
/**/

/****** EV-Ladesäule **
struct _led_groups led_groups[] = 
{
  { .start = 0, .end = 20, .overlap = false, .color_arrangement = GRB_LED },       // group 0 
  { .start = 21, .end = 52, .overlap = false, .color_arrangement = GRB_LED },     // group 1
  { .start = 53, .end = 73, .overlap = false, .color_arrangement = GRB_LED },       // group 2
  { .start = 74, .end = 117, .overlap = false, .color_arrangement = GRB_LED },      // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },         // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }          // group 5
};
/**/

/****** EV-Ladesäule Saelker **
struct _led_groups led_groups[] = 
{
  { .start = 0, .end = 43, .overlap = false, .color_arrangement = GRB_LED },      // group 0
  { .start = 44, .end = 65, .overlap = false, .color_arrangement = GRB_LED },       // group 1 
  { .start = 66, .end = 93, .overlap = false, .color_arrangement = GRB_LED },     // group 2
  { .start = 94, .end = 118, .overlap = false, .color_arrangement = GRB_LED },       // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },         // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }          // group 5
};
/**/

/****** EV-Ladesäule Muster **
struct _led_groups led_groups[] = 
{
  { .start = 0, .end = 17, .overlap = false, .color_arrangement = GRB_LED },       // group 0 
  { .start = 18, .end = 35, .overlap = false, .color_arrangement = GRB_LED },     // group 1
  { .start = 36, .end = 53, .overlap = false, .color_arrangement = GRB_LED },       // group 2
  { .start = 54, .end = 97, .overlap = false, .color_arrangement = GRB_LED },      // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },         // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }          // group 5
};
/**

/****** PINPAD **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 3, .overlap = false, .color_arrangement = GRB_LED },      // group 0
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 1
  { .start = 1, .end = 1, .overlap = true, .color_arrangement = GRB_LED },       // group 2
  { .start = 2, .end = 2, .overlap = true, .color_arrangement = GRB_LED },       // group 3
  { .start = 3, .end = 3, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/****** Stromsäule Dagebüll **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 18, .overlap = false, .color_arrangement = GRB_LED },     // group 0 - Hinterleuchtung QR Code
  { .start = 19, .end = 37, .overlap = true, .color_arrangement = GRB_LED },     // group 1
  { .start = 38, .end = 56, .overlap = true, .color_arrangement = GRB_LED },     // group 2
  { .start = 0, .end = 56, .overlap = true, .color_arrangement = GRB_LED },      // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/* **** Stromsäule 2-fach schmale Version **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 17, .overlap = false, .color_arrangement = GRB_LED },      // group 0
  { .start = 18, .end = 37, .overlap = true, .color_arrangement = GRB_LED },       // group 1
  { .start = 0, .end = 37, .overlap = true, .color_arrangement = GRB_LED },       // group 2
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/* **** Stromsäule 2-fach verbreiterte Version **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 20, .overlap = false, .color_arrangement = GRB_LED },      // group 0
  { .start = 21, .end = 40, .overlap = true, .color_arrangement = GRB_LED },       // group 1
  { .start = 0, .end = 40, .overlap = true, .color_arrangement = GRB_LED },       // group 2
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/* **** Stromsäule 4-fach **
struct _led_groups led_groups[] =
{
  { .start =  0, .end =  73, .overlap = false, .color_arrangement = GRB_LED },      // group 0
  { .start = 74, .end =  80, .overlap = true, .color_arrangement = GRB_LED },       // group 1
  { .start = 81, .end =  87, .overlap = true, .color_arrangement = GRB_LED },       // group 2
  { .start = 88, .end =  94, .overlap = true, .color_arrangement = GRB_LED },       // group 3
  { .start = 95, .end = 102, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 0,  .end = 102, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/* **** Stromsäule 2-fach Hochwasser **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 36, .overlap = false, .color_arrangement = GRB_LED },     // group 0
  { .start = 37, .end = 73, .overlap = true, .color_arrangement = GRB_LED },     // group 1
  { .start = 0, .end = 73, .overlap = true, .color_arrangement = GRB_LED },      // group 2
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/****** Münzsäule Lütetsburg **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 19, .overlap = false, .color_arrangement = GRB_LED },     // group 0 
  { .start = 20, .end = 94, .overlap = true, .color_arrangement = GRB_LED },     // group 1
  { .start = 0, .end = 94, .overlap = true, .color_arrangement = GRB_LED },      // group 2
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },      // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/* **** pramux Leser RFID **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 12, .overlap = false, .color_arrangement = GRB_LED },      // group 0
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 1
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 2
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/* **** pramux Leser RFID Seitlich 2 LEDs **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 0, .overlap = false, .color_arrangement = RGB_LED },      // group 0
  { .start = 1, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 1
  { .start = 1, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 2
  { .start = 1, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 3
  { .start = 1, .end = 0, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 1, .end = 0, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/* ***** Wassersaule **
struct _led_groups led_groups[] =
{
  { .start = 0, .end = 3, .overlap = false, .color_arrangement = GRB_LED },      // group 0
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },       // group 1
  { .start = 1, .end = 1, .overlap = true, .color_arrangement = GRB_LED },       // group 2
  { .start = 2, .end = 2, .overlap = true, .color_arrangement = GRB_LED },       // group 3
  { .start = 3, .end = 3, .overlap = true, .color_arrangement = GRB_LED },      // group 4
  { .start = 4, .end = 102, .overlap = true, .color_arrangement = GRB_LED }       // group 5
};
/**/

/****** WC-Gate **/
struct _led_groups led_groups[] = 
{
  { .start = 0, .end = 43, .overlap = false, .color_arrangement = GRB_LED },      // group 0
  { .start = 0, .end = 0, .overlap = false, .color_arrangement = GRB_LED },       // group 1 
  { .start = 0, .end = 0, .overlap = false, .color_arrangement = GRB_LED },     // group 2
  { .start = 0, .end = 0, .overlap = false, .color_arrangement = GRB_LED },       // group 3
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED },         // group 4
  { .start = 0, .end = 0, .overlap = true, .color_arrangement = GRB_LED }          // group 5
};
/**/

uint8_t led_changed[]     = {1, 0, 0, 0, 0, 0};
boolean pixels_updated = false;

uint8_t led_blink_default[] = {0, 0, 0, 0, 0, 0};
uint8_t led_red_default[]   = {0,    0,   0,   0,   0,   0};
uint8_t led_green_default[] = {0,    0,   0,   0,   0,   0};
uint8_t led_blue_default[]  = {128, 0, 0, 0, 0, 0};

uint8_t  led_red_current[]      = {0, 0, 0, 0, 0, 0};
uint8_t  led_green_current[]    = {0, 0, 0, 0, 0, 0};
uint8_t  led_blue_current[]     = {0, 0, 0, 0, 0, 0};
uint8_t  led_blink_current[]    = {0, 0, 0, 0, 0, 0};
uint32_t led_duration_current[] = {0, 0, 0, 0, 0, 0};

void init_leds()
{
  // put your setup code here, to run once:

  FastLED.addLeds<WS2812B, LED_DATA_PIN, RGB>(pixels, NUM_PIXELS_MAX);

  update_leds(0, 0, 0, 0);
  update_leds(1, 0, 0, 0);
  update_leds(2, 0, 0, 0);
  update_leds(3, 0, 0, 0);
  update_leds(4, 0, 0, 0);
  update_leds(5, 0, 0, 0);
  FastLED.show();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  update_leds(0, 128, 0, 0);
  update_leds(1, 128, 0, 0);
  update_leds(2, 128, 0, 0);
  update_leds(3, 128, 0, 0);
  update_leds(4, 128, 0, 0);
  update_leds(5, 128, 0, 0);
  FastLED.show();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  update_leds(0, 0, 0, 0);
  update_leds(1, 0, 0, 0);
  update_leds(2, 0, 0, 0);
  update_leds(3, 0, 0, 0);
  update_leds(4, 0, 0, 0);
  update_leds(5, 0, 0, 0);
  FastLED.show();
}

void led_set(uint8_t led, uint8_t red, uint8_t green, uint8_t blue, uint32_t duration, uint8_t blink)
{

  if ((led >= 0) || (led < 6))
  {

    led_red_current[led]   = red;
    led_green_current[led] = green;
    led_blue_current[led]  = blue;
    led_duration_current[led] = duration;
    led_blink_current[led]  = blink;
  
  time_leds_current[led] = 0; //duration neu starten

    update_leds(led, red, green, blue);
  }
}

void led_set_defaults(uint8_t led)
{
  update_leds(led, led_red_default[led], led_green_default[led], led_blue_default[led]);
}

void led_read_defaults(uint8_t led)
{ /*
    defaults_red = EEPROM.read(EEPROM_ADDR_LED + 0);
    defaults_green = EEPROM.read(EEPROM_ADDR_LED + 1);
    defaults_blue = EEPROM.read(EEPROM_ADDR_LED + 2);
    defaults_blink = EEPROM.read(EEPROM_ADDR_LED + 3);*/
}

void led_write_defaults(uint8_t led, uint8_t red, uint8_t green, uint8_t blue, uint8_t blink)
{ /*
    EEPROM.write(EEPROM_ADDR_LED + 0, red);
    EEPROM.write(EEPROM_ADDR_LED + 1, green);
    EEPROM.write(EEPROM_ADDR_LED + 2, blue);
    EEPROM.write(EEPROM_ADDR_LED + 3, blink);

    defaults_red = red;
    defaults_green = green;
    defaults_blue = blue;
    defaults_blink = blink;

    led_set_defaults();*/
}

void set_leds()
{

  if ((millis() - time_check_leds) < CHECK_LEDS_TIME)
  {

    return;
  }
  else
  {
    time_check_leds = millis();

    flash_counter++;
    if (flash_counter > 9) flash_counter = 0;

    for (int led = 0; led < 6; led++)
    {

      //change
      if (led_changed[led])
      {
        led_changed[led] = 0;

        if ((led_duration_current[led] > 0) && (led_blink_current[led] != 1))
        {
          update_leds(led, led_red_current[led], led_green_current[led], led_blue_current[led]);
        }
        if ((led_duration_current[led] == 0) && (led_blink_default[led] != 1))
        {
          update_leds(led, led_red_default[led], led_green_default[led], led_blue_default[led]);
        }
      }

      //blink
      if (((led_duration_current[led] >= 0) && (led_blink_current[led] == 1)) || ((led_duration_current[led] == 0) && (led_blink_default[led] == 1)))
      {
        if (flash_counter == 0)
        { //on
          if (led_duration_current[led] >= 0)
          {
            update_leds(led, led_red_current[led], led_green_current[led], led_blue_current[led]);
          }
          else
          {
            update_leds(led, led_red_default[led], led_green_default[led], led_blue_default[led]);
          }
        }
        if (flash_counter == 5)
        { //off
          update_leds(led, 0, 0, 0);
        }
      }

      //duration
      if (led_duration_current[led] > 0)
      { // duration gesetzt
        if (time_leds_current[led] > 0)
        { //zeit laeuft bereits
          if ((millis() - time_leds_current[led]) > led_duration_current[led])
          {
            led_blink_current[led] = 0;
            led_duration_current[led] = 0;
            led_changed[led] = 1; // durch normale Routine wieder auf alten Wert sertzen lassen
          }
        }
        else
        {
          time_leds_current[led] = millis(); //zeit starten
        }
      }
      else
      {
        time_leds_current[led] = 0;
      }
    }
  }

  if (pixels_updated)
  {
    FastLED.show();
    pixels_updated = false;
    Serial.println("pixels_updated");
  }
}

void update_leds(uint8_t led, uint8_t red, uint8_t green, uint8_t blue) 
{
  uint8_t pixel_num_end;
  uint8_t pixel_num_start;
  uint8_t pixel_color_arrangement;
  int i;

  pixel_num_start = led_groups[led].start;
  pixel_num_end   = led_groups[led].end;
  pixel_color_arrangement = led_groups[led].color_arrangement;

  for (i = pixel_num_start; i <= pixel_num_end; i++) {
    if (pixel_color_arrangement == GRB_LED)
    {
      pixels[i] = CRGB(green, red, blue);
    }
    else {
      pixels[i] = CRGB(red, green, blue);
    }
  }  
  pixels_updated = true;
}
