void init_reset()
{
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
}

void do_hardreset()
{
  Serial.println("perform hardreset");
  delay(1000);
  pinMode(4, OUTPUT);
  delay(1);
  digitalWrite(4, HIGH);
  
}


char *get_uptime_string()
{

  Serial.println("Starting uptime: ");
  unsigned long i_uptime = millis();

  int d, h, m, s, ms;

  d = 0; h = 0; m = 0; s = 0; ms = 0;
  ms = i_uptime % 1000;
  s = (int) i_uptime / 1000;
  if (s > 0)
  {
    m = (int) s / 60;
    s = s - (m * 60);
  }
  if (m > 0)
  {
    h = (int) m / 60;
    m = m - (h * 60);
  }
  if (h > 0)
  {
    d = (int) h / 24;
    h = h - (d * 24);
  }

  sprintf(s_uptime, "%dd %dh %dm %ds", d, h, m, s);

  Serial.println(s_uptime);

  return s_uptime;
}
