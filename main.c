#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <linux/i2c-dev.h>  
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/gpio.h>  

#include <MQTTClient.h>
#include <cjson/cJSON.h>

#define MQTT_TOPIC_BUTTON "zigbee2mqtt/button0"
#define MQTT_TOPIC_RELAY "zigbee2mqtt/relay0"
#define MQTT_SERVER "127.0.0.1:1883"
#define MQTT_CLENT_NAME "z2m-node-control"

static volatile sig_atomic_t g_quit = false;

void sig_handler(int signum)
{
	(void) signum;
	g_quit = true;
}

int main(int argc, char **argv)
{
	int retval = 0;
	signal(SIGINT, sig_handler);

	MQTTClient client = 0;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	MQTTClient_create(&client, MQTT_SERVER, MQTT_CLENT_NAME,
			MQTTCLIENT_PERSISTENCE_NONE, NULL);
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	retval = MQTTClient_connect(client, &conn_opts);
	if (retval != MQTTCLIENT_SUCCESS) {
		fprintf(stderr, "mqtt connect failed\n");
		goto exit;
	}

	MQTTClient_subscribe(client, MQTT_TOPIC_BUTTON, 0);

	char* topic_name = NULL;
	int topic_length = 0;
	MQTTClient_message* msg_in = NULL;

	MQTTClient_message msg_out = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token = {0};
	msg_out.qos = 1;
	msg_out.retained = 0;

	while (!g_quit) {
		MQTTClient_receive(client, &topic_name, &topic_length, &msg_in, 200);
		if (msg_in != NULL) {
			cJSON *json = cJSON_ParseWithLength(
					msg_in->payload, msg_in->payloadlen);
			cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
			if (cJSON_IsString(action) && (action->valuestring != NULL)) {
				if (!strcmp(action->valuestring, "single")) {
					msg_out.payload = "{\"state_l1\":\"TOGGLE\",\"state_l2\":\"TOGGLE\"}";
					msg_out.payloadlen = strlen(msg_out.payload);
					MQTTClient_publishMessage(client, MQTT_TOPIC_RELAY "/set", &msg_out, &token);
				} else if (!strcmp(action->valuestring, "double")) {
					msg_out.payload = "{\"state_l1\":\"OFF\",\"state_l2\":\"OFF\"}";
					msg_out.payloadlen = strlen(msg_out.payload);
					MQTTClient_publishMessage(client, MQTT_TOPIC_RELAY "/set", &msg_out, &token);
				}
			}
			MQTTClient_free(topic_name);
			MQTTClient_freeMessage(&msg_in);
		}
	}

	MQTTClient_disconnect(client, 10000);
	MQTTClient_destroy(&client);

exit:
	return retval;
}
