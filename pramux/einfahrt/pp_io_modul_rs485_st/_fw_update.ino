/**
    File:    _fw_update

    Author1:  P. Ideus
    Date:     10/2021
    Summary of File:

    All firmware update related functions

*/
#define DEST_FS_USES_LITTLEFS // neccessery pre define of dest. f. system
#include <ESP32-targz.h>      // includes LittleFS internally
#ifdef PLATFORM_SDK_20
#define LITTLEFS LittleFS
#endif
#include <Update.h>

enum _update_status       // possible update states
{
  UNDEFINED,
  REQUEST_SUCCESSFUL,
  REQUEST_FAILED,
  TRANSMISSION_STARTED,
  TRANSMISSION_COMPLETED,
  TRANSMISSION_FAILED,
  FILE_VERIFIED,
  FILE_CORRUPTED,
  UPDATE_COMPLETED,
  UPDATE_FAILED,
  FILE_DELETED,
  DELETE_FAILED
};

#define HASH_SIZE 16                // size of hash
#define BUFFER_SIZE 1024            // buffer size for md5 calc
#ifdef _DEBUG
#define FW_UPDATE_LOG true
#else
#define FW_UPDATE_LOG false
#endif

char last_update_file_name[128];

uint16_t num_bytes_per_payload = 0;

unsigned int file_size_received = 0;

uint16_t update_seq_number = 0;
uint16_t total_package_count = 0;

/*
   Function:  init_fw_update
   --------------------
   initialize fw_update module / check for update completed

   parameters: -

   returns:    -
*/
int init_fw_update() {
  Serial.println("init fw_update \t ");
  if (getUpdateState())
  {
    update_data = UPDATE_COMPLETED;
    Serial.println("  Restarted after update \n");
    update_data_available = true;
  }

  return 0;
}

/*
   Function:  fw_update_log
   --------------------
   logging util

   message: message to be printed

   returns:    -
*/
void fw_update_log(char * message)
{
  if (FW_UPDATE_LOG)
  {
    Serial.println(message);
  }
}

/*
   Function:  printBytes
   --------------------
   utility function to print byte arrays

   arr: byte array to be printed
   len: lenth of array (short)

   returns:    -
*/
void printBytes(byte * arr, short len)
{
  if (FW_UPDATE_LOG)
  {
    short i;
    for (i = 0; i < len; i++)
    {
      if (i > 0) Serial.printf(":");
      Serial.printf("%02X", arr[i]);
    }
    Serial.printf("\n");
  }
}


/*
   Function:  caclucalteFileHash
   --------------------
   computes the md5 Hash of given file via mbedtls lib

   fs:        Filesystem to use e.g. LittleFS
   filepath:  Path to file e.g. "/example.txt"
   res:       Pointer to char array for resulting hash


    returns:  0 - if successfull
              1 - if failed
*/
int calculateFileHash(fs::FS &fs, const char * filepath, uint8_t * res)
{
  File file = LITTLEFS.open(filepath, FILE_READ);

  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return 1;
  }

  // Initialize Hashing
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts_ret(&ctx);

  // Read file in chunks of size BUFFER_SIZE
  char buff[BUFFER_SIZE];
  size_t readSize;
  while (file.available()) {
    readSize =  file.readBytes(buff, BUFFER_SIZE);
    mbedtls_md5_update_ret(&ctx, (unsigned char *) buff, readSize);
  }

  // Calculate final hash sum
  mbedtls_md5_finish_ret(&ctx, res);

  // Cleanup
  file.close();
  mbedtls_md5_free(&ctx);

  return 0;
}

/*
   Function:  updateFromFS
   --------------------
   General update entry point. Do update + Reboot.

   fs:        Filesystem to use e.g. LittleFS
   filepath:  Path to file
   res:       Pointer to char array for resulting hash


    returns:  void
*/
void updateFromFS(fs::FS &fs, char * filename)
{
  File updateBin = fs.open(filename);
  bool success = false;
  if (updateBin)
  {
    if (updateBin.isDirectory())
    {
      Serial.println("Error, update.bin is not a file");
      updateBin.close();
      return;
    }
    size_t updateSize = updateBin.size();

    if (updateSize > 0)
    {
      //check for compressed File
      if (strstr(updateBin.name(), "gz"))
      {
        Serial.println("Try to start compressed update...");
        success = performUpdateCompressed(filename);
      }
      else
      {
        Serial.println("Try to start normal update...");
        success = performUpdate(updateBin, updateSize);
      }

    }
    else
    {
      Serial.println("Error, file is empty");
    }

    updateBin.close();

    if (success)
    {
      //fs.remove(filename);
      storeUpdateState(true);
      rebootEspWithReason("Rebooting after successful Update...");
    }
    else
    {
      update_data = UPDATE_FAILED;
      update_data_available = true;
    }
  }
  else
  {
    Serial.printf("Could not load %s from file root\n", filename);
  }
}


void rebootEspWithReason(String reason) {
  Serial.println(reason);
  delay(1000);
  ESP.restart();
}

/*
   Function:  performUpdateCompressed
   --------------------
   perform the actual update from a given compressed file.

   filepath:  path to file

   returns:   true -  if successful
              false - if failed
*/
bool performUpdateCompressed(char * filepath)
{

  GzUnpacker *GZUnpacker = new GzUnpacker();

  GZUnpacker->haltOnError( true );                                              // stop on fail (manual restart/reset required)
  GZUnpacker->setupFSCallbacks( targzTotalBytesFn, targzFreeBytesFn );          // prevent the partition from exploding, recommended
  GZUnpacker->setGzProgressCallback( BaseUnpacker::defaultProgressCallback );   // targzNullProgressCallback or defaultProgressCallback
  GZUnpacker->setLoggerCallback( BaseUnpacker::targzPrintLoggerCallback  );     // gz log verbosity

  if ( ! GZUnpacker->gzUpdater(tarGzFS, filepath, U_FLASH, false) )
  {
    Serial.printf("gzUpdater failed with return code #%d\n", GZUnpacker->tarGzGetError() );
    return false;
  }
  else
  {
    return true;
  }
}

/*
   Function:  performUpdate
   --------------------
   perform the actual update from a given file.

   updateSource:  file stream
   updateSize:    actual size of the file

   returns:   true -  if successful
              false - if failed
*/
bool performUpdate(Stream &updateSource, size_t updateSize)
{
  if (Update.begin(updateSize))
  {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize)
    {
      Serial.println("Written : " + String(written) + " successfully");
    }
    else
    {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end())
    {
      Serial.println("OTA done!");
      if (Update.isFinished())
      {
        Serial.println("Update successfully completed. Rebooting.");
        return true;
      }
      else
      {
        Serial.println("Update not finished? Something went wrong!");
      }
    }
    else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
    }

  }
  else
  {
    Serial.println("Not enough space to begin OTA");
  }
  return false;
}
