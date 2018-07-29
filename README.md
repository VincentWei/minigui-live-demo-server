# Web Display Server

Web Display Server is a general WebSocket server which can acts as a broker of 
a local display client and a WebSocket client (webpage).

By using the Web Display Server, you can show a GUI app in your webpage!
For example, you can interact with a MiniGUI app in a HTML5 browser, 
but the MiniGUI app is running in your IoT device actually.

## Story

1. A webpage connects to the Server via a URI like this:

        ws://<domain.nam>:7777/<display-client-name>?width=<horizontal-resolution-in-pixel>&height=<vertical-resolution-in-pixel>

2. A local display client specified by the URI will be forked and executed
   by the Server.

3. The local dipslay client then connencts to the Server via UnixSocket:

    * The Server will create a shadow frame buffer for the client according to
      the resolution specified by the URI. The pixel format of the shadow FB 
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

## Protocols

### WebSocket

### UnixSocekt

MiniGUI Remote Server acts as a Websocket server and a UNIX domain socket server at
the same time.

1. It listens on the port 7777 for WebSocket connections and /var/tmp/mg-remote-sever
   for UNIX domain socket connections.

1. A WebSocket client connects to the server, tells the server the demo it want to run.
   It will also tell the server the resolution (width and height in pixels) to show the demo.

2. The server forks and executes the MiniGUI program specified by the WebSocket client
   in the child. Before this, the server should set the following environment variables
   for the MiniGUI client.

    * `MG_GAL_ENGINE`: should be commonlcd
    * `MG_DEFAULTMODE`: should be a pattern like 320x240-16bpp
    * `MG_IAL_ENGINE`: shold be common

3. The server also creates a UNIX domain socket at a well-known path (/var/tmp/mg-remote-sever)
   and wait for the connection from the MiniGUI client.

4. The MiniGUI client connects to the UnixSocket, creates a shared memory as a
   virtual frame buffer, and sends the identifier of the shared memory to the server
   via the UnixSocket.

5. Note that the resolution of the virtual frame buffer should be sent by the server as
   the first message. 

6. The server accepts the connection request from the MiniGUI client, gets the
   identifier of the shared memory sent by the client via the UnixSocket,
   and attaches to the shared memory.

7. The MiniGUI client sends the dirty rectangle information via the
   UnixSocket, and the server then sends the information and the pixel data
   to the WebSocket client. For the performance reason, the server creates
   a shadow frame buffer and copy the pixel data from the virtual frame buffer
   and send an ACK message to MiniGUI client immediately when it gets the dirty
   rectangle message. The server then sends the pixel data from the shadow 
   frame buffer to the WebSocket client.

8. The server gets the input events from the WebSocket client and sends them
   to the MiniGUI client via the UnixSocket.

## Other information

Note that this Server is based on the work of allinurl at:

    https://github.com/allinurl/gwsocket

The sofware `gwsocket` is licensed under MIT.

