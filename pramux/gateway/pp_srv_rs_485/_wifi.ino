
void init_WiFi()
{
  // Connect tp Wifi
  WiFi.begin(SSID, PSWD);
  WiFi.setHostname("test");


  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    log(".");
  }

  char message[64];
  sprintf(message, "WiFi connected - IP-Address: %s", WiFi.localIP().toString().c_str());

  eth = false;
  eth_connected = true;

  log(message);

  init_time();
}
