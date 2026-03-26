/**
    File:    _st_fs

    Author1:  P. Ideus
    Date:     10/2021
    Summary of File:

    All filesystem related functions.
    (LITTLEFS)
*/
#include "FS.h"
#define FORMAT_LITTLEFS_IF_FAILED true

File update_file;                     // global file handle
uint32_t calc_file_size;              // calculated file size

/*
   Function:  init_fs
   --------------------
   initialize LITTLEFS filesystem. Format if not exists

    returns:  0 - if successfull
              1 - if failed
*/
int init_fs()
{
  Serial.println("init filesystem");
  if (!LITTLEFS.begin(FORMAT_LITTLEFS_IF_FAILED))
  {
    Serial.println("LITTLEFS Mount Failed");
    return 1;
  }
  listDir("/", 3);
  freeSpace();
  return 0;
}

/*
   Function:  correctFilepath
   --------------------
   initialize LITTLEFS filesystem. Format if not exists

   filepath:  path to file

   returns: overwrites filepath with sanitized path

*/
void correctFilepath(char * filepath)
{
  char validPath[128];
  if (strstr(filepath, "/"))
  {
    sprintf(validPath, "%s", filepath);
  }
  else
  {
    sprintf(validPath, "/%s", filepath);
  }
  strcpy(filepath, validPath);
}

/*
   Function:  freeSpace
   --------------------
   util for printing available space

   returns: -

*/
void freeSpace()
{
  size_t totalBytes = LITTLEFS.totalBytes();
  size_t usedBytes = LITTLEFS.usedBytes();

  Serial.printf("totalBytes:%zu   usedBytes: %zu   free: %zu \n", totalBytes, usedBytes, totalBytes - usedBytes);
}

/*
   Function:  isSufficientSpace
   --------------------
   util for checking if free space suffices

   filesize:  size to be checked

   returns: true  - if enough space
            false - if not enough space

*/
bool isSufficientSpace(uint32_t * filesize)
{
  size_t totalBytes = LITTLEFS.totalBytes();
  size_t usedBytes = LITTLEFS.usedBytes();
  return (totalBytes - usedBytes >= *filesize);
}

/*
   Function:  listDir
   --------------------
   util for listing directory

   dirname: path to directory from file root
   levels:  how deep the lisitng should go
   returns: -

*/
void listDir(const char * dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = LITTLEFS.open(dirname);
  if (!root)
  {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
  Serial.println();
}

/*
   Function:  readFile
   --------------------
   util for printing file content

   filepath: path to file

   returns: -

*/
void readFile(const char * filepath)
{
  File file = LITTLEFS.open(filepath, FILE_READ);
  while (file.available())
  {
    // Dateiinhalt Zeichen für Zeichen lesen und ausgeben
    Serial.write(file.read());
  }
  file.close();
}

/*
   Function:  deleteFile
   --------------------
   util for deleting file

   filepath: path to file

   returns: true  - if successful
            false - if failed

*/
bool deleteFile(const char * path)
{
  Serial.printf("Deleting file: %s\r\n", path);
  if (LITTLEFS.remove(path))
  {
    Serial.println("- file deleted");
    return true;
  }
  else
  {
    Serial.println("- delete failed");
    return false;
  }
}

/*
   Function:  deleteAllFiles
   --------------------
   util for deleting all files

   dirname: path to dir

   returns: -1 - if failed
            >0 - if successful (number of deletions)

*/
int deleteAllFiles(const char * dirname)
{
  File root = LITTLEFS.open(dirname);
  unsigned char total_deleted = 0;
  if (!root)
  {
    Serial.println("- failed to open directory");
    return -1;
  }
  if (!root.isDirectory())
  {
    Serial.println(" - not a directory");
    return -1;
  }
  char path[64];

  File file = root.openNextFile();
  while (file)
  {
    sprintf(path, "/%s", file.name());
    deleteFile(path);
    total_deleted++;
    file = root.openNextFile();
  }
  return total_deleted;

}

/*
   Function:  openFile
   --------------------
   open file in global handle

   path: path + filename

   returns: true  - if successful
            false - if failed

*/
bool openFile(const char * path)
{
  update_file = LITTLEFS.open(path, FILE_WRITE);
  if (!update_file)
  {
    Serial.println("- failed to open file for appending");
    return false;
  }
  calc_file_size = 0;
  return true;
}

/*
   Function:  closeFile
   --------------------
   close file in global handle

   returns: true  - if successful
            false - if failed

*/
bool closeFile()
{
  if (update_file)
  {
    update_file.close();
    return true;
  }
  return false;
}

/*
   Function:  appendFile
   --------------------
   append to file opened in global handle

   bytes:     bytes to be appended
   pos:       current file position
   num_bytes: length of bytes

   returns: true  - if successful
            false - if failed

*/
bool appendFile(const byte * bytes, uint32_t pos, const uint8_t num_bytes)
{
  uint32_t t = millis();
  bool success = false;

  if (update_file.write(bytes, num_bytes) == num_bytes )
  {
    success = true;
  }
  else
  {
    success = false;
  }

  // calculate filesize
  calc_file_size += num_bytes;

#ifdef _DEBUG
  Serial.printf("Offset: %d \t %d - %s Größe: %d %s %ld\n", pos / num_bytes_per_payload, num_bytes,
                success ? "bytes appended" : "append failed",
                calc_file_size, (pos + num_bytes) == calc_file_size ? "" : " size mismatch",
                millis() - t);
#endif

  return (pos + num_bytes) == calc_file_size ? success : false;
}
