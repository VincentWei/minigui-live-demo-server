# Web Display Server

- [Story](#story)
- [Living Exsamples](#living-exsamples)
- [Todo](#todo)
- [Copying](#copying)
- [Other information](#other-information)

Web Display Server is a general WebSocket server which acts as a broker of 
a local display client and a WebSocket client (webpage).

By using the Web Display Server, you can show a GUI app in your webpage.
For example, you can interact with a MiniGUI app in a HTML5 browser, 
but the MiniGUI app is running in your IoT device actually.

## Story

1. A webpage connects to the Server via a URI like this:

        ws://<domain.nam>:7788/<display-client-name>

2. A local display client specified by the URI will be forked and executed
   by the Server.

3. The local dipslay client then connencts to the Server via UnixSocket:

    * The Server will create a shadow frame buffer for the client according to
      the resolution of the local display client. The pixel format of the shadow FB 
      will be always RGB888.

    * The local display client sends the dirty rectangle and raw pixels of the
      display to the Server via the UnixSocket (/var/tmp/web-display-server).

    * The server converts the pixels to RGB888 format and stores to the shadow 
      frame buffer. 

    * The Server sends the input events received from the web client to the
      display client via the UnixSocket.

4. The Server encodes the pixels in the accumulated dirty rectangle
   in PNG format, saves the PNG files to the specific directory, and sends
   the dirty information to the web client. The information will be a
   WebSocket binary packet and contain:

    * The dirty rectangle of the display.
    * The URL of the PNG files.

5. The web client can send the keyboard and touch events to the Server. 
   The server forwards the events to the display client. In this way, 
   a web user can interact with the remote display client.

In your webpage, please use `web/webdisplay.js` to connect to the Web Display Server
and render the pixels in a canvas in your HTML5 page. 

Refer to the directory `sample/` for a complete example.

## Living Exsamples

The live demo for MiniGUI is using this Server. Please visit the following URL
for more information:

<https://minigui.fmsoft.cn/live-demos>


## Todo

Use thread for every pair of the remote WebSocket client and the local UnixSocket client.

## Copying

Copyright (c) 2018 ~ 2020 FMSoft (<https://www.fmsoft.cn>)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Other information

Note that this Server is derived from gwsocket:

    https://github.com/allinurl/gwsocket

The sofware `gwsocket` is licensed under MIT.

