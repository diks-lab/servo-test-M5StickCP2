'use strict';

let toolTitle;
let lblMessage;
let valUsec;
let rngUsec;

var ws = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
//var ws = new WebSocket('ws://192.168.1.10:81/', ['arduino']);
ws.binaryType = 'arraybuffer';
ws.onopen = function () {
  //< !--ws.send('Connect ' + new Date()); -->
  toolTitle.setAttribute("status", "on");
  ws.send('Connect ' + new Date());
  lblMessage.textContent = '';
  console.log("WebSocket ws opened");
}
ws.onerror = function (error) {
  toolTitle.setAttribute("status", "err");
  toolTitle.textContent = "No Connection to WebSocket Server.";
  // lblMessage.textContent = 'No Connection to WebSocket Server.';
  console.log('WebSocket Error ', error);
};
ws.onmessage = function (e) {
  console.log('Message from server ');
  if (typeof (e.data) == 'string') { // Text frame
    console.log('Server: ', e.data);
    if (e.data[0] == 'w') {
      var val = e.data.substring(1);
      valUsec.value = val;
      rngUsec.value = val;
    }
  } else { // Binary
  }
};

window.addEventListener('DOMContentLoaded', () => {
  toolTitle = document.querySelector(".tool-title");
  lblMessage = document.querySelector(".lblMessage");
  valUsec = document.querySelector("#valUsec");
  rngUsec = document.querySelector("#rngUsec");

  rngUsec.addEventListener('input', (e) => {
    if (ws.readyState !== ws.OPEN) {
      toolTitle.setAttribute("status", "off");
      valUsec.value = '- - -';
      lblMessage.textContent = 'WebSocket is already in CLOSED state.';
      return
    }
    valUsec.value = e.target.value;
    let sendText = 'm' + e.target.value + 'x';
    ws.send(sendText);
    console.log(sendText);

  })
});
