/*----------------------------------------------------------------------------------------------------------------------------------------
 * stm32flash.cpp - flash STM32 via Bootloader API
 *
 * GPIO pins
 *   STM32    NodeMCU
 *   RST      D2 GPIO 4
 *   BOOT0    D1 GPIO 5
 *   UART-TX  D7 GPIO 13 RXD2
 *   UART-RX  D8 GPIO 15 TXD2
 * 
 *----------------------------------------------------------------------------------------------------------------------------------------
 * MIT License
 *
 * Copyright (c) 2017-2022 Frank Meyer (frank@uclock.de)
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
#include <string.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include "http.h"
#include <LittleFS.h>

#if 0 // yet not used
#include <ESP8266httpUpdate.h>
#include "httpclient.h"
#endif

#define hex2toi(pp)                         htoi(pp, 2)

#define STM32_RESET_PIN                     4
#define STM32_BOOT0_PIN                     5
#define STM32_TX_PIN                        13      // RXD2
#define STM32_RX_PIN                        15      // RXD2

#define STM32_BEGIN                         0x7F
#define STM32_ACK                           0x79
#define STM32_NACK                          0x1F

#define STM32_CMD_GET                       0x00    // gets the version and the allowed commands supported by the current version of the bootloader
#define STM32_CMD_GET_VERSION               0x01    // gets the bootloader version and the Read Protection status of the Flash memory
#define STM32_CMD_GET_ID                    0x02    // gets the chip ID
#define STM32_CMD_READ_MEMORY               0x11    // reads up to 256 bytes of memory starting from an address specified by the application
#define STM32_CMD_GO                        0x21    // jumps to user application code located in the internal Flash memory or in SRAM
#define STM32_CMD_WRITE_MEMORY              0x31    // writes up to 256 bytes to the RAM or Flash memory starting from an address specified by the application
#define STM32_CMD_ERASE                     0x43    // erases from one to all the Flash memory pages
#define STM32_CMD_EXT_ERASE                 0x44    // erases from one to all the Flash memory pages using two byte addressing mode
#define STM32_CMD_WRITE_PROTECT             0x63    // enables the write protection for some sectors
#define STM32_CMD_WRITE_UNPROTECT           0x73    // disables the write protection for all Flash memory sectors
#define STM32_CMD_READOUT_PROTECT           0x82    // enables the read protection
#define STM32_CMD_READOUT_UNPROTECT         0x92    // disables the read protection

#define STM32_INFO_BOOTLOADER_VERSION_IDX   0       // Bootloader version (0 < Version < 255), example: 0x10 = Version 1.0
#define STM32_INFO_GET_CMD_IDX              1       // 0x00 – Get command
#define STM32_INFO_GET_VERSION_CMD_IDX      2       // 0x01 – Get Version and Read Protection Status
#define STM32_INFO_GET_ID_CMD_IDX           3       // 0x02 – Get ID
#define STM32_INFO_READ_MEMORY_CMD_IDX      4       // 0x11 – Read Memory command
#define STM32_INFO_GO_CMD_IDX               5       // 0x21 – Go command
#define STM32_INFO_WRITE_MEMORY_CMD_IDX     6       // 0x31 – Write Memory command
#define STM32_INFO_ERASE_CMD_IDX            7       // 0x43 or 0x44 - Erase command or Extended Erase command
#define STM32_INFO_WRITE_PROTECT_CMD_IDX    8       // 0x63 – Write Protect command
#define STM32_INFO_WRITE_UNPROTECT_CMD_IDX  9       // 0x73 – Write Unprotect command
#define STM32_INFO_READOUT_PROTECT          10      // 0x82 – Readout Protect command
#define STM32_INFO_READOUT_UNPROTECT        11      // 0x92 – Readout Unprotect command
#define STM32_INFO_SIZE                     12      // number of bytes in INFO array

static uint8_t                              bootloader_info[STM32_INFO_SIZE];

#define STM32_VERSION_BOOTLOADER_VERSION    0       // Bootloader version (0 < Version < 255), example: 0x10 = Version 1.0
#define STM32_VERSION_OPTION_BYTE1          1       // option byte 1
#define STM32_VERSION_OPTION_BYTE2          2       // option byte 2
#define STM32_VERSION_SIZE                  3       // number of byted in VERSION array

// yet not used
// static uint8_t                              bootloader_version[STM32_VERSION_SIZE];

#define STM32_ID_BYTE1                      0       // production id byte 1
#define STM32_ID_BYTE2                      1       // production id byte 2
#define STM32_ID_SIZE                       2       // number of bytes in ID array

// yet not used
// static uint8_t                              bootloader_id[STM32_ID_SIZE];

#define STM32_BUFLEN                        256
static uint8_t                              stm32_buf[STM32_BUFLEN + 1];                        // one more byte for checksum

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * hex to integer
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
uint16_t
htoi (char * buf, uint8_t max_digits)
{
    uint8_t     i;
    uint8_t     x;
    uint16_t    sum = 0;

    for (i = 0; i < max_digits && *buf; i++)
    {
        x = buf[i];

        if (x >= '0' && x <= '9')
        {
            x -= '0';
        }
        else if (x >= 'A' && x <= 'F')
        {
            x -= 'A' - 10;
        }
        else if (x >= 'a' && x <= 'f')
        {
            x -= 'a' - 10;
        }
        else
        {
            x = 0;
        }
        sum <<= 4;
        sum += x;
    }

    return (sum);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * poll a character from serial
 * Returns -1 on errror
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
int
stm32_serial_poll (unsigned long timeout, int log_error)
{
    unsigned long    curtime = millis ();
    unsigned int ch;

    Serial.flush ();

    while (Serial.available() <= 0)
    {
        yield ();

        if (millis () - curtime >= timeout)
        {
            if (log_error)
            {
                http_send_FS ("Timeout<BR>\r\n");
            }
            return -1;
        }
    }

    ch = Serial.read ();
    return (int) ch;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * wait for ACK
 * Returns -1 on error
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static int
wait_for_ack (long timeout, int show_errors)
{
    int ch;

    if ((ch = stm32_serial_poll(timeout, 0)) < 0)
    {
        if (show_errors)
        {
            http_send_FS ("timeout, no ACK<BR>\r\n");
        }

        return -1;
    }
    else if (ch != STM32_ACK)
    {
        if (show_errors)
        {
            char buf[32];
            http_send_FS ("no ACK: ");
            sprintf (buf, "(0x%02x)<BR>\r\n", ch);
            http_send (buf);
        }

        return -1;
    }
    return 0;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * write a bootloader command to STM32
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
stm32_write_cmd (uint8_t cmd)
{
    while (Serial.available())
    {
        Serial.read();
    }
    stm32_buf[0] = cmd;
    stm32_buf[1] = ~cmd;
    Serial.write (stm32_buf, 2);
    Serial.flush ();
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: GET
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static int
stm32_get (uint8_t * buf, int maxlen)
{
    int i;
    int ch;
    int n_bytes;

    stm32_write_cmd (STM32_CMD_GET);

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("Command GET failed<BR>\r\n");
        return -1;
    }

    n_bytes = stm32_serial_poll (1000, 1);

    if (n_bytes < 0)
    {
        return -1;
    }

    n_bytes++;

    for (i = 0; i < n_bytes; i++)
    {
        ch = stm32_serial_poll (1000, 1);

        if (ch < 0)
        {
            return -1;
        }

        if (i < maxlen)
        {
            buf[i] = ch;
        }
    }

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("Command GET failed<BR>\r\n");
        return -1;
    }
    return n_bytes;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: GET VERSION
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#if 0 // yet not used
static int
stm32_get_version (uint8_t * buf, int maxlen)
{
    int i;
    int ch;
    int n_bytes;

    stm32_write_cmd (bootloader_info[STM32_INFO_GET_VERSION_CMD_IDX]);

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("Command GET VERSION failed<BR>\r\n");
        return -1;
    }

    n_bytes = 3;

    for (i = 0; i < n_bytes; i++)
    {
        ch = stm32_serial_poll (1000, 1);

        if (ch < 0)
        {
            return -1;
        }

        if (i < maxlen)
        {
            buf[i] = ch;
        }
    }

    if (wait_for_ack (1000, 1) < 0)
    {
        return -1;
    }

    return n_bytes;
}
#endif // 0

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: GET ID
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#if 0 // yet not used
static int
stm32_get_id (uint8_t * buf, int maxlen)
{
    int i;
    int ch;
    int n_bytes;

    stm32_write_cmd (bootloader_info[STM32_INFO_GET_ID_CMD_IDX]);

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("Command GET ID failed<BR>\r\n");
        return -1;
    }

    n_bytes = stm32_serial_poll (1000, 1);

    if (n_bytes < 0)
    {
        return -1;
    }

    n_bytes++;

    for (i = 0; i < n_bytes; i++)
    {
        ch = stm32_serial_poll (1000, 1);

        if (ch < 0)
        {
            return -1;
        }

        if (i < maxlen)
        {
            buf[i] = ch;
        }
    }

    if (wait_for_ack (1000, 1) < 0)
    {
        return -1;
    }
    return n_bytes;
}
#endif // 0

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: READ MEMORY
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static int
stm32_read_memory (uint32_t address, unsigned int len)
{
    unsigned int  i;
    int           ch;

    stm32_write_cmd (bootloader_info[STM32_INFO_READ_MEMORY_CMD_IDX]);

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("Command READ MEMORY failed<BR>\r\n");
        return -1;
    }

    stm32_buf[0] = (address >> 24) & 0xFF;
    stm32_buf[1] = (address >> 16) & 0xFF;
    stm32_buf[2] = (address >>  8) & 0xFF;
    stm32_buf[3] = address & 0xFF;
    stm32_buf[4] = stm32_buf[0] ^ stm32_buf[1] ^ stm32_buf[2] ^ stm32_buf[3];

    Serial.write (stm32_buf, 5);
    Serial.flush ();

    if (wait_for_ack (1000, 1) < 0)
    {
        char logbuf[64];
        sprintf (logbuf, "%02x %02x %02x %02x (%02x)", stm32_buf[0], stm32_buf[1], stm32_buf[2], stm32_buf[3], stm32_buf[4]);
        http_send_FS ("READ MEMORY: address ");
        http_send (logbuf);
        http_send_FS (" failed<BR>\r\n");
        return -1;
    }

    stm32_buf[0] = len - 1;
    stm32_buf[1] = ~stm32_buf[0];
    Serial.write (stm32_buf, 2);
    Serial.flush ();

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("READ MEMORY: length failed<BR>\r\n");
        return -1;
    }

    for (i = 0; i < len; i++)
    {
        ch = stm32_serial_poll (1000, 1);

        if (ch < 0)
        {
            return -1;
        }

        if (i < STM32_BUFLEN)
        {
            stm32_buf[i] = ch;
        }
    }

    return len;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: GO
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#if 0 // yet not used
static int
stm32_go (uint32_t address)
{
    stm32_write_cmd (bootloader_info[STM32_INFO_GO_CMD_IDX]);

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("Command GO failed<BR>\r\n");
        return -1;
    }

    stm32_buf[0] = (address >> 24) & 0xFF;
    stm32_buf[1] = (address >> 16) & 0xFF;
    stm32_buf[2] = (address >>  8) & 0xFF;
    stm32_buf[3] = address & 0xFF;
    stm32_buf[4] = stm32_buf[0] ^ stm32_buf[1] ^ stm32_buf[2] ^ stm32_buf[3];

    Serial.write (stm32_buf, 5);
    Serial.flush ();

    if (wait_for_ack (1000, 1) < 0)
    {
        return -1;
    }
    return 0;
}
#endif // 0

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: WRITE MEMORY
 *
 * len must be a multiple of 4!
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static int
stm32_write_memory (uint8_t * image, uint32_t address, unsigned int len)
{
    uint8_t       sum;
    unsigned int  i;

    if (len > 256)
    {
        return -1;
    }

    stm32_write_cmd (bootloader_info[STM32_INFO_WRITE_MEMORY_CMD_IDX]);

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("Command WRITE MEMORY failed<BR>\r\n");
        return -1;
    }

    stm32_buf[0] = (address >> 24) & 0xFF;
    stm32_buf[1] = (address >> 16) & 0xFF;
    stm32_buf[2] = (address >>  8) & 0xFF;
    stm32_buf[3] = address & 0xFF;
    stm32_buf[4] = stm32_buf[0] ^ stm32_buf[1] ^ stm32_buf[2] ^ stm32_buf[3];

    Serial.write (stm32_buf, 5);
    Serial.flush ();

    if (wait_for_ack (1000, 1) < 0)
    {
        char logbuf[64];
        sprintf (logbuf, "%02x %02x %02x %02x (%02x)", stm32_buf[0], stm32_buf[1], stm32_buf[2], stm32_buf[3], stm32_buf[4]);
        http_send_FS ("WRITE MEMORY: address ");
        http_send (logbuf);
        http_send_FS (" failed<BR>\r\n");
        return -1;
    }

    stm32_buf[0] = len - 1;
    sum = stm32_buf[0];
    Serial.write (stm32_buf, 1);
    Serial.flush ();

    for (i = 0; i < len; i++)
    {
        sum ^= image[i];
    }

    Serial.write (image, len);
    stm32_buf[0] = sum;
    Serial.write (stm32_buf, 1);
    Serial.flush ();

    if (wait_for_ack (1000, 1) < 0)
    {
        char logbuf[64];
        sprintf (logbuf, "%02x %02x %02x %02x", stm32_buf[0], stm32_buf[1], stm32_buf[2], stm32_buf[3]);
        http_send_FS ("WRITE MEMORY: data at address ");
        http_send (logbuf);
        http_send_FS ("failed<BR>\r\n");
        return -1;
    }
    return 0;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: WRITE UNPROTECT
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static int
stm32_write_unprotect (void)
{
    stm32_write_cmd (bootloader_info[STM32_INFO_WRITE_UNPROTECT_CMD_IDX]);

    if (wait_for_ack (1000, 1) < 0)        // 1st
    {
        http_send_FS ("Command WRITE UNPROTECT (1st) failed<BR>\r\n");
        return -1;
    }

    if (wait_for_ack (1000, 1) < 0)        // 2nd
    {
        http_send_FS ("Command WRITE UNPROTECT (2nd) failed<BR>\r\n");
        return -1;
    }
    return 0;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: ERASE
 *
 * n_pages == 0: global erase
 * 1 <= n_pages <= 255: erase N pages
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static int
stm32_erase (uint8_t * pagenumbers, uint16_t n_pages)
{
    int     i;
    uint8_t real_pages;
    uint8_t sum;

    if (n_pages > 256)
    {
        return -1;
    }

    stm32_write_cmd (bootloader_info[STM32_INFO_ERASE_CMD_IDX]);

    if (wait_for_ack (1000, 1) < 0)
    {
        return -1;
    }

    real_pages = n_pages;
    n_pages--;                                      // 0x0000 -> 0xFFFF: mass erase
    n_pages &= 0xFF;                                // 0xFFFF -> 0xFF

    stm32_buf[0] = n_pages;
    Serial.write (stm32_buf, 1);

    if (real_pages == 0)
    {
        sum = ~stm32_buf[0];
    }
    else
    {
        sum = stm32_buf[0];

        for (i = 0; i < real_pages; i++)
        {
            stm32_buf[0] = pagenumbers[i];
            Serial.write (stm32_buf, 1);
            sum ^= pagenumbers[i];
        }
    }

    stm32_buf[0] = sum;
    Serial.write (stm32_buf, 1);
    Serial.flush ();

    if (wait_for_ack (35000, 1) < 0)
    {
        return -1;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * STM32 bootloader command: EXT ERASE
 *
 * n_pages == 0x0000: global mass erase
 * n_pages == 0xFFFF: bank 1 mass erase
 * n_pages == 0xFFFE: bank 2 mass erase
 * 1 <= n_pages < 0xFFF0: erase N pages
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
static int
stm32_ext_erase (uint16_t * pagenumbers, uint16_t n_pages)
{
    int         i;
    uint16_t    real_pages;
    uint8_t     sum;

    stm32_write_cmd (bootloader_info[STM32_INFO_ERASE_CMD_IDX]);

    if (wait_for_ack (1000, 1) < 0)
    {
        http_send_FS ("Command EXT ERASE failed<BR>\r\n");
        return -1;
    }

    real_pages = n_pages;
    n_pages--;                                                          // global erase: 0x0000 -> 0xFFFF!

    if (n_pages == 0xFFFF || n_pages == 0xFFFE || n_pages == 0xFFFD)
    {                                                                   // -> 0xFFFF (mass erase), 0xFFFE (bank1 erase), 0xFFFD (bank2 erase)
        stm32_buf[0] = n_pages >> 8;                                    // MSB
        stm32_buf[1] = n_pages & 0xFF;                                  // LSB
        stm32_buf[2] = stm32_buf[0] ^ stm32_buf[1];                     // checksum

        Serial.write (stm32_buf, 3);
        Serial.flush ();

        if (wait_for_ack (35000, 1) < 0)
        {
            return -1;
        }
    }
    else if (n_pages <= 0xFFF0)
    {
        stm32_buf[0] = n_pages >> 8;                                    // MSB
        stm32_buf[1] = n_pages & 0xFF;                                  // LSB
        Serial.write (stm32_buf, 2);

        sum = stm32_buf[0] ^ stm32_buf[1];

        for (i = 0; i < real_pages ; i++)
        {
            stm32_buf[0] = pagenumbers[i] >> 8;                         // MSB
            stm32_buf[1] = pagenumbers[i] & 0xFF;                       // LSB
            sum ^= stm32_buf[0] ^ stm32_buf[1];
            Serial.write (stm32_buf, 2);
        }

        stm32_buf[0] = sum;
        Serial.write (stm32_buf, 1);
        Serial.flush ();

        if (wait_for_ack (35000, 1) < 0)
        {
            return -1;
        }
    }
    else
    {
        return -1;                                                      // codes from 0xFFF0 to 0xFFFD are reserved
    }

    return 0;
}

#define PAGESIZE        256
static uint32_t         start_address   = 0x00000000;                   // address of program start

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * stm32_flash_image (do_flash) - check or flash image
 *
 * do_flash == false: only check file
 * do_flash == true: check and flash file
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#define LINE_BUFSIZE    256
static int
stm32_flash_image (String fname, bool do_flash)
{
    char            linebuf[LINE_BUFSIZE];
    char            logbuf[128];
    uint32_t        i;
    int             len;
    int             line;
    uint8_t         pagebuf[PAGESIZE];
    uint32_t        pageaddr        = 0xffffffff;
    uint32_t        pageidx         = 0;
    unsigned char   datalen;
    uint32_t        address_min     = 0xffffffff;               // minimum address (incl.)
    uint32_t        address_max     = 0x00000000;               // maximum address (incl.)
    uint32_t        last_address    = 0x00000000;
    uint32_t        drlo;                                       // DATA Record Load Offset (current address, 2 bytes)
    uint32_t        ulba            = 0x00000000;               // Upper Linear Base Address (address offset, 4 bytes)
    unsigned char   dri;                                        // Data Record Index
    uint32_t        bytes_written;
    uint32_t        bytes_read;
    uint32_t        pages_written;
    int             eof_record_found = 0;
    int             idx;
    int             ch;
    int             errors = 0;
    int             rtc = 0;

    if (do_flash)
    {
        http_send_FS ("Flashing STM32...<br/>");
        http_flush ();
    }
    else
    {
        http_send_FS ("Checking HEX file ");
        http_send_string (fname);
        http_send_FS (" ...<br/>");
    }

    File f = LittleFS.open(fname, "r");

    if (f)
    {
        line            = 0;
        bytes_read      = 0;
        bytes_written   = 0;
        pages_written   = 0;

        while(f.available())
        {
            idx = 0;

            while ((ch = f.read ()) != EOF)
            {
                if (ch != '\r')
                {
                    if (ch == '\n')
                    {
                        linebuf[idx] = '\0';
                        break;
                    }
                    else if (idx < LINE_BUFSIZE - 1)
                    {
                        linebuf[idx] = ch;
                        idx++;
                    }
                }
            }

            if (idx == 0)
            {
                break;
            }

            line++;
            bytes_read += idx;

            len = idx;
    
            if (linebuf[0] == ':' && len >= 11)
            {
                unsigned char   addrh;
                unsigned char   addrl;
                unsigned char   rectype;
                char *          dataptr;
                unsigned char   chcksum;
                int             sum = 0;
                unsigned char   ch;
    
                datalen = hex2toi (linebuf +  1);
                addrh   = hex2toi (linebuf +  3);
                addrl   = hex2toi (linebuf +  5);
                drlo    = (addrh << 8 | addrl);
                rectype = hex2toi (linebuf + 7);
                dataptr = linebuf + 9;
    
                sum = datalen + addrh + addrl + rectype;
    
                if (len == 9 + 2 * datalen + 2)
                {
                    if (rectype == 0)                               // Data Record
                    {
                        for (dri = 0; dri < datalen; dri++)
                        {
                            ch = hex2toi (dataptr);
    
                            pageidx = ulba + drlo + dri;
    
                            if (pageidx - pageaddr >= PAGESIZE)
                            {
                                if (pageaddr != 0xffffffff)
                                {
                                    if (do_flash)
                                    {
                                        // flash (pagebuf, pageaddr, pageidx - pageaddr);
                                        if (stm32_write_memory (pagebuf, pageaddr, pageidx - pageaddr) < 0)
                                        {
                                            rtc = -1;
                                            break;
                                        }
        
                                        yield ();
        
                                        if (stm32_read_memory (pageaddr, pageidx - pageaddr) < 0)
                                        {
                                            rtc = -1;
                                            break;
                                        }
        
                                        yield ();
        
                                        if (memcmp (pagebuf, stm32_buf, pageidx - pageaddr) != 0)
                                        {
                                            errors++;
                                            char tmpbuf[32];
                                            http_send_FS ("verify failed at address=\r\n");
                                            sprintf (tmpbuf, "%08X \r\n", pageaddr);
                                            http_send (tmpbuf);
                                            sprintf (tmpbuf, "len=%d<BR>\r\n", pageidx - pageaddr);
                                            http_send (tmpbuf);
                                            http_send_FS ("pagebuf:<BR><pre>\r\n");
        
                                            for (i = 0; i < pageidx - pageaddr; i++)
                                            {
                                                sprintf (tmpbuf, "%02X ", pagebuf[i]);
                                                http_send (tmpbuf);
                                                yield ();
                                            }
        
                                            http_send_FS ("</pre><BR>\r\n");
        
                                            http_send_FS ("stm32_buf:<BR><pre>\r\n");
        
                                            for (i = 0; i < pageidx - pageaddr; i++)
                                            {
                                                sprintf (tmpbuf, "%02X ", stm32_buf[i]);
                                                http_send (tmpbuf);
                                                yield ();
                                            }
        
                                            http_send_FS ("</pre><BR>\r\n");
                                            rtc = -1;
                                            break;
                                        }
        
                                        bytes_written += pageidx - pageaddr;
                                    }

                                    pages_written++;

                                    if (do_flash)
                                    {
                                        http_send_FS (".");
    
                                        if (pages_written % 80 == 0)
                                        {
                                            http_send_FS ("<br>");
                                        }

                                        http_flush ();
                                    }
                                }
    
                                pageaddr = ulba + drlo + dri;
                                memset (pagebuf, 0xFF, PAGESIZE);
                            }
    
                            pagebuf[pageidx - pageaddr] = ch;

                            if (! do_flash) // only check
                            {
                                if (last_address && last_address + 1 != ulba + drlo + dri)
                                {
                                    sprintf (logbuf, "Info: gap in line %d, addr 0x%08X. This is normal.\r\n", line, ulba + drlo + dri);
                                    http_send (logbuf);
                                }
                                last_address = ulba + drlo + dri;
                            }

                            if (address_min > ulba + drlo + dri)
                            {
                                address_min = ulba + drlo + dri;
                            }
    
                            if (address_max < ulba + drlo + dri)
                            {
                                address_max = ulba + drlo + dri;
                            }
    
                            sum += ch;
                            dataptr +=2;
                        }
    
                        if (rtc == -1)
                        {
                            break;
                        }
                    }
                    else if (rectype == 1)                          // End of File Record
                    {
                        eof_record_found = 1;

                        for (dri = 0; dri < datalen; dri++)
                        {
                            ch = hex2toi (dataptr);
    
                            sum += ch;
                            dataptr +=2;
                        }
    
                        if (pageidx - pageaddr > 0)
                        {
                            pageidx++;                              // we have to go behind last byte

                            if (do_flash)
                            {
                                // flash (pagebuf, pageaddr, pageidx - pageaddr);
                                if (stm32_write_memory (pagebuf, pageaddr, pageidx - pageaddr) < 0)
                                {
                                    rtc = -1;
                                    break;
                                }
        
                                yield ();
        
                                if (stm32_read_memory (pageaddr, pageidx - pageaddr) < 0)
                                {
                                    rtc = -1;
                                    break;
                                }
        
                                yield ();
        
                                if (memcmp (pagebuf, stm32_buf, pageidx - pageaddr) != 0)
                                {
                                    errors++;

                                    char tmpbuf[32];

                                    http_send_FS ("verify failed at address \r\n");
                                    sprintf (tmpbuf, "%08X<BR>\r\n", pageaddr);
                                    http_send (tmpbuf);
                                    http_send_FS ("pagebuf:<BR><pre>\r\n");

                                    for (i = 0; i < pageidx - pageaddr; i++)
                                    {
                                        sprintf (tmpbuf, "%02X ", pagebuf[i]);
                                        http_send (tmpbuf);
                                        yield ();
                                    }

                                    http_send_FS ("</pre><BR>\r\n");
                                    http_send_FS ("stm32_buf:<BR><pre>\r\n");
        
                                    for (i = 0; i < pageidx - pageaddr; i++)
                                    {
                                        sprintf (tmpbuf, "%02X ", stm32_buf[i]);
                                        http_send (tmpbuf);
                                        yield ();
                                    }
        
                                    http_send_FS ("</pre><BR>\r\n");
                                    rtc = -1;
                                    break;
                                }
        
                                bytes_written += pageidx - pageaddr;
                                pageidx = 0;
                                memset (pagebuf, 0xFF, PAGESIZE);
                            }

                            pages_written++;

                            if (do_flash)
                            {
                                http_send_FS (".");
    
                                if (pages_written % 80 == 0)
                                {
                                    http_send_FS ("<br>");
                                }
                                http_flush ();
                            }
                        }
                        break;                                      // stop reading here
                    }
                    else if (rectype == 4)                          // Extended Linear Address Record
                    {
                        if (drlo == 0)
                        {
                            ulba = 0;
    
                            for (dri = 0; dri < datalen; dri++)
                            {
                                ch = hex2toi (dataptr);
    
                                ulba <<= 8;
                                ulba |= ch;
    
                                sum += ch;
                                dataptr +=2;
                            }
    
                            ulba <<= 16;
                        }
                        else
                        {
                            sprintf (logbuf, "line %d: rectype = %d: address field is 0x%04X\r\n", line, rectype, drlo);
                            http_send (logbuf);
                            rtc = -1;
                            break;
                        }
                    }
                    else if (rectype == 5)                          // Start Linear Address Record
                    {
                        if (drlo == 0)
                        {
                            start_address = 0;
    
                            for (dri = 0; dri < datalen; dri++)
                            {
                                ch = hex2toi (dataptr);
    
                                start_address <<= 8;
                                start_address |= ch;
    
                                sum += ch;
                                dataptr +=2;
                            }
                        }
                        else
                        {
                            sprintf (logbuf, "line %d: rectype = %d: address field is 0x%04X\r\n", line, rectype, drlo);
                            http_send (logbuf);
                            rtc = -1;
                            break;
                        }
                    }
                    else
                    {
                        sprintf (logbuf, "line %d: unsupported record type: %d\r\n", line, rectype);
                        http_send (logbuf);
                        rtc = -1;
                        break;
                    }
    
                    sum = (0x100 - (sum & 0xff)) & 0xff;
                    chcksum = hex2toi (dataptr);
    
                    if (sum != chcksum)
                    {
                        sprintf (logbuf, "invalid checksum: sum: 0x%02X chcksum: 0x%02X\r\n", sum, chcksum);
                        http_send (logbuf);
                        rtc = -1;
                        break;
                    }
                }
                else
                {
                    sprintf (logbuf, "invalid len: %d, expected %d\r\n", len, 12 + 2 * datalen + 2);
                    http_send (logbuf);
                    rtc = -1;
                    break;
                }
            }
            else
            {
                sprintf (logbuf, "invalid INTEL HEX format, len: %d\r\n", len);
                http_send (logbuf);
                rtc = -1;
                break;
            }
        }
    
        f.close();

        if (do_flash)
        {
            sprintf (logbuf, "<BR>Lines read: %d<BR>\r\n", line);
            http_send (logbuf);
            sprintf (logbuf, "Pages flashed: %u<BR>\r\n", pages_written);
            http_send (logbuf);
            sprintf (logbuf, "Bytes flashed: %u<BR>\r\n", bytes_written);
            http_send (logbuf);
            sprintf (logbuf, "Flash write errors: %d<BR>\r\n", errors);
            http_send (logbuf);
    
            if (rtc == 0)
            {
                http_send_FS ("Flash successful<BR>\r\n");
            }
            else
            {
                http_send_FS ("Flash failed<BR>\r\n");
            }
        }
        else
        {
            if (rtc == 0 && ! eof_record_found)
            {
                http_send_FS ("Error: no EOF record found. HEX file may be incomplete.<BR>\r\n");
                rtc = -1;
            }

            if (rtc == 0)
            {
                http_send_FS ("<BR>Check successful<BR>\r\n");
                sprintf (logbuf, "File size: %d<BR>\r\n", bytes_read + 2 * line);
                http_send (logbuf);
            }
            else
            {
                http_send_FS ("Check failed<BR>\r\n");
            }
        }
    }
    else
    {
        http_send_FS ("error: cannot open file<br/>");
        rtc = -1;
    }

    if (do_flash)
    {
        http_flush ();
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * start STM32 bootloader
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#define N_RETRIES    4

static int
stm32_bootloader (String fname, int do_unprotect)
{
    char          buffer[256];
    int           i;
    int           ch;
    unsigned long time1;
    unsigned long time2;
    int           rtc = 0;

    buffer[0] = STM32_BEGIN;

    for (i = 0; i < N_RETRIES; i++)
    {
        http_send_FS ("Trying to enter bootloader mode...<br>\r\n");
        http_flush ();
        Serial.write (buffer, 1);
        ch = stm32_serial_poll (1000, 1);

        if (ch == STM32_ACK)
        {
            break;
        }
    }

    if (i == N_RETRIES)
    {
        return -1;
    }

    rtc = stm32_get (bootloader_info, STM32_INFO_SIZE);

    if (rtc < 0)
    {
        return rtc;
    }

    http_send_FS ("Bootloader version: ");
    sprintf (buffer, "%X.%X", bootloader_info[STM32_INFO_BOOTLOADER_VERSION_IDX] >> 4, bootloader_info[STM32_INFO_BOOTLOADER_VERSION_IDX] & 0x0F);
    http_send (buffer);
    http_send_FS ("<BR>\r\n");

#if 0
    rtc = stm32_get_version (bootloader_version, STM32_VERSION_SIZE);

    if (rtc < 0)
    {
        return rtc;
    }

    http_send_FS ("Option Bytes: ");
    sprintf (buffer, "0x%02X 0x%02X", bootloader_version[STM32_VERSION_OPTION_BYTE1], bootloader_version[STM32_VERSION_OPTION_BYTE2]);
    http_send (buffer);
    http_send_FS ("<BR>\r\n");
#endif

    if (do_unprotect)
    {
        rtc = stm32_write_unprotect ();

        if (rtc >= 0)
        {
            delay (500);

            http_send_FS ("Flash now unprotected<BR>\r\n");
            http_flush ();
            buffer[0] = STM32_BEGIN;

            http_send_FS ("Trying to enter bootloader mode again...");
            http_flush ();

            for (i = 0; i < N_RETRIES; i++)
            {
                Serial.write (buffer, 1);
                ch = stm32_serial_poll (1000, 1);

                if (ch == STM32_ACK)
                {
                    break;
                }
            }

            if (i < N_RETRIES)
            {
                http_send_FS ("successful<br>\r\n");
                http_flush ();
            }
            else
            {
                http_send_FS ("failed<br>\r\n");
            }
        }
    }
    else
    {
        rtc = 0;
    }

    if (rtc >= 0)
    {
        time1 = millis ();
        rtc = stm32_flash_image (fname, false);
        time1 = millis () - time1;
    }

    if (rtc >= 0)
    {
        if (bootloader_info[STM32_INFO_ERASE_CMD_IDX] == STM32_CMD_ERASE)
        {
            http_send_FS ("Erasing flash (standard method)... ");
            http_flush ();
            rtc = stm32_erase (0, 0);
        }
        else if (bootloader_info[STM32_INFO_ERASE_CMD_IDX] == STM32_CMD_EXT_ERASE)
        {
            http_send_FS ("Erasing flash (extended method)... ");
            http_flush ();
            rtc = stm32_ext_erase (0, 0);
        }
        else
        {
            http_send_FS ("Unknown erase method<br>");
            http_flush ();
            rtc = -1;
        }

        if (rtc >= 0)
        {
            http_send_FS ("successful!<br>\r\n");
            http_flush ();
        }
    }

    if (rtc >= 0)
    {
        time2 = millis ();
        rtc = stm32_flash_image (fname, true);
        time2 = millis () - time2;

        sprintf (buffer, "%lu", time1); 
        http_send_FS ("Check time: ");
        http_send (buffer);
        http_send_FS (" msec<BR>");

        sprintf (buffer, "%lu", time2); 
        http_send_FS ("Flash time: ");
        http_send (buffer);
        http_send_FS (" msec<BR>");
    }

    return rtc;
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * download image
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#if 0
#define MAX_LINEBUFLEN  255
bool
stm32_flash_download_image (const char * host, const char * path, const char * filename)
{
    unsigned char   linebuf[MAX_LINEBUFLEN + 1];
    int             len;                                                // content len
    int             read_len;                                           // bytes read in current line
    bool            rtc = false;

    http_send_FS ("Downloading ");
    http_send (filename);
    http_send_FS ("... ");
    http_flush();

    len = httpclient (host, path, filename);

    if (len > 0)
    {
        File f = LittleFS.open("stm32.hex", "w");
    
        while (len > 0)
        {
            read_len = httpclient_read_line (linebuf, MAX_LINEBUFLEN, &len);

            if (read_len > 0)
            {
                f.write (linebuf, read_len);
            }
            else
            {
                break;
            }
        }
    
        f.close();

        httpclient_stop ();

        if (len > 0)                                                                // data remaining?
        {
            http_send_FS ("http read error<br/>");
        }
        else
        {
            http_send_FS ("done.<br/>");
            rtc = true;
        }

        http_flush();
    }
    else
    {
        http_send_FS ("<font color='red'>host connection failed.</font><BR>");
        rtc = false;
    }
    return rtc;
}
#endif

static void
stm32_activate_bootloader (void)
{
    digitalWrite(STM32_BOOT0_PIN, HIGH);                    // activate BOOT0 GPIO5
    pinMode(STM32_RESET_PIN, OUTPUT);                       // RESET to output
    digitalWrite(STM32_RESET_PIN, LOW);                     // activate RESET
    delay (200);                                            // wait 200ms
    pinMode(STM32_RESET_PIN, INPUT);                        // release RESET

    while (stm32_serial_poll (1000, 0) >= 0)                // flush characters in serial input
    {
        ;
    }
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * check hex file
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
stm32_check_hex_file (String fname)
{
    stm32_flash_image (fname, false);
}

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * flash STM32
 * cmd = 0xFF: run some tests
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
stm32_flash_from_local (String fname)
{
    stm32_activate_bootloader ();
    http_send_FS ("Start Bootloader<BR>\r\n");
    http_flush ();
    stm32_bootloader (fname, 1);
    http_send_FS ("End Bootloader<BR>\r\n");
    http_flush ();
}

#if 0 // yet not used
/*-------------------------------------------------------------------------------------------------------------------------------------------
 * flash STM32
 * cmd = 0xFF: run some tests
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
void
stm32_flash_from_server (const char * host, const char * path, const char * filename)
{
    LittleFS.begin();

    if (stm32_flash_download_image(host, path, filename))
    {
        stm32_flash_from_local (filename);
    }

    LittleFS.remove("stm32.hex");
    LittleFS.end();
}
#endif
void
stm32_reset (void)
{
    digitalWrite(STM32_BOOT0_PIN, LOW);                     // deactivate BOOT0 GPIO4
    pinMode(STM32_RESET_PIN, OUTPUT);                       // RESET to output
    digitalWrite(STM32_RESET_PIN, LOW);                     // activate RESET
    delay (200);                                            // wait 200ms
    pinMode(STM32_RESET_PIN, INPUT);                        // release RESET
}

void
stm32_flash_setup (void)
{
    pinMode(STM32_BOOT0_PIN, OUTPUT);                       // BOOT0 GPIO5
    digitalWrite(STM32_BOOT0_PIN, LOW);
    pinMode(STM32_RESET_PIN, INPUT);
    pinMode(STM32_TX_PIN, INPUT);                           // swapped RX, default is input
    pinMode(STM32_RX_PIN, INPUT);                           // swapped TX, default is input
}
