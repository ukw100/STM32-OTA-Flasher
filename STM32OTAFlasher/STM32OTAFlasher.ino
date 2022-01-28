/*----------------------------------------------------------------------------------------------------------------------------------------
 * STM32OTAFlasher.ino - STM32 OTA Flasher
 *
 * Arduino Settings: NodeMCU 1.0 
 *----------------------------------------------------------------------------------------------------------------------------------------
 * MIT License
 *
 * Copyright (c) 2022 Frank Meyer (frank@uclock.de)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include "STM32OTAFlasher.h"
#include "http.h"
#include "stm32flash.h"
#include "eepromdata.h"

#define WIFI_STATE_DISCONNECTED   0
#define WIFI_STATE_CONNECTED      1
#define WIFI_STATE_AP             2

/*----------------------------------------------------------------------------------------------------------------------------------------
 * global data:
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static int                          wifi_state = WIFI_STATE_DISCONNECTED;
static bool                         serial_swapped = false;
static WiFiServer                   telnet_server(23);

/*----------------------------------------------------------------------------------------------------------------------------------------
 * telnet_setup () - start telnet server
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static void
telnet_setup (void)
{
    telnet_server.begin();
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * telnet_loop - telnet server loop
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static void
telnet_loop (void)
{
    static WiFiClient telnet_client;
    
    if (! telnet_client || ! telnet_client.connected())
    {
        telnet_client = telnet_server.available();
    }

    if (telnet_client && telnet_client.connected())
    {
        while (telnet_client.available())
        {
            Serial.write (char (telnet_client.read ()));
        }

        Serial.flush ();

        while (Serial.available())
        {
            telnet_client.write (char (Serial.read()));
        }

        telnet_client.flush ();
    }
    else      // not connected, flush input queue
    {
        while (Serial.available())
        {
            Serial.read();
        }
    }
}

static void
services_start (void)
{
    if (! serial_swapped)
    {
        Serial.flush ();
        delay (200);
        Serial.end ();                                          // end old serial
        Serial.begin (115200, SERIAL_8E1);                      // switch to 8 bit even parity, connect to STM32
        Serial.swap ();                                         // swap UART pins to UART2: GPIO15 is now TX, GPIO13 is now RX
        serial_swapped = true;
    }

    telnet_setup ();
    http_setup ();

}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * connect to AP
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
wifi_connect (char * ssid, char * ssidkey, bool do_disconnect)
{
    int cnt;
  
    WiFi.mode(WIFI_STA);

    if (do_disconnect)
    {
        WiFi.disconnect();
        delay(100);
    }

    wifi_state = WIFI_STATE_DISCONNECTED;
    WiFi.begin(ssid, ssidkey);

    if (! serial_swapped)
    {
        Serial.print("Connecting to ");
        Serial.print(ssid);
        Serial.println(" ...");
    }

    for (cnt = 0; cnt < 20; cnt++)                                          // check 10 seconds if connected
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            break;
        }
  
        delay(500);
    }
  
    if (WiFi.status() != WL_CONNECTED)
    {
        if (! serial_swapped)
        {
            Serial.print("WiFi connection to '");
            Serial.print(ssid);
            Serial.println("' not established!");  
        }
    }
    else
    {
        if (! serial_swapped)
        {
            Serial.println("WiFi connection established.");  
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        }

        services_start ();
        wifi_state = WIFI_STATE_CONNECTED;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * setup local AP
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
wifi_ap (const char * apssid, const char * apssidkey)
{
    WiFi.mode(WIFI_AP);
  
    if (! serial_swapped)
    {
        Serial.println ("disconnecting...");
        Serial.flush ();
    }

    WiFi.disconnect();
    wifi_state = WIFI_STATE_DISCONNECTED;
    delay(100);

    if (! serial_swapped)
    {
        Serial.println ("starting ap mode...");
        Serial.flush ();
    }

    WiFi.softAP(apssid, apssidkey);

    if (! serial_swapped)
    {
        Serial.println ("begin server...");
        Serial.flush ();
        Serial.print ("AP ");
        Serial.println(apssid);
        Serial.print ("ipaddress ");
        Serial.println(WiFi.softAPIP());
    }

    wifi_state = WIFI_STATE_AP;
    services_start ();
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * global setup
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
setup (void)
{
    stm32_flash_setup ();

    Serial.begin(115200);                                     // use 115200 Baud
    EEPROM.begin(512);                                        // use 512 bytes as EEPROM
    delay(10);
    Serial.println();
    delay(500);
    Serial.println ("FIRMWARE 1.0.0");                        // print firmware version

    eeprom_read ();

#if 0 // force connect
    eeprom_flags = 0x00;
    eeprom_save_flags ();
    eeprom_commit ();
#endif

    if (eeprom_flags & EEPROM_FLAG_BOOT_AS_AP)
    {
        wifi_ap (eeprom_ap_ssid, eeprom_ap_ssidkey);
    }
    else
    {
        wifi_connect (eeprom_ssid, eeprom_ssidkey, false);
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * main loop
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
loop() 
{
    if (digitalRead(0) == LOW)
    {
        wifi_ap (EEPROM_AP_SSID_CONTENT, EEPROM_AP_SSID_KEY_CONTENT);
    }

    if (wifi_state != WIFI_STATE_DISCONNECTED)
    {
        http_loop ();
        telnet_loop ();
    }
}
