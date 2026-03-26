#include <WiFiClient.h>
#include <WiFiServer.h>

struct ocr_read
{
    char plate[32];
    char ocrscore[32];
    char nread[32];
    char char_width[32];
    char char_height[32];
    bool new_read = false;
    bool handled = false;
    time_t read_timestamp;
};

const uint ServerPort = 53365;
WiFiServer Server(ServerPort);
WiFiClient RemoteClient;
uint8_t ReceiveBuffer[256];
uint8_t last_ocr_read_len = 0;
bool ocr_data_available = false;

ocr_read new_reading;
ocr_read new_kfz_plate;

void CheckForConnections()
{
    // RemoteClient = Server.client();
    if (Server.hasClient())
    {
        // If we are already connected to another computer,
        // then reject the new connection. Otherwise accept
        // the connection.
        // Serial.println("Got A new Client");

        if (RemoteClient.connected())
        {
            Serial.println("Connection rejected");
            RemoteClient.stop();
            // RemoteClient.setTimeout(500);
            //  Server.available().stop();
            RemoteClient = Server.available();
        }
        else
        {

            // Serial.println("Connection accepted");
            RemoteClient = Server.available();
        }
    }
}

void EchoReceivedData()
{
    while (RemoteClient.connected() && RemoteClient.available())
    {
        memset(ReceiveBuffer, 0x0, 256);
        int Received = RemoteClient.read(ReceiveBuffer, sizeof(ReceiveBuffer));
        char tmp_log[128];
        sprintf(tmp_log, "READ FROM OCR - LEN: %i\n", Received);
        log(tmp_log);

        last_ocr_read_len = Received;
        if (Received)
        {
            ocr_data_available = true;
        }
    }
}

// parse aufgrund dynamischer längen der varialen
bool parse_ReceivedData()
{
    ESP_LOGI("tatile", "%s", "Start");
    char *ReceiveBuffer_cpy = (char *)malloc(last_ocr_read_len);
    char plate[32];
    char ocrscore[32];
    char nread[32];
    char char_width[32];
    char char_height[32];
    char mqtt_channel[4];
    
    ocr_data_available = false;
    memcpy(ReceiveBuffer_cpy, (char *)ReceiveBuffer, last_ocr_read_len);
    bool got_valid_data = false;
    char *cmd_seq;
    char *cmd_seq_end;
    cmd_seq = strtok_r(ReceiveBuffer_cpy, ";", &cmd_seq_end);
    // Serial.println(cmd_seq);

    while (cmd_seq != NULL)
    {
        ESP_LOGI("tatile", "%s", "while spam?");
        char *key_end;
        char *key, *value;
        key = strtok_r(cmd_seq, "=", &key_end);
        value = strtok_r(NULL, "=", &key_end);

        if (value != NULL)
        {
            got_valid_data = true;
            if (strcmp(key, "plate") == 0)
            {
                sprintf(plate, "%s", value);
                // Serial.println(plate);
            }
            if (strcmp(key, "ocrscore") == 0)
            {
                sprintf(ocrscore, "%s", value);
                // Serial.println(ocrscore);
            }
            if (strcmp(key, "nread") == 0)
            {
                sprintf(nread, "%s", value);
                // Serial.println(nread);
            }
            if (strcmp(key, "char_width") == 0)
            {
                sprintf(char_width, "%s", value);
                // Serial.println(char_width);
            }
            if (strcmp(key, "char_height") == 0)
            {
                sprintf(char_height, "%s", value);
                // Serial.println(char_height);
            }
            if (strcmp(key, "channel") == 0)
            {
                sprintf(mqtt_channel, "%s", value);
            }
        }
        else
        {
            Serial.printf("empty value... Key: %s\n", key);
        }

        cmd_seq = strtok_r(NULL, ";", &cmd_seq_end);
    }

    if (got_valid_data)
    {
        sprintf(new_reading.plate, "%s", plate);
        sprintf(new_reading.ocrscore, "%s", ocrscore);
        sprintf(new_reading.nread, "%s", nread);
        sprintf(new_reading.char_height, "%s", char_height);
        sprintf(new_reading.char_width, "%s", char_width);
    }
    else
    {
        sprintf(new_reading.plate, "%s", "NULL");
        sprintf(new_reading.ocrscore, "%s", "NULL");
        sprintf(new_reading.nread, "%s", "NULL");
        sprintf(new_reading.char_height, "%s", "NULL");
        sprintf(new_reading.char_width, "%s", "NULL");
    }
    new_reading.read_timestamp = millis();

    new_reading.new_read = true;
    ocr_data_available = false;
    if (last_ocr_read_len > 255)
    {
        Serial.println("!!! receved multiple reads at once !!!");
        free(cmd_seq);
        free(cmd_seq_end);
        free(ReceiveBuffer_cpy);
        return 0;
    }
    ESP_LOGI("tatile", "%s", "readyto send");
    char topic[128];
    char payload[256];
    sprintf(topic, "%s/%s/channel/%s/out/plate_reader", mqtt_topic_praefix, mqtt_device_serial, mqtt_channel);
    sprintf(payload, "%s", ReceiveBuffer);
    publish(topic, payload, false);
    vTaskDelay(20);
    free(cmd_seq);
    free(cmd_seq_end);
    free(ReceiveBuffer_cpy);
    ESP_LOGI("tatile", "%s", "returning");
    return 1;
}

int init_OCR()
{
    Server.begin();
    return 1;
}

void task_ocr(void *param)
{
    Serial.println("Task: ocr Created");
    bool delete_old_plate = false;
    for (;;)
    {
        CheckForConnections();
        EchoReceivedData();

        if (ocr_data_available)
        {
            bool parsed_ocr_data = parse_ReceivedData();
            if (parsed_ocr_data)
            {
                // extern statemachine Menu_SM;
                new_kfz_plate = new_reading;
                new_kfz_plate.new_read = true;
                delete_old_plate = true;
            }
        }
        if (delete_old_plate && (millis() - new_kfz_plate.read_timestamp > 20000))
        {
            delete_old_plate = false;
            new_kfz_plate.new_read = false;
            sprintf(new_kfz_plate.plate, "NULL");
            Serial.println("deleting old plate by timestamp");
        }
        vTaskDelay(10);
    }
}
