var game = new Phaser.Game(800, 600, Phaser.AUTO, "",
    { preload: preload, create: create, update: update });

function preload() {
  game.load.image("font", "data/images/font-7.png");
}

function create() {
  websocket = new WebSocket("ws://localhost:7732/");
  websocket.onopen = function(e) {
    console.log("connected");
    websocket.send("hi there!");
  }
  websocket.onmessage = onMessage;
  game.add.sprite(0, 0, "font");
}

function update() {
}

function onMessage(e) {
  console.log(e.data);
}
