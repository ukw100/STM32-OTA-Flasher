/*----------------------------------------------------------------------------------------------------------------------------------------
 * eepromdata.cpp - EEPROM data routines
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
#include <Arduino.h>
#include <EEPROM.h>
#include "eepromdata.h"

#define EEPROM_VERSION_100          "100"                                               // 1.0.0
#define EEPROM_CURRENT_VERSION      EEPROM_VERSION_100                                  // current eeprom version

/*----------------------------------------------------------------------------------------------------------------------------------------
 * data stored in EEPROM:
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
#define EEPROM_MAGIC_CONTENT        "3.141593"                                          // exact 8 characters long

#define EEPROM_MAGIC_OFFSET         (0)
#define EEPROM_VERSION_OFFSET       (EEPROM_MAGIC_OFFSET + EEPROM_MAGIC_LEN)
#define EEPROM_SSID_OFFSET          (EEPROM_VERSION_OFFSET + EEPROM_VERSION_LEN)
#define EEPROM_SSID_KEY_OFFSET      (EEPROM_SSID_OFFSET + EEPROM_SSID_LEN)
#define EEPROM_AP_SSID_OFFSET       (EEPROM_SSID_KEY_OFFSET + EEPROM_SSID_KEY_LEN)
#define EEPROM_AP_SSID_KEY_OFFSET   (EEPROM_AP_SSID_OFFSET + EEPROM_AP_SSID_LEN)
#define EEPROM_FLAGS_OFFSET         (EEPROM_AP_SSID_KEY_OFFSET + EEPROM_AP_SSID_KEY_LEN)

/*----------------------------------------------------------------------------------------------------------------------------------------
 * global data:
 *----------------------------------------------------------------------------------------------------------------------------------------
 */

static char                         eeprom_magic[EEPROM_MAGIC_LEN + 1];
static char                         eeprom_version[EEPROM_VERSION_LEN + 1];             // 101 means 1.0.1
char                                eeprom_ssid[EEPROM_SSID_LEN + 1];
char                                eeprom_ssidkey[EEPROM_SSID_KEY_LEN + 1];
char                                eeprom_ap_ssid[EEPROM_AP_SSID_LEN + 1];
char                                eeprom_ap_ssidkey[EEPROM_AP_SSID_KEY_LEN + 1];
uint8_t                             eeprom_flags;
static bool                         eeprom_changed = false;

static void
eeprom_read_entry (char * target, int offset, int len)
{
    int   i;
  
    for (i = 0; i < len; i++)
    {
        target[i] = EEPROM.read(offset + i);
    }
}

static uint8_t
eeprom_read_byte (int offset)
{
    uint8_t   value;
    value = EEPROM.read(offset);
    return value;
}

static void
eeprom_write_entry (char * src, int offset, int len)
{
    int   i;
  
    for (i = 0; i < len; i++)
    {
        EEPROM.write(offset + i, src[i]);
    }
}

