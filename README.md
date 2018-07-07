# MiniGUI Live Demo Server

MiniGUI live demo server acts as a Websocket server.

1. It listens on the port 7777 by default to wait for the connection request
   from a client (a web page).

2. When there is a connection request, the server creates a shared memory 
   as the virtual frame buffer for MiniGUI and a UNIX socket for communication
   between MiniGUI and the server. Then it forks a child and executes the 
   specified MiniGUI demo.

3. The MiniGUI demo uses the virtual frame buffer as the screen and renders
   the graphics in it. It also read the input events (touch and/or key 
   button events) from the UNIX socket. We implement the callback functions 
   of MiniGUI common (GAL and IAL) engines for this purpose.

4. The server tranfers the pixel data from MiniGUI demo via UNIX socket
   then transfers the data to the client via Websocket. The server also
   transfers the input events from the client via Websocket, and transfers
   the events to MiniGUI via UNIX socket.

Note that this server is based on the work of allinurl at:

    https://github.com/allinurl/gwsocket

The sofware gwsocket is licensed under MIT.

## Usage

