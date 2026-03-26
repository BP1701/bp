void update_firmaware(char *url, char *filename)
{
  char fw_name[64];
  if (strstr(filename, "/"))
  {
    sprintf(fw_name, "%s", filename);
  }
  else
  {
    sprintf(fw_name, "/%s", filename);
  }


  // Suspend other Thread
  Serial.println("Suspend threads ...");
  
  wait_for_update = true;
  
  Serial.println(" finish");


  download_file(url, filename);
  updateFromFS(SD_MMC, fw_name);


  vTaskResume(x_rs485_handle);
  vTaskResume(x_mqtt_handle);

}

// perform the actual update from a given stream
void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Written : " + String(written) + " successfully");
    }
    else {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end())
    {
      Serial.println("OTA done!");
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting.");
        rebootEspWithReason("reboot");
      }
      else
      {
        Serial.println("Update not finished? Something went wrong!");
      }
    }
    else
    {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
    }

  }
  else
  {
    Serial.println("Not enough space to begin OTA");
  }
}

void updateFromFS(fs::FS &fs, char * filename)
{
  File updateBin = fs.open(filename);
  if (updateBin)
  {
    if (updateBin.isDirectory())
    {
      Serial.printf("Error, %s is not a file\n", filename);
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0)
    {
      Serial.println("Try to start update");
      performUpdate(updateBin, updateSize);
    }
    else {
      Serial.println("Error, file is empty");
    }

    updateBin.close();

    // whe finished remove the binary from sd card to indicate end of the process
    fs.remove(filename);
  }
  else
  {
    Serial.printf("Could not load %s from sd root\n", filename);
  }
}

void rebootEspWithReason(String reason)
{
  Serial.println(reason);
  delay(1000);
  do_hardreset();
  //ESP.restart();
}
