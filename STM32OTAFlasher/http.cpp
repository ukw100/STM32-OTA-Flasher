/*----------------------------------------------------------------------------------------------------------------------------------------
 * http.cpp - STM32 OTA Flasher Webserver
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
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <LittleFS.h>

#include "STM32OTAFlasher.h"
#include "http.h"
#include "eepromdata.h"
#include "stm32flash.h"

/*----------------------------------------------------------------------------------------------------------------------------------------
 * global data:
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static const char *                 host = "stm32flasher";

ESP8266WebServer                    httpServer(80);
ESP8266HTTPUpdateServer             httpUpdater;
String                              sResponse;

/*----------------------------------------------------------------------------------------------------------------------------------------
 * http send C string
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
http_send (const char * s)
{
    sResponse += s;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * http send C++ string
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
http_send_string (String s)
{
    sResponse += s;
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * http send C string
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
http_flush (void)
{
    httpServer.sendContent(sResponse);
    sResponse = "";
}
/*----------------------------------------------------------------------------------------------------------------------------------------
 * send http header
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
#if 0 // not used
static void
http_header (void)
{
    http_send ("HTTP/1.0 200 OK\r\n\r\n");
}
#endif

/*----------------------------------------------------------------------------------------------------------------------------------------
 * send html header
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static void
html_header (String title, String url, bool use_utf8)
{
    sResponse = "";

    String network_color  = "blue";
    String update_color   = "blue";
    String upload_color   = "blue";
    String flash_color    = "blue";

    if (url.equals ("/"))
    {
        network_color = "red";
    }
    else if (url.equals ("/net"))
    {
        network_color = "red";
    }
    else if (url.equals ("/upd"))
    {
        update_color = "red";
    }
    else if (url.equals ("/upl"))
    {
        upload_color = "red";
    }
    else if (url.equals ("/flash"))
    {
        flash_color = "red";
    }

    sResponse += (String) "<!DOCTYPE HTML>\r\n";
    sResponse += (String) "<html>\r\n";
    sResponse += (String) "<head>\r\n";
    if (use_utf8)
    {
        sResponse += (String) "<meta charset='UTF-8'>";
    }
    else
    {
        sResponse += (String) "<meta charset='ISO-8859-1'>";
    }
    sResponse += (String) "<title>";
    sResponse += (String) "STM32OTAFlasher";

    if (! title.equals (""))
    {
        sResponse += (String) " - " + title;
    }

    sResponse += (String) "</title>\r\n";
    sResponse += (String) "<meta name='viewport' content='width=device-width,initial-scale=1'/>\r\n";
    sResponse += (String) "<style>\r\n";
    sResponse += (String) "BODY { FONT-FAMILY: Helvetica,Arial; FONT-SIZE: 14px; }\r\n";
    sResponse += (String) "</style>\r\n";
    sResponse += (String) "</head>\r\n";
    sResponse += (String) "<body>\r\n";
    sResponse += (String) "\r\n";
    sResponse += (String) "<table>\r\n";
    sResponse += (String) "<tr>\r\n";
    sResponse += (String) "<td style='padding:5px;'><a style='color:" + network_color + ";' href='/net'>Network</a></td>\r\n";
    sResponse += (String) "<td style='padding:5px;'><a style='color:" + update_color  + ";' href='/upd'>Update ESP8266</a></td>\r\n";
    sResponse += (String) "<td style='padding:5px;'><a style='color:" + upload_color  + ";' href='/upl'>Upload File</a></td>\r\n";
    sResponse += (String) "<td style='padding:5px;'><a style='color:" + flash_color   + ";' href='/flash'>Flash STM32</a></td>\r\n";
    sResponse += (String) "</tr>\r\n";
    sResponse += (String) "</table>\r\n";

    if (! title.equals (""))
    {
        sResponse += (String) "<H3 style='margin-left:10px'>";
        sResponse += (String) title;
        sResponse += (String) "</H3>\r\n";
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------
 * send html trailer
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
static void
html_trailer (void)
{
    http_send ("</body>\r\n");
    http_send ("</html>\r\n");
}

static void
handle_pre_actions (String action)
{
    if (action.equals ("delete"))
    {
        String fname = httpServer.arg("fname");
        LittleFS.remove (fname);
    }
}

static void
handle_post_actions (String action)
{
    if (action.equals ("check"))
    {
        String fname = httpServer.arg("fname");
        sResponse += (String) "<BR>\r\n";
        stm32_check_hex_file (fname);
    }
}

static void
show_directory (String action, String url, bool verbose)
{
    FSInfo fs_info;
    LittleFS.info(fs_info);

    handle_pre_actions (action);

    if (verbose)
    {
        sResponse += (String) "<table>\r\n";
        sResponse += (String) "<tr><td>Total space:</td><td align='right'>" + fs_info.totalBytes + "</td></tr>\r\n";
        sResponse += (String) "<tr><td>Space used:</td><td align='right'>" + fs_info.usedBytes + "</td></tr>\r\n";
        sResponse += (String) "<tr><td>Block size:</td><td align='right'>" + fs_info.blockSize + "</td></tr>\r\n";
        sResponse += (String) "<tr><td>Page size:</td><td align='right'>" + fs_info.pageSize + "</td></tr>\r\n";
        sResponse += (String) "<tr><td>Max open files:</td><td align='right'>" + fs_info.maxOpenFiles + "</td></tr>\r\n";
        sResponse += (String) "<tr><td>Max path length:</td><td align='right'>" + fs_info.maxPathLength + "</td></tr>\r\n";
        sResponse += (String) "</table>\r\n";
        sResponse += (String) "<BR>\r\n";
    }
   
    sResponse += (String) "<B>Directory:</B>\r\n";
    sResponse += (String) "<table style='border:1px gray solid\'>\r\n";
    sResponse += (String) "<tr bgcolor='#e0e0e0'><th width='120' align='left'>Filename</th><th>Size</th><th colspan='3'>Action</th></tr>\r\n";
 
    Dir dir = LittleFS.openDir("/");

    while (dir.next())
    {
        String  filename;
        String  suffix;
        int     len;
        int     siz;

        if(dir.fileSize())
        {
            File f = dir.openFile("r");
            siz = f.size();
            f.close();
        }
        else
        {
            siz = 0;
        }

        filename = dir.fileName();

        sResponse += "<tr>";
        sResponse += "<td>";
        sResponse += filename;
        sResponse += "</td><td align='right'>";
        sResponse += siz;
        sResponse += "</td>";

        len = filename.length();
        
        if (len > 4)
        {
            suffix = filename.substring (len - 4);
            suffix.toLowerCase();

            sResponse += "<td>\r\n";
            sResponse += "<form action='";
            sResponse += url;
            sResponse += "' method='GET'>\r\n";
            sResponse += "  <input type='hidden' name='action' value='delete'>\r\n";
            sResponse += "  <input type='hidden' name='fname'  value='";
            sResponse += filename;
            sResponse += "'>\r\n";
            sResponse += "  <input type='submit' value='Delete'>\r\n";
            sResponse += "</form>\r\n";
            sResponse += "</td>\r\n";

            if (suffix.equals(".hex"))
            {
                sResponse += "<td>\r\n";
                sResponse += "<form action='";
                sResponse += url;
                sResponse += "' method='GET'>\r\n";
                sResponse += "  <input type='hidden' name='action' value='check'>\r\n";
                sResponse += "  <input type='hidden' name='fname'  value='";
                sResponse += filename;
                sResponse += "'>\r\n";
                sResponse += "  <input type='submit' value='Check'>\r\n";
                sResponse += "</form>\r\n";
                sResponse += "</td>\r\n";

                sResponse += "<td>\r\n";
                sResponse += "<form action='/flash' method='GET'>\r\n";
                sResponse += "  <input type='hidden' name='action' value='flash'>\r\n";
                sResponse += "  <input type='hidden' name='fname'  value='";
                sResponse += filename;
                sResponse += "'>\r\n";
                sResponse += "  <input type='submit' value='Flash'>\r\n";
                sResponse += "</form>\r\n";
                sResponse += "</td>\r\n";
            }
        }
        sResponse += "</tr>\r\n";
    }

    sResponse += (String) "</table>\r\n";
    handle_post_actions (action);
}

static void
handle_doupload ()
{
    static File   fp = (File) 0;
    static String filename;
    String action = String ();

    HTTPUpload& uploadfile = httpServer.upload();

    if (uploadfile.status == UPLOAD_FILE_START)
    {
        filename = uploadfile.filename;

        if (!filename.startsWith("/"))
        {
            filename = "/" + filename;
        }

        LittleFS.remove(filename);
        fp = LittleFS.open (filename, "w");
    }
    else if (uploadfile.status == UPLOAD_FILE_WRITE)
    {
        if (fp)
        {
            fp.write (uploadfile.buf, uploadfile.currentSize);
        }
    } 
    else if (uploadfile.status == UPLOAD_FILE_END)
    {
        String  title = "Result upload file";
        String  url = "/upl";

        html_header (title, url, false);

        if (fp)
        {                                    
            fp.close();
            fp = (File) 0;

            sResponse += F("File upload successful.<BR>\r\n"); 
            sResponse += F("Uploaded File Name: ");
            sResponse += uploadfile.filename;
            sResponse += "\r\n";
        }
        else
        {
            sResponse += F("could not create file: ");
            sResponse += filename;
            sResponse += F("\r\n");
        }

        sResponse += F("<P>\r\n");
        show_directory (action, url, true);
        html_trailer ();
        httpServer.send(200, "text/html", sResponse);
    }
}

void
handle_main ()
{
    String    title   = "";
    String    url     = "/";

    html_header (title, url, false);

    sResponse += "<H3 style='margin-left:10px'>Welcome to STM32 OTA Flasher!</H3>";
    html_trailer ();
    httpServer.send(200, "text/html", sResponse);
}

void
handle_net()
{
    String    title   = "Network";
    String    url     = "/";
    bool      connect = false;
    bool      ap      = false;
    const char * msg  = (char *) 0;

    String action = httpServer.arg("action");
    // int state = server.arg("state").toInt();

    if (action.equals ("connect"))
    {
        String  pssid     = httpServer.arg ("ssid");
        String  pkey      = httpServer.arg ("key");

        if (! pssid.equals (eeprom_ssid))
        {
            pssid.toCharArray(eeprom_ssid, EEPROM_SSID_LEN);
            eeprom_save_ssid ();
        }

        if (! pkey.equals (eeprom_ssidkey))
        {
            pkey.toCharArray (eeprom_ssidkey, EEPROM_SSID_KEY_LEN);
            eeprom_save_ssidkey ();
        }

        eeprom_flags &= ~EEPROM_FLAG_BOOT_AS_AP;
        eeprom_save_flags ();
        eeprom_commit ();

        connect = true;
    }
    else if (action.equals ("ap"))
    {
        String  ap_ssid     = httpServer.arg ("ap_ssid");
        String  ap_key      = httpServer.arg ("ap_key");

        if (ap_key.length() < 8)
        {
            msg = "The length of the key must have at least 8 characters!";
        }
        else
        {
            if (! ap_ssid.equals (eeprom_ap_ssid))
            {
                ap_ssid.toCharArray(eeprom_ap_ssid, EEPROM_AP_SSID_LEN);
                eeprom_save_ap_ssid ();
            }
    
            if (! ap_key.equals (eeprom_ap_ssidkey))
            {
                ap_key.toCharArray (eeprom_ap_ssidkey, EEPROM_AP_SSID_KEY_LEN);
                eeprom_save_ap_ssidkey ();
            }
    
            eeprom_flags |= EEPROM_FLAG_BOOT_AS_AP;
            eeprom_save_flags ();
            eeprom_commit ();
    
            ap = true;
        }
    }

    html_header (title, url, true);               // save SSIDs and KEYs in UTF-8

    if (connect)
    {
        sResponse += "<P><B>Connecting, please try again later...</B>\r\n";
    }
    else if (ap)
    {
        sResponse += "<P><B>Starting as AP, please try again later...</B>\r\n";
    }
    else
    {
        sResponse += (String) "<form method=\"GET\" action=\"/\">\r\n";
        sResponse += (String) "  <div style='margin:10px;padding:10px;border:1px lightgray solid; width:360px;'>\r\n";
        sResponse += (String) "  <table>\r\n";
        sResponse += (String) "    <tr>\r\n";
        sResponse += (String) "      <td width='100'>SSID</td>\r\n";
        sResponse += (String) "      <td width='100'><input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"" + eeprom_ssid + "\" maxlength=\"32\" size=\"32\"></td>\r\n";
        sResponse += (String) "    </tr>\r\n";
        sResponse += (String) "    <tr>\r\n";
        sResponse += (String) "      <td>Key</td>\r\n";
        sResponse += (String) "      <td><input type=\"text\" id=\"key\" name=\"key\" value=\"" + eeprom_ssidkey + "\" maxlength=\"64\" size=\"32\"></td>\r\n";
        sResponse += (String) "    </tr>\r\n";
        sResponse += (String) "    <tr>\r\n";
        sResponse += (String) "      <td></td>\r\n";
        sResponse += (String) "      <td><button type=\"submit\" name=\"action\" value=\"connect\">Connect to SSID</button></td>\r\n";
        sResponse += (String) "    </tr>\r\n";
        sResponse += (String) "  </table>\r\n";
        sResponse += (String) "  </div>\r\n";
        sResponse += (String) "</form>\r\n";

        sResponse += (String) "<form method=\"GET\" action=\"/\">\r\n";
        sResponse += (String) "  <div style='margin:10px;padding:10px;border:1px lightgray solid; width:360px;'>\r\n";
        sResponse += (String) "  <table>\r\n";
        sResponse += (String) "    <tr>\r\n";
        sResponse += (String) "      <td width='100'>AP SSID</td>\r\n";
        sResponse += (String) "      <td width='100'><input type=\"text\" id=\"ssid\" name=\"ap_ssid\" value=\"" + eeprom_ap_ssid + "\" maxlength=\"32\" size=\"32\"></td>\r\n";
        sResponse += (String) "    </tr>\r\n";
        sResponse += (String) "    <tr>\r\n";
        sResponse += (String) "      <td>Key</td>\r\n";
        sResponse += (String) "      <td><input type=\"text\" id=\"key\" name=\"ap_key\" value=\"" + eeprom_ap_ssidkey + "\" maxlength=\"64\" size=\"32\"></td>\r\n";
        sResponse += (String) "    </tr>\r\n";
        sResponse += (String) "    <tr>\r\n";
        sResponse += (String) "      <td></td>\r\n";
        sResponse += (String) "      <td><button type=\"submit\" name=\"action\" value=\"ap\">Start as AP</button></td>\r\n";
        sResponse += (String) "    </tr>\r\n";
        sResponse += (String) "  </table>\r\n";
        sResponse += (String) "  </div>\r\n";
        sResponse += (String) "</form>\r\n";

        if (msg)
        {
            sResponse += (String) "<BR><font color='red'>" + msg + "</font>\r\n";
        }
    }

    html_trailer ();
    httpServer.send(200, "text/html", sResponse);

    if (connect)
    {
        delay (1000);
        wifi_connect (eeprom_ssid, eeprom_ssidkey, true);
    }
    else if (ap)
    {
        delay (1000);
        wifi_ap (eeprom_ap_ssid, eeprom_ap_ssidkey);
    }
}

void
handle_upd ()
{
    String    title   = "Update ESP8266";
    String    url     = "/upd";

    html_header (title, url, false);

    sResponse += (String) "<form method='POST' action='/update' enctype='multipart/form-data'>\r\n";
    sResponse += (String) "<div style='margin:10px;padding:10px;border:1px lightgray solid; width:360px;'>\r\n";
    sResponse += (String) "ESP 8266 Firmware:<br><br>\r\n";
    sResponse += (String) "<input type='file' accept='.bin,.bin.gz' name='firmware'>\r\n";
    sResponse += (String) "<input type='submit' value='Update'>\r\n";
    sResponse += (String) "</div>\r\n";
    sResponse += (String) "</form>\r\n";

    html_trailer ();
    httpServer.send(200, "text/html", sResponse);
}

void
handle_upl ()
{
    String      title   = "Upload File";
    String      url     = "/upl";
    String      action  = httpServer.arg("action");

    html_header (title, url, false);
    sResponse += F("<P>\r\n");
    sResponse += (String) "<div style='margin:10px;padding:10px;border:1px lightgray solid; width:360px;'>\r\n";
    show_directory (action, url, true);

    sResponse += (String) "<BR>\r\n";
    sResponse += (String) "<form method='POST' action='/doupload' enctype='multipart/form-data'>\r\n";
    sResponse += (String) "Upload File:<br><br>\r\n";
    sResponse += (String) "<input type='file' name='file'>\r\n";
    sResponse += (String) "<input type='submit' value='Upload'>\r\n";
    sResponse += (String) "</form>\r\n";
    sResponse += (String) "</div>\r\n";

    html_trailer ();
    httpServer.send(200, "text/html", sResponse);
}

void
handle_flash ()
{
    String    title       = "Flash STM32";
    String    url         = "/flash";
    String    action      = httpServer.arg("action");

    html_header (title, url, false);
    httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);        // unknown length of output
    httpServer.send(200, "text/html", sResponse);               // send header part
    sResponse = "";

    sResponse += F("<P>\r\n");
    sResponse += (String) "<div style='margin:10px;padding:10px;border:1px lightgray solid; width:360px;'>\r\n";
    show_directory (action, url, false);
    sResponse += (String) "</div>\r\n";

    if (action.equals ("flash"))
    {
        String fname = httpServer.arg("fname");

        sResponse += (String) "<BR>\r\n";
        stm32_flash_from_local (fname);
    }
    else if (action.equals ("reset"))
    {
        stm32_reset ();
    }

    sResponse += (String) "<form method='GET' action='" + url + "'>\r\n";
    sResponse += (String) "<P><button type='submit' name=\"action\" value=\"reset\">Reset STM32</button>\r\n";
    sResponse += (String) "</form>\r\n";
    html_trailer ();                                        // send trailer part
    http_flush ();
    httpServer.sendContent("");                             // EOF: empty line
}


/*----------------------------------------------------------------------------------------------------------------------------------------
 * http_init
 *----------------------------------------------------------------------------------------------------------------------------------------
 */
void
http_setup (void)
{
    LittleFS.begin();
    MDNS.begin(host);
    httpUpdater.setup(&httpServer);
    httpServer.begin();
    httpServer.on("/", handle_main);
    httpServer.on("/net", handle_net);
    httpServer.on("/upd", handle_upd);
    httpServer.on("/upl", handle_upl);
    httpServer.on("/flash", handle_flash);
    httpServer.on("/doupload", HTTP_POST, []() { httpServer.send(200); }, handle_doupload );
    MDNS.addService("http", "tcp", 80);
}

void
http_loop (void)
{
    httpServer.handleClient();
    MDNS.update();
}