static void
eeprom_write_byte (uint8_t src, int offset)
{
    EEPROM.write(offset, src);
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * save magic - save eeprom magic
 * this function is intentionally static
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static void
eeprom_save_magic (void)
{
    strncpy (eeprom_magic, EEPROM_MAGIC_CONTENT, EEPROM_MAGIC_LEN);
    eeprom_write_entry (eeprom_magic, EEPROM_MAGIC_OFFSET, EEPROM_MAGIC_LEN);
    eeprom_changed = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * save version
 * this function is intentionally static
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static void
eeprom_save_version (void)
{
    eeprom_write_entry (eeprom_version, EEPROM_VERSION_OFFSET, EEPROM_VERSION_LEN);
    eeprom_changed = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * save ssid
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
eeprom_save_ssid (void)
{
    eeprom_write_entry (eeprom_ssid, EEPROM_SSID_OFFSET, EEPROM_SSID_LEN);
    eeprom_changed = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * save ssid key
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
eeprom_save_ssidkey (void)
{
    eeprom_write_entry (eeprom_ssidkey, EEPROM_SSID_KEY_OFFSET, EEPROM_SSID_KEY_LEN);
    eeprom_changed = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * save AP ssid
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
eeprom_save_ap_ssid (void)
{
    eeprom_write_entry (eeprom_ap_ssid, EEPROM_AP_SSID_OFFSET, EEPROM_AP_SSID_LEN);
    eeprom_changed = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * save AP ssid key
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
eeprom_save_ap_ssidkey (void)
{
    eeprom_write_entry (eeprom_ap_ssidkey, EEPROM_AP_SSID_KEY_OFFSET, EEPROM_AP_SSID_KEY_LEN);
    eeprom_changed = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * save flags
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
eeprom_save_flags (void)
{
    eeprom_write_byte (eeprom_flags, EEPROM_FLAGS_OFFSET);
    eeprom_changed = true;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * commit EEPROM writes
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
eeprom_commit (void)
{
    if (eeprom_changed)
    {
        EEPROM.commit ();
        eeprom_changed = false;
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * write complete EEPROM
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static void
format_eeprom (void)
{
    strcpy (eeprom_magic, EEPROM_MAGIC_CONTENT);
    strcpy (eeprom_version, EEPROM_CURRENT_VERSION);
    strcpy (eeprom_ssid, "");
    strcpy (eeprom_ssidkey, "");
    strcpy (eeprom_ap_ssid, EEPROM_AP_SSID_CONTENT);
    strcpy (eeprom_ap_ssidkey, EEPROM_AP_SSID_KEY_CONTENT);
    eeprom_flags = EEPROM_FLAG_BOOT_AS_AP;

    eeprom_save_magic ();
    eeprom_save_version ();
    eeprom_save_ssid ();
    eeprom_save_ssidkey ();
    eeprom_save_ap_ssid ();
    eeprom_save_ap_ssidkey ();
    eeprom_save_flags ();
    eeprom_commit ();
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * read eeprom
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
eeprom_read (void)
{
    int i;
    int n_version;

    eeprom_read_entry (eeprom_magic, EEPROM_MAGIC_OFFSET, EEPROM_MAGIC_LEN);

    if (strcmp (eeprom_magic, EEPROM_MAGIC_CONTENT) != 0)
    {
        Serial.println ("Formatting EEPROM...");
        format_eeprom ();
    }
    else
    {
        eeprom_read_entry (eeprom_version, EEPROM_VERSION_OFFSET, EEPROM_VERSION_LEN);
    
        n_version = atoi (eeprom_version);
        Serial.print ("EEPROM version: ");
        Serial.println (n_version);
    
        if (n_version >= 100 && n_version < 200)                                                        // 1.0.0 to 1.9.9
        {
            eeprom_read_entry (eeprom_ssid, EEPROM_SSID_OFFSET, EEPROM_SSID_LEN);
            eeprom_read_entry (eeprom_ssidkey, EEPROM_SSID_KEY_OFFSET, EEPROM_SSID_KEY_LEN);
            eeprom_read_entry (eeprom_ap_ssid, EEPROM_AP_SSID_OFFSET, EEPROM_AP_SSID_LEN);
            eeprom_read_entry (eeprom_ap_ssidkey, EEPROM_AP_SSID_KEY_OFFSET, EEPROM_AP_SSID_KEY_LEN);
            eeprom_flags = eeprom_read_byte (EEPROM_FLAGS_OFFSET);

            Serial.print ("EEPROM ssid: ");
            Serial.println (eeprom_ssid);

            Serial.print ("EEPROM ssidkey: ");
            Serial.println (eeprom_ssidkey);

            Serial.print ("EEPROM AP ssid: ");
            Serial.println (eeprom_ap_ssid);

            Serial.print ("EEPROM AP ssidkey: ");
            Serial.println (eeprom_ap_ssidkey);

            Serial.print ("EEPROM flags: ");
            Serial.println (eeprom_flags);
        }
    }
}
