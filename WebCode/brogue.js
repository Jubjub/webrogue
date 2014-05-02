cols = 100;
rows = 34;
tileWidth = 11;
tileHeight = 19;

var game = new Phaser.Game(cols * tileWidth, rows * tileHeight, Phaser.AUTO, "",
    { preload: preload,
      create: create,
      update: update
    });

function preload() {
  game.load.image("font", "data/images/font-5.png");
}

function create() {
  /* connect to the brogue server */
  websocket = new WebSocket("ws://localhost:7732/");
  websocket.onopen = function(e) {
    console.log("connected");
    websocket.send("hi there!");
  }
  websocket.onmessage = onMessage;
  /* set up tilemap */
  map = game.add.tilemap();
  map.addTilesetImage("font", "font", tileWidth, tileHeight);
  layer = map.create("characters", cols, rows, tileWidth, tileHeight);
  layer.resizeWorld();
  /* set up input */
  game.input.keyboard.onDownCallback = function(e) {
    console.log(e.keyCode);
  }
}

function update() {
}

function onMessage(e) {
  //console.log(JSON.parse(e.data));
  data = JSON.parse(e.data);
  for (var i = 0; i < data["tiles"].length; i++) {
    var tile = data["tiles"][i];
    map.putTile(tile[2], tile[0], tile[1], "characters");
  }
}
