// Timezone and server
#define NTP_SERVER "de.pool.ntp.org"
#define TZ_INFO "WEST-1DWEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00" // Western European Time


void init_time()
{
  setenv("TZ", TZ_INFO, 1);             // configure timezone
  tzset();

  Serial.println("getting NTP time");
  struct tm local;
  configTzTime(TZ_INFO, NTP_SERVER); // synchronise ESP system time with NTP
  print_local_time();
}

uint32_t get_timestamp()
{
  tm local;
  getLocalTime(&local);
  return mktime(&local);
}

void print_local_time()
{
  tm local;
  getLocalTime(&local);
  Serial.println(&local, "Date: %d.%m.%y  Time: %H:%M:%S"); // format timestring

  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
