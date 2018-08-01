# Web Display Server

Web Display Server is a general WebSocket server which can acts as a broker of 
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
   in PNG format and sends the data to the web client in every 35ms.
   The data will be a WebSocket binary packet and contain:

    * The dirty rectangle of the display.
    * The pixels of the dirty rectangle encoded in PNG format.

5. The web client can send the keyboard and touch events to the Server. 
   The server forwards the events to the display client. In this way, 
   a web user can interact with the remote display client.

For more information, please see the example in the directory `sample/`.

## Other information

Note that this Server is based on the work of allinurl at:

    https://github.com/allinurl/gwsocket

The sofware `gwsocket` is licensed under MIT.

