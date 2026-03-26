
void init_ethernet()
{
  WiFi.onEvent(WiFiEvent);

  //IPAddress ip_address(192, 168, 211, 90);
  //IPAddress gateway(192, 168 ,211 ,254);
  //IPAddress subnet(255, 255 ,255 ,0);
    
  //IPAddress dns1(8, 8, 8, 8);
  //IPAddress dns2(8, 8, 4, 4);
    
  ETH.begin();
  //ETH.config(ip_address, gateway, subnet, dns1, dns2);

}
void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      init_time();
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}
