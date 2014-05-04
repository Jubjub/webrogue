cols = 100;
rows = 34;
tileWidth = 11;
tileHeight = 19;

/* set up renderer */
var stage = new PIXI.Stage(0x000000);
var renderer = PIXI.autoDetectRenderer(cols * tileWidth, rows * tileHeight);
document.body.appendChild(renderer.view);

/* set up networking */
websocket = new WebSocket("ws://localhost:7732/");
/* connect to the brogue server */
websocket.onopen = function(e) {
  console.log("connected");
  //websocket.send("hi there!");
}
websocket.onmessage = onMessage;

/* set up graphics */
/* TODO: alpha image with the same brighness as the b/w one */
texture = PIXI.Texture.fromImage("data/images/font-5.png");
/* generate glyph textures */
var glyphs = new Array();
for (var y = 0; y < 16; y++) {
  for (var x = 0; x < 16; x++) {
    var rect = {x: x * tileWidth, y: y * tileHeight, width: tileWidth, height: tileHeight};
    glyphs[y * 16 + x] = new PIXI.Texture(texture.baseTexture, rect);
  }
}
/* generate character sprites */
vconsoleBg = new Array();
for (var y = 0; y < rows; y++) {
  for (var x = 0; x < cols; x++) {
    var sprite = new PIXI.Sprite(glyphs[1]);
    sprite.position.x = x * tileWidth;
    sprite.position.y = y * tileHeight;
    stage.addChild(sprite);
    vconsoleBg[y * cols + x] = sprite;
  }
}

vconsole = new Array();
for (var y = 0; y < rows; y++) {
  for (var x = 0; x < cols; x++) {
    var sprite = new PIXI.Sprite(glyphs[0]);
    sprite.position.x = x * tileWidth;
    sprite.position.y = y * tileHeight;
    stage.addChild(sprite)
    vconsole[y * cols + x] = sprite;
  }
}

/* set up input */
document.addEventListener('keydown', function(e) {
  console.log(e.keyCode);
  /* prevent arrow keys scrolling */
  if([32, 37, 38, 39, 40].indexOf(e.keyCode) > -1) {
    e.preventDefault();
  }
  inputMessage = new Object();
  inputMessage["keycode"] = e.keyCode;
  inputMessage["shift"] = e.shiftKey;
  inputMessage["ctrl"] = e.ctrlKey;
  websocket.send(JSON.stringify(inputMessage));
});

/* main loop */
requestAnimFrame(animate);
function animate() {
  renderer.render(stage);
}

/* parse game update and refresh the virtual console */
function onMessage(e) {
  //console.log(JSON.parse(e.data));
  data = JSON.parse(e.data);
  for (var i = 0; i < data["tiles"].length; i++) {
    var tile = data["tiles"][i];
    var cell = vconsole[tile[1] * cols + tile[0]];
    var bg = vconsoleBg[tile[1] * cols + tile[0]];
    cell.texture = glyphs[tile[2]];
    cell.tint = tile[3];
    bg.tint = tile[4];
  }
  requestAnimFrame(animate);
}
