
//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

// initi filesystem
void init_filesystem()
{
  Serial.println("init filesystem");

  if (!SD_MMC.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD_MMC card attached");
    return;
  }

  Serial.print("SD_MMC Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

  listDir(SD_MMC, "/", 0);


  Serial.println("init filesystem done");
}

// list filesystem

void listDir(fs::FS &fs, const char * dirname, uint8_t levels)
{
  char *result = NULL;
  listDir(fs, dirname, levels, result);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels, char *result)
{
  char msg[128];

  File root = fs.open(dirname);
  if (!root) {
    log("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    log(" - not a directory");
    return;
  }

  if (NULL !=result)
  {
    sprintf(result, "");
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      sprintf(msg, "DIR: %s", file.name());
      log(msg);
      if (NULL != result)
      {
        sprintf(result, "%s\n%s", result, msg);
      }

      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      sprintf(msg, "FILE: %s - %ld", file.name(), file.size());
      log(msg);
      if (NULL != result)
      {
        sprintf(result, "%s\n%s", result, msg);
      }

    }
    file = root.openNextFile();
  }
}

void download_file(char *url, char *filename)
{
  File fw_file;
  char fw_name[64];
  char urlstring[64];

  if (strstr(filename, "/"))
  {
    sprintf(fw_name, "%s", filename);
  }
  else
  {
    sprintf(fw_name, "/%s", filename);
  }

  Serial.println("...start downloading firmware using https");
  Serial.print("   file: ");
  Serial.print(fw_name);
  Serial.print(" from: ");
  Serial.println(url);

  fw_file = SD_MMC.open(fw_name, "w");

  if (!fw_file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  sprintf(urlstring, "%s%s", url, fw_name);

  Serial.println(urlstring);
  if (https.begin(espClient1, urlstring))
  {
    https.setAuthorization("pramux", "*Pt6hGXY96q9M_PEC5msTnpR");
    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        Serial.println("Write File");
        https.writeToStream(&fw_file);
        fw_file.close();
      }
    }
    else
    {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      return;
    }

    https.end();
  }
  else
  {
    Serial.printf("[HTTPS] Unable to connect\n");
    return;
  }

  listDir(SD_MMC, "/", 0);

}

void delete_file(char *filename)
{
  File fw_file;
  char fw_name[64];
  char urlstring[64];

  if (strstr(filename, "/"))
  {
    sprintf(fw_name, "%s", filename);
  }
  else
  {
    sprintf(fw_name, "/%s", filename);
  }

  SD_MMC.remove(fw_name);

  listDir(SD_MMC, "/", 0);
}
