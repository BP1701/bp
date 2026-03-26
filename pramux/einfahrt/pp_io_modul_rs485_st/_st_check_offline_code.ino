const char PRAMUX_SECRET[] = "95Cd4QY4UUu2";


int check_offline_code(char *code, int &artikel, int &id, uint32_t &timestamp)
{
  char chksum[5];
  char tmpbuf[5];
  char buf[28];

  memset(chksum, 0x0, 5);
  memset(tmpbuf, 0x0, 5);
  memset(buf, 0x0, 28);

  memcpy(chksum, code + 15, 4);


  // Initialize Hashing
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts_ret(&ctx);

  // calculate an verify hash
  uint8_t res[16];
  Serial.println(sizeof(buf));
  memcpy(buf, code, 15);
  strcat(buf, PRAMUX_SECRET);
  mbedtls_md5_update_ret(&ctx, (unsigned char *) buf, 27);
  // Calculate final hash sum
  mbedtls_md5_finish_ret(&ctx, res);

  //mbedtls_md5_free(&ctx);


  Serial.printf("MD5-Hash: ");
  for (int i = 0; i < 16; i++)
  {
    Serial.printf("%02x", res[i]);
  }
  Serial.println();

  sprintf(tmpbuf, "%02x%02x", res[14], res[15]);

  Serial.print("DEBUG: ");
  Serial.println(tmpbuf);

  if (memcmp(chksum, tmpbuf, 4) != 0)
  {
    Serial.println("hash verification failed");
    Serial.printf("chksum: %s\n", chksum);
    Serial.printf("md5sum: %s\n", tmpbuf);
    artikel = 0;
    id = 0;
    timestamp = 0;
    return -1;
  }

  Serial.println("hash verification successfully");

  memset(tmpbuf, 0x0, 10);
  memcpy(tmpbuf, code, 1);
  artikel = atoi(tmpbuf);

  memset(tmpbuf, 0x0, 10);
  memcpy(tmpbuf, code + 1, 4);
  id = atoi(tmpbuf);

  memset(tmpbuf, 0x0, 10);
  memcpy(tmpbuf, code + 5, 10);
  timestamp = atol(tmpbuf);

  return 0;
}
