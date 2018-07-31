function WebDisplay (host, port, appname, canvasId)
{
    this.host = host;
    this.port = port;
    this.appname = appname;
    this.canvasId = canvasId;
    this.connected = false;
    this.canvas = null;
    this.context = null;
    this.mousedown = false;
}

WebDisplay.prototype.onopen = function () {
    console.log ("onopen");
    this.connected = true;
}

WebDisplay.prototype.onmessage = function (msg) {
    var blob = msg.data;
    if (typeof (blob) == 'object' && blob.slice !== undefined) {

        var dirtyRect = new Uint32Array (msg.data, 0, 16);

        var bytes = new Uint8Array (msg.data, 16, msg.data.byteLength - 16);

        var image = new Image();
        image.src = 'data:image/png;base64,' + btoa (String.fromCharCode.apply (null, bytes));

        image.onload = function () {
            this.context.drawImage (image, dirtyRect[0], dirtyRect[1], dirtyRect[2] - dirtyRect[0], dirtyRect[3] - dirtyRect[1]);
        }.bind (this);
    }
    else {
        console.log ("Got unknown data: " + blob);
    }
};

WebDisplay.prototype.onclose = function () {
    console.log ("onClose");
    this.connected = false;
};

function getPointOnCanvas (canvas, x, y) {
    var bbox = canvas.getBoundingClientRect();

    return {
        x: parseInt (x - bbox.left * (canvas.width / bbox.width)),
        y: parseInt (y - bbox.top  * (canvas.height / bbox.height)) };
}

WebDisplay.prototype.onmousedown = function (evt) {
    var x = evt.clientX;
    var y = evt.clientY;
    var canvas = evt.target;
    var loc = getPointOnCanvas (canvas, x, y);

    this.mousedown = true;
    console.log ("MOUSEDOWN " + loc.x + ' ' + loc.y);
    this.socket.send ("MOUSEDOWN " + loc.x + ' ' + loc.y);
};

WebDisplay.prototype.onmouseup = function (evt) {
    var x = evt.clientX;
    var y = evt.clientY;
    var canvas = evt.target;
    var loc = getPointOnCanvas (canvas, x, y);

    this.mousedown = false;
    console.log ("MOUSEUP " + loc.x + ' ' + loc.y);
    this.socket.send ("MOUSEUP " + loc.x + ' ' + loc.y);
};

WebDisplay.prototype.onmousemove = function (evt) {
    if (this.mousedown) {
        var x = evt.clientX;
        var y = evt.clientY;
        var canvas = evt.target;
        var loc = getPointOnCanvas (canvas, x, y);

        this.socket.send ("MOUSEMOVE " + loc.x + ' ' + loc.y);
    }
};

WebDisplay.prototype.init = function () {
    var canvas = document.getElementById (this.canvasId);
    if (typeof (canvas) != 'object') {
        return false;
    }

    this.context = canvas.getContext ("2d");
    if (typeof (this.context) != 'object') {
        return false;
    }

    var wsURL = "ws://" + this.host + ":" + this.port + "/" + this.appname;

    console.log ("Connecting to " + wsURL);
    this.socket = new WebSocket (wsURL);
    if (typeof (this.socket) != 'object') {
        return false;
    }

    this.socket.binaryType = "arraybuffer";
    this.socket.onopen = this.onopen.bind (this);
    this.socket.onmessage = this.onmessage.bind (this);
    this.socket.onclose = this.onclose.bind (this);

    canvas.addEventListener ("mousedown", this.onmousedown.bind (this), false);
    canvas.addEventListener ('mousemove', this.onmousemove.bind (this), false);
    canvas.addEventListener ('mouseup',  this.onmouseup.bind (this), false);

    return true;
};

