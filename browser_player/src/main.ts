import "./style.css";
import "@xterm/xterm/css/xterm.css";
import { RewindTTYPlayer } from "./player";

const player = new RewindTTYPlayer();

document.addEventListener("DOMContentLoaded", async () => {
  await player.initialize();
});
