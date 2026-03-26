/**
    File:    _st_eeprom.h

    Author1:  S. Gliese
    Author2:  P. Ideus
    Date:     09/2021
    Summary of File:

    Store data in key-value pairs.
    Utilizing eeprom aka non-volatile storage.
    Ex. EEPROM.h DEPRECATED DEPRECATED

*/
#include <Preferences.h>

#define PREFERENCES_NAMESPACE "eeprom"

Preferences preferences;

/*
   Function:  init_eeprom
   --------------------
   initialize non-volatile eeprom storage

   returns:  -
*/
void init_eeprom() {
  Serial.print("init Preferences...");
  preferences.begin(PREFERENCES_NAMESPACE, false);
  Serial.printf("Free entries: %d\n", preferences.freeEntries());
}

/*
   Function:  clearAllEntries
   --------------------
   deletes all preferences entries

   returns:  -
*/
void clearAllEntries()
{
  Serial.println("Clearing all EEPROM Entries ...");
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.clear();
  preferences.end();
}

/*
   Function:  store_filesize
   --------------------
   Store filesize in preferences.
   Using first 15 chars of filename as identifier.

   identifier:  key to identify entry
   filesize:    size to be stored

   returns:   true  - if successful
              false - if failed
*/
bool store_filesize(const char * identifier, const uint32_t filesize )
{
  String s = identifier;
  s = s.substring(0, 14);
  preferences.begin(PREFERENCES_NAMESPACE, false);
  if (!preferences.putUInt(s.c_str(), filesize) == 1)
  {
    Serial.println("Could not store filesize");
    return false;
  }
  else
  {
    Serial.printf("Key: %s, Value: %d stored.\n", s.c_str(), filesize);
  }
  preferences.end();
  return true;
}

/*
   Function:  get_filesize
   --------------------
   Get filesize from preferences.
   Using first 15 chars of filename as identifier.

   identifier:  key to identify entry

   returns:   >0  - if successful
              0   - if failed
*/
uint32_t get_filesize(const char * identifier)
{
  String s = identifier;
  s = s.substring(0, 14);
  uint32_t filesize;
  preferences.begin(PREFERENCES_NAMESPACE, true);
  filesize = preferences.getUInt(s.c_str(), 0);
  preferences.end();
  return filesize;
}

/*
   Function:  store_file_hash
   --------------------
   Store filehash in preferences.
   Fixed identifier ("HASH")

   identifier:  key to identify entry

   returns:   true  - if successful
              false  - if failed
*/
bool store_file_hash(const char * identifier, const byte *  value)
{
  preferences.begin(PREFERENCES_NAMESPACE, false);
  if (preferences.putBytes(identifier, value, 16) == 0)
  {
    Serial.println("Could not store file hash");
    return false;
  }
  preferences.end();
  return true;
}

/*
   Function:  validate_file_hash
   --------------------
   Validates filehash from preferences.
   Fixed identifier ("HASH")

   hash:  hash to be verified

   returns:   true  - if successful
              false  - if failed
*/
bool validate_file_hash(byte * hash)
{
  bool match = false;
  byte storedHash[HASH_SIZE];
  preferences.begin(PREFERENCES_NAMESPACE, true);
  preferences.getBytes("HASH", storedHash, HASH_SIZE);
  match = (memcmp(storedHash, hash, HASH_SIZE) == 0);
#ifdef _DEBUG
  printBytes(hash, HASH_SIZE);
  printBytes(storedHash, HASH_SIZE);
#endif
  preferences.end();
  return match;
}

/*
   Function:  storeUpdateState
   --------------------
   Store update state in preferences.
   Fixed identifier ("UPDATED")

   state:  state of update

   returns: -
*/
void storeUpdateState(bool state)
{
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.putBool("UPDATED", state);
  preferences.end();
}

/*
   Function:  storeUpdateState
   --------------------
   Get stored state from preferences.
   Reset if true.
   Fixed identifier ("UPDATED")

   returns: last stored state in preferences
*/
bool getUpdateState()
{
  bool last_state = false;
  preferences.begin(PREFERENCES_NAMESPACE, true);
  last_state = preferences.getBool("UPDATED", false);
  if (last_state)
  {
    preferences.putBool("UPDATED", false);
  }
  return last_state;
}
