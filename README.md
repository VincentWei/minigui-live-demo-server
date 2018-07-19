# MiniGUI Remote Server

MiniGUI Remote Server acts as a Websocket server and a UNIX domain socket server at
the same time.

1. It listens on the port 7777 for websocket connections and /var/tmp/mg-remote-sever
   for UNIX domain socket connections.

1. A websocket client connects to the server, tells the server the demo it want to run.
   It will also tell the server the resolution (width and height in pixels) to show the demo.

2. The server forks and executes the MiniGUI program specified by the websocket client
   in the child. Before this, the server should set the following environment variables
   for the MiniGUI client.

    * `MG_GAL_ENGINE`: should be commonlcd
    * `MG_DEFAULTMODE`: should be a pattern like 320x240-16bpp
    * `MG_IAL_ENGINE`: shold be common

3. The server also creates a UNIX domain socket at a well-known path (/var/tmp/mg-remote-sever)
   and wait for the connection from the MiniGUI client.

4. The MiniGUI client connects to the UNIX socket, creates a shared memory as a
   virtual frame buffer, and sends the identifier of the shared memory to the server
   via the UNIX socket.

5. Note that the resolution of the virtual frame buffer should be sent by the server as
   the first message. 

6. The server accepts the connection request from the MiniGUI client, gets the
   identifier of the shared memory sent by the client via the UNIX socket,
   and attaches to the shared memory.

7. The MiniGUI client sends the dirty rectangle information via the
   UNIX socket, and the server then sends the information and the pixel data
   to the websocket client. For the performance reason, the server creates
   a shadow frame buffer and copy the pixel data from the virtual frame buffer
   and send an ACK message to MiniGUI client immediately when it gets the dirty
   rectangle message. The server then sends the pixel data from the shadow 
   frame buffer to the websocket client.

8. The server gets the input events from the websocket client and sends them
   to the MiniGUI client via the UNIX socket.

Note that this server is based on the work of allinurl at:

    https://github.com/allinurl/gwsocket

The sofware `gwsocket` is licensed under MIT.

## Usage

