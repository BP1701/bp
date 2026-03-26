void log(String logmessage)
{
  log(logmessage, 0);
}

void log(String logmessage, int num)
{
 #ifdef _DEBUG
  Serial.println(logmessage);
 #endif

  /*
  if (0 != sockets_connected)
  {
    webSocket.sendTXT(num, logmessage);
  }
  */
}
