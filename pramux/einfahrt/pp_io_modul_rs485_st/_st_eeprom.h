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

bool store_config(_device_config cfg)
{
  uint8_t buffer[sizeof(_device_config)];

  memcpy(buffer, _device_config, sizeof(_device_config));

  crc8_be(0, buffer, sizeof(_device_config));
}
