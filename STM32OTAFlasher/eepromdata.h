/*----------------------------------------------------------------------------------------------------------------------------------------
 * eepromdata.h - EEPROM data routines
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
#ifndef EEPROMDATA_H
#define EEPROMDATA_H

/*----------------------------------------------------------------------------------------------------------------------------------------
 * Default values
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
#define EEPROM_AP_SSID_CONTENT      "STM32Flasher"                                      // default AP SSID
#define EEPROM_AP_SSID_KEY_CONTENT  "1234567890"                                        // default AP SSID KEY, min. length is 8!

/*----------------------------------------------------------------------------------------------------------------------------------------
 * Possible flags set in eeprom_flags
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
#define EEPROM_FLAG_BOOT_AS_AP      0x01

/*----------------------------------------------------------------------------------------------------------------------------------------
 * Lengths of EEPROM values
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
#define EEPROM_MAGIC_LEN            8
#define EEPROM_VERSION_LEN          3
#define EEPROM_SSID_LEN             32
#define EEPROM_SSID_KEY_LEN         64
#define EEPROM_AP_SSID_LEN          32
#define EEPROM_AP_SSID_KEY_LEN      64
#define EEPROM_FLAGS_LEN            1

/*----------------------------------------------------------------------------------------------------------------------------------------
 * EEPROM values
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
extern char                         eeprom_ssid[EEPROM_SSID_LEN + 1];
extern char                         eeprom_ssidkey[EEPROM_SSID_KEY_LEN + 1];
extern char                         eeprom_ap_ssid[EEPROM_AP_SSID_LEN + 1];
extern char                         eeprom_ap_ssidkey[EEPROM_AP_SSID_KEY_LEN + 1];
extern uint8_t                      eeprom_flags;

/*----------------------------------------------------------------------------------------------------------------------------------------
 * EEPROM access functions
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
extern void                         eeprom_save_ssid (void);
extern void                         eeprom_save_ssidkey (void);
extern void                         eeprom_save_ap_ssid (void);
extern void                         eeprom_save_ap_ssidkey (void);
extern void                         eeprom_save_flags (void);
extern void                         eeprom_commit (void);
extern void                         eeprom_read (void);

#endif // EEPROMDATA_H
