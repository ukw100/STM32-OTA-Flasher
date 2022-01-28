/*----------------------------------------------------------------------------------------------------------------------------------------
 * http.h - STM32 OTA Flasher Webserver
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
#ifndef HTTP_H
#define HTTP_H

#define FS(str)             String(F(str)).c_str()
#define http_send_FS(x)     http_send(FS(x))                                         // leave string constants in flash memory

extern void                 http_send (const char *);
extern void                 http_send_string (String);
extern void                 http_flush (void);
extern void                 http_setup (void);
extern void                 http_loop (void);

extern String               sResponse;

#endif
