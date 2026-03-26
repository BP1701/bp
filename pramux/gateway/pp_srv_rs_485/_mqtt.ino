SemaphoreHandle_t x_mqtt_s;
QueueHandle_t x_mqtt_q;
QueueHandle_t x_mqtt_payload_q;

struct _mqtt_message
{
  char topic[128];
  char payload[128];
  bool retained;
} mqtt_message;

struct _mqtt_payload
{
  char topic[128];
  byte payload[128];
  unsigned int length;
} mqtt_payload;

bool compare_map(char *k0, char *k1)
{
  return (strcmp(k0, k1) == 0);
}

void init_mqtt()
{
  x_mqtt_s = xSemaphoreCreateMutex();
  x_mqtt_q = xQueueCreate(40, sizeof(_mqtt_message));
  x_mqtt_payload_q = xQueueCreate(40, sizeof(_mqtt_payload));

  if (x_mqtt_s == NULL) {
    Serial.println("Mutex can not be created");
  }

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqtt_callback);
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_device_serial, MQTT_USER, MQTT_SECRET))
    {
      Serial.println("connected");
      
      // create and subscribe channels

      char topic[128];
      char payload[128];
      sprintf(topic, "%s/%s/config/in", mqtt_topic_praefix, mqtt_device_serial);
      client.publish(topic, "init", true);
      client.subscribe(topic);
      sprintf(topic, "%s/%s/config/out", mqtt_topic_praefix, mqtt_device_serial);
      client.publish(topic, "init", true);
      sprintf(topic, "%s/%s/config/state", mqtt_topic_praefix, mqtt_device_serial);
      client.publish(topic, "init", true);


      sprintf(topic, "%s/%s/config/mac-address", mqtt_topic_praefix, mqtt_device_serial);
      if (eth)
      {
        sprintf(payload, "%s", ETH.macAddress().c_str());
      }
      else
      {
        sprintf(payload, "%s", WiFi.macAddress().c_str());
      }
      client.publish(topic, payload, true);

      sprintf(topic, "%s/%s/config/ip-address", mqtt_topic_praefix, mqtt_device_serial);
      if (eth)
      {
        sprintf(payload, "%s", ETH.localIP().toString().c_str());
      }
      else
      {
        sprintf(payload, "%s", WiFi.localIP().toString().c_str());
      }
      client.publish(topic, payload, true);

      sprintf(topic, "%s/%s/config/localtime", mqtt_topic_praefix, mqtt_device_serial);
      tm local;
      getLocalTime(&local);
      strftime(payload, 64, "Date: %d.%m.%y Time: %H:%M:%S", &local);
      client.publish(topic, payload, true);

      for (int i = 0; i < NUM_DEVICES; i++)
      {
        sprintf(topic, "%s/%s/channel/%02d/in", mqtt_topic_praefix, mqtt_device_serial, i);
        client.publish(topic, "init", true);
        client.subscribe(topic);

        sprintf(topic, "%s/%s/channel/%02d/out", mqtt_topic_praefix, mqtt_device_serial, i);
        client.publish(topic, "init", true);

        sprintf(topic, "%s/%s/channel/%02d/state", mqtt_topic_praefix, mqtt_device_serial, i);
        client.publish(topic, "init", true);

        sprintf(topic, "%s/%s/channel/%02d", mqtt_topic_praefix, mqtt_device_serial, i);

        switch (devices[i].type)
        {
          case PP_SEN_RID_RS485:
            {
              client.publish(topic, "rs485-rfid-reader", true);
              break;
            }
          case PP_SEN_COM_RS485:
            {
              client.publish(topic, "rs485-combi-reader", true);
              break;
            }
          case PP_SEN_SCN_RS485:
            {
              client.publish(topic, "rs485-barcode-reader", true);
              break;
            }
          case PP_SEN_PIN_RS485:
            {
              client.publish(topic, "rs485-pinpad", true);
              break;
            }
          case PP_ACT_REL_RS485:
            {
              client.publish(topic, "rs485-relay", true);
              break;
            }
          case PP_ACT_GAT_RS485:
            {
              client.publish(topic, "rs485-gate-controller", true);
              
                for (int pmc = 0; pmc < devices[i].counter_count; pmc++)
                {
                sprintf(topic, "%s/%s/channel/%02d/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, i, pmc);
                Serial.print("Subscribe: ");
                Serial.println(topic);
                client.subscribe(topic);
                }
              
              break;
            }
          case PP_ACT_PSS_RS485:
            {
              client.publish(topic, "rs485-power-supply-station", true);
              /*
                for (int pmc = 0; pmc < devices[i].counter_count; pmc++)
                {
                sprintf(topic, "%s/%s/channel/%02d/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, i, pmc);
                Serial.print("Subscribe: ");
                Serial.println(topic);
                client.subscribe(topic);
                }
              */
              break;
            }
          case PP_64IO_RS485:
            {
              client.publish(topic, "rs485-64bit-io-modul", true);

              // if local counter store is empty subscribe counter topics to obtain valid values

              for (int cnt = 0; cnt < 32; cnt++)
              {
                if (devices[i].counter_valid[cnt] == false)
                {
                  sprintf(topic, "%s/%s/channel/%02d/io64/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, i, cnt);
                  Serial.print("Subscribe: ");
                  Serial.println(topic);
                  client.subscribe(topic);
                }
              }
              break;
            }
          case PP_SEN_CNT_INT:
            {
              client.publish(topic, "internal-input-counter", true);

              for (int pmc = 0; pmc < devices[i].counter_count; pmc++)
              {
                sprintf(topic, "%s/%s/channel/%02d/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, i, pmc);
                client.subscribe(topic);
              }
              break;
            }

          case PP_ACT_REL_INT:
            {
              client.publish(topic, "internal-relay-output", true);
              break;
            }
          case PP_SEN_CUR_R485:
            {
              client.publish(topic, "rs485-current-meter", true);
              break;
            }
          case PP_SEN_CNT_RS485:
            {
              client.publish(topic, "rs485-counter", true);
              break;
            }
          case PP_ACT_EV_CS:
            {
              client.publish(topic, "rs485-ev-charing-station", true);
              /*
                for (int pmc = 0; pmc < devices[i].counter_count; pmc++)
                {
                sprintf(topic, "%s/%s/channel/%02d/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, i, pmc);
                Serial.print("Subscribe: ");
                Serial.println(topic);
                client.subscribe(topic);
                }
              */
              break;
            }
          case PP_SEN_INP_INT:
            {
              client.publish(topic, "internal-input-io", true);
              io_cb(0, 0xff, port_exp_input_0.read_all());
              break;
            }  
          case PP_ACT_ASS_RS485:
            {
              client.publish(topic, "rs485-aqua-supply-station", true);
              /*
                for (int pmc = 0; pmc < devices[i].counter_count; pmc++)
                {
                sprintf(topic, "%s/%s/channel/%02d/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, i, pmc);
                Serial.print("Subscribe: ");
                Serial.println(topic);
                client.subscribe(topic);
                }
              */
              break;
            }                       
          case PP_PAP_GAT_RS485:
            {
              client.publish(topic, "rs485-pap-gate-controller", true);

              sprintf(topic, "%s/%s/channel/%02d/in/gate", mqtt_topic_praefix, mqtt_device_serial, i);
              client.publish(topic, "init", true);
              Serial.print("Subscribe: ");
              Serial.println(topic);
              client.subscribe(topic);
              break;
            } 
          case TYPE_NONE:
            {
              client.publish(topic, "undefinded", true);
              break;
            }
          default:
            {
              client.publish(topic, "undefinded", true);
              break;
            }

        }
        /*
          if (i < 4)
          {
          client.publish(topic, "rs485", true);
          }
          else if (i >= 4 && i < 8)
          {
          client.publish(topic, "sensor", true);
          }
          else
          {
          client.publish(topic, "actor", true);
          }
        */
      }
      mqtt_connected = true;
    }
    else
    {
      mqtt_reconnect++;
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);

      if (mqtt_reconnect > 60)
      {
        rebootEspWithReason("failed MQTT connection");
      }
    }
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length)
{
  // queue messages
  memset(&mqtt_payload, 0x0, sizeof(struct _mqtt_payload));

  strcpy(mqtt_payload.topic, topic);
  memcpy(mqtt_payload.payload, payload, length);
  mqtt_payload.length = length;


  if (xQueueSend(x_mqtt_payload_q, &mqtt_payload, 512) != pdTRUE)
  {
    Serial.println("MQTT Payload failed queueing message");
  }
}


void publish(char * topic, char *payload)
{
  publish(topic, payload, false);
}

void publish(char * topic, char *payload, bool retained)
{
  /*
    if ( xSemaphoreTake( x_mqtt_s, ( TickType_t ) 1000 ) == pdTRUE )
    {
    client.publish(topic, payload, retained);
    xSemaphoreGive(x_mqtt_s);
    }
    else
    {
    Serial.println("can not take semaphore");
    }
  */
  memset(&mqtt_message, 0x0, sizeof(struct _mqtt_message));

  strcpy(mqtt_message.topic, topic);
  strcpy(mqtt_message.payload, payload);
  mqtt_message.retained = retained;

  if (xQueueSend(x_mqtt_q, &mqtt_message, 512) != pdTRUE)
  {
    Serial.println("failed queueing message");
  }
}

void task_mqtt(void *param)
{
  for (;;)
  {
    /*
       while (wait_for_update)
       {
         Serial.println("   --> task mqtt suspended");
         x_task_mqtt_suspended = true;
         vTaskSuspend(x_mqtt_handle);
       }
    */

    if ( xSemaphoreTake( x_mqtt_s, ( TickType_t ) 1000 ) == pdTRUE )
    {
      if (!client.connected())
      {
        reconnect();
      }
      mqtt_reconnect = 0;
      client.loop();
      xSemaphoreGive(x_mqtt_s);
    }
    else
    {
      Serial.println("can not take semaphore");
    }

    if ( xQueueReceive( x_mqtt_q, &mqtt_message, (TickType_t) 10) == pdPASS)
    {
      client.publish(mqtt_message.topic, mqtt_message.payload, mqtt_message.retained);
      Serial.printf("publish message: %s %s\n", mqtt_message.topic, mqtt_message.payload);
    }

    if (millis() > uptime_timer + 60000)
    {
      uptime_timer = millis();
      char topic[128];
      char payload[128];
      sprintf(topic, "%s/%s/config/state", mqtt_topic_praefix, mqtt_device_serial);
      sprintf(payload, "Program: %s Ver %s - Uptime: %s", PROG, VER, get_uptime_string());
      publish(topic, payload);

      sprintf(topic, "%s/%s/config/localtime", mqtt_topic_praefix, mqtt_device_serial);
      tm local;
      getLocalTime(&local);
      strftime(payload, 64, "Date: %d.%m.%y Time: %H:%M:%S", &local);
      publish(topic, payload, true);

      // last_seen_message for PAP Devices as localtime
      for (int i = 0; i < NUM_DEVICES; i++)
      {
        if ((devices[i].type == PP_PAP_GAT_RS485) && (!devices[i].last_seen_message))
        {
          strftime(payload, 64, "%d-%m-%Y %H:%M:%S", &local);
          sprintf(topic, "%s/%s/channel/%02d/out/localtime", mqtt_topic_praefix, mqtt_device_serial, i);
          publish(topic, payload, true);
        }
      }
    }

    // process incomming message
    get_next_mqtt_payload();

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
