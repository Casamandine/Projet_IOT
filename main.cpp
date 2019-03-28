/*
 * Copyright (c) 2017, CATIE, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "mbed.h"
#include <SocketAddress.h>
#include "zest-radio-atzbrf233.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"

static AnalogIn soil_moisture(ADC_IN1);
static float air_value = 0.000244 * 3.3;
static float water_value = 0.858962 * 3.3;
static DigitalOut led1(LED1);
static DigitalOut btn(BUTTON1);

I2C i2c (I2C1_SDA, I2C1_SCL);
uint8_t lm75_adress = 0x48 << 1;
// Network interface
NetworkInterface *net;
SocketAddress add;
int arrivedcount = 0;

float getHumidity(){
	return ((soil_moisture.read() * 3.3) - air_value) * 100.0 / (water_value - air_value);
}

float getTemperature(){
	char cmd[2];
	cmd[0] = 0x00;
	i2c.write(lm75_adress, cmd, 1);
	i2c.read(lm75_adress, cmd, 2);
	return ((cmd[0] << 8 | cmd[1] ) >> 7) * 0.5;
}

void togleLight(MQTT::MessageData& md){
	char temp[20];
	strncpy(temp, (char *) md.message.payload, md.message.payloadlen);

	//allumer ou éteindre la led en fonction de l'état du bouton
	if (strcmp((const char *) temp, "ON") == 0) {
		led1 = !led1;
		printf("light On");
	} else {
		led1 = !led1;
		printf("light Off");
	}
}

int main() {
    char *buffer = new char[5];
    nsapi_size_or_error_t result;

    printf("Starting \n");

    // Add the border router DNS to the DNS table
    nsapi_addr_t new_dns = {
        NSAPI_IPv6,
        { 0xfd, 0x9f, 0x59, 0x0a, 0xb1, 0x58, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x01 }
    };
    nsapi_dns_add_server(new_dns);

    // Get default Network interface (6LowPAN)
    net = NetworkInterface::get_default_instance();
    if (!net) {
        printf("Error! No network inteface found.\n");
        return 0;
    }

    // Connect 6LowPAN interface
    result = net->connect();
    if (result != 0) {
        printf("Error! net->connect() returned: %d\n", result);
        return result;
    }

    float version = 0.6;
    char* topicTemp = "projet_iot/feeds/temperature";
    char* topicHumi = "projet_iot/feeds/humidite";
    char* topicLight = "projet_iot/feeds/light";


    MQTTNetwork mqttNetwork(net);

    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    const char* hostname = "io.adafruit.com";
    int port = 1883;
    printf("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc);

    printf("Connected \n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Projet_iot";
    data.username.cstring = "projet_iot";
    data.password.cstring = "a154302bfc3f4e2080984a586c6d6660";
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);

    if ((rc = client.subscribe(topicLight, MQTT::QOS0, togleLight)) != 0)
    	printf("rc from MQTT subscribe is %d\r\n", rc);


    MQTT::Message message;

    while(true) {

		// QoS 0
		char buf[100];
		sprintf(buf, "%f\r\n", getTemperature());
		message.qos = MQTT::QOS0;
		message.retained = false;
		message.dup = false;
		message.payload = (void*)buf;
		message.payloadlen = strlen(buf)+1;
		rc = client.publish(topicTemp, message);

		sprintf(buf, "%f\r\n", getHumidity());
		message.qos = MQTT::QOS0;
		message.retained = false;
		message.dup = false;
		message.payload = (void*)buf;
		message.payloadlen = strlen(buf)+1;
		rc = client.publish(topicHumi, message);

		client.yield(100);

		printf("Humidity : %f % \n", getHumidity());
		printf("Temperature : %f °C \n", getTemperature());
		wait_ms(4000);
    }

    printf("Stop \n");

    mqttNetwork.disconnect();

    printf("Version %.2f: finish \n", version);

    return 0;
}
