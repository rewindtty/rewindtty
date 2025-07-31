import { Terminal } from "@xterm/xterm";
import type {
  Session,
  Bookmark,
  PlaybackState,
  TimelineCommand,
} from "./types";
import { FitAddon } from "@xterm/addon-fit";

export class RewindTTYPlayer {
  private terminal: Terminal;
  private fitAddon: FitAddon;
  private sessions: Session[] = [];
  private bookmarks: Bookmark[] = [];
  private playbackState: PlaybackState;
  private timelineCommands: TimelineCommand[] = [];
  private playbackTimer: number | null = null;
  private startTime: number = 0;
  private lastProcessedTime: number = 0;

  private elements: {
    currentCommand: HTMLElement;
    sessionTime: HTMLElement;
    playPauseBtn: HTMLElement;
    restartBtn: HTMLElement;
    speedBtn: HTMLElement;
    timeline: HTMLElement;
    timelineProgress: HTMLElement;
    timelineCommands: HTMLElement;
    timelineScrubber: HTMLElement;
    bookmarksContainer: HTMLElement;
    addBookmarkBtn: HTMLElement;
    clearBookmarksBtn: HTMLElement;
    fileInput: HTMLInputElement;
    loadFileBtn: HTMLElement;
  };

  constructor() {
    this.terminal = new Terminal({
      convertEol: true,
      cursorBlink: true,
      theme: {
        background: "#1e1e1e",
        foreground: "#cccccc",
      },
    });

    this.fitAddon = new FitAddon();
    this.terminal.loadAddon(this.fitAddon);

    this.playbackState = {
      isPlaying: false,
      currentTime: 0,
      totalDuration: 0,
      playbackSpeed: 1,
      currentSessionIndex: 0,
      currentChunkIndex: 0,
    };

    this.elements = this.initializeElements();
    this.setupEventListeners();
    this.loadBookmarks();
  }

  private initializeElements() {
    return {
      currentCommand: document.getElementById("current-command")!,
      sessionTime: document.getElementById("session-time")!,
      playPauseBtn: document.getElementById("play-pause-btn")!,
      restartBtn: document.getElementById("restart-btn")!,
      speedBtn: document.getElementById("speed-btn")!,
      timeline: document.getElementById("timeline")!,
      timelineProgress: document.getElementById("timeline-progress")!,
      timelineCommands: document.getElementById("timeline-commands")!,
      timelineScrubber: document.getElementById("timeline-scrubber")!,
      bookmarksContainer: document.getElementById("bookmarks")!,
      addBookmarkBtn: document.getElementById("add-bookmark-btn")!,
      clearBookmarksBtn: document.getElementById("clear-bookmarks-btn")!,
      fileInput: document.getElementById("file-input") as HTMLInputElement,
      loadFileBtn: document.getElementById("load-file-btn")!,
    };
  }

  private setupEventListeners(): void {
    this.elements.playPauseBtn.addEventListener("click", () =>
      this.togglePlayPause()
    );
    this.elements.restartBtn.addEventListener("click", () => this.restart());
    this.elements.speedBtn.addEventListener("click", () => this.cycleSpeed());
    this.elements.timeline.addEventListener("click", (e) =>
      this.seekToPosition(e)
    );
    this.elements.addBookmarkBtn.addEventListener("click", () =>
      this.addBookmark()
    );
    this.elements.clearBookmarksBtn.addEventListener("click", () =>
      this.clearBookmarks()
    );
    this.elements.loadFileBtn.addEventListener("click", () =>
      this.elements.fileInput.click()
    );
    this.elements.fileInput.addEventListener("change", (e) =>
      this.handleFileLoad(e)
    );

    window.addEventListener("resize", () => this.fitAddon.fit());

    window.addEventListener("keydown", (e) => {
      if (e.code === "Space") {
        e.preventDefault();
        this.togglePlayPause();
      } else if (e.code === "KeyR") {
        e.preventDefault();
        this.restart();
      } else if (e.code === "KeyB") {
        e.preventDefault();
        this.addBookmark();
      }
    });
  }

  async initialize(): Promise<void> {
    this.terminal.open(document.getElementById("terminal")!);
    this.fitAddon.fit();

    try {
      const response = await fetch("../data/session.json");
      if (response.ok) {
        this.sessions = await response.json();
        this.processSessionData();
      } else {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
    } catch (error) {
      console.error("Failed to load default session data:", error);
      this.elements.currentCommand.textContent =
        "Click 'Load JSON' to load a session file";
    }
  }

  private handleFileLoad(event: Event): void {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];

    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const content = e.target?.result as string;
        this.sessions = JSON.parse(content);
        this.processSessionData();
        this.elements.currentCommand.textContent = `Loaded: ${file.name}`;
        this.play();
      } catch (error) {
        console.error("Failed to parse JSON file:", error);
        this.elements.currentCommand.textContent = "Error: Invalid JSON file";
      }
    };

    reader.readAsText(file);
  }

  private processSessionData(): void {
    this.restart();
    this.calculateTotalDuration();
    this.createTimelineCommands();
    this.renderTimelineCommands();
    this.renderBookmarks();
    this.updateUI();
  }

  private calculateTotalDuration(): void {
    if (this.sessions.length === 0) return;

    let totalTime = 0;
    this.sessions.forEach((session) => {
      // Use the session's actual duration (already calculated correctly in JSON)
      totalTime += session.duration * 1000; // Convert to milliseconds
    });

    this.playbackState.totalDuration = totalTime;
  }

  private createTimelineCommands(): void {
    this.timelineCommands = [];
    let cumulativeTime = 0;

    this.sessions.forEach((session, sessionIndex) => {
      const position =
        this.playbackState.totalDuration > 0
          ? (cumulativeTime / this.playbackState.totalDuration) * 100
          : 0;

      this.timelineCommands.push({
        sessionIndex,
        command: session.command,
        startTime: cumulativeTime,
        position,
      });

      // Add the session's duration to cumulative time
      cumulativeTime += session.duration * 1000;
    });
  }

  private renderTimelineCommands(): void {
    this.elements.timelineCommands.innerHTML = "";

    this.timelineCommands.forEach((cmd) => {
      const marker = document.createElement("div");
      marker.className = "command-marker";
      marker.style.left = `${cmd.position}%`;
      marker.setAttribute("data-command", cmd.command);
      marker.addEventListener("click", (e) => {
        e.stopPropagation();
        this.seekToTime(cmd.startTime);
      });
      this.elements.timelineCommands.appendChild(marker);
    });
  }

  private togglePlayPause(): void {
    if (this.playbackState.isPlaying) {
      this.pause();
    } else {
      this.play();
    }
  }

  private play(): void {
    if (this.sessions.length === 0) return;

    this.playbackState.isPlaying = true;
    this.elements.playPauseBtn.classList.add("playing");
    this.startTime = Date.now() - this.playbackState.currentTime;

    this.playbackTimer = window.setInterval(() => {
      this.updatePlayback();
    }, 100);
  }

  private pause(): void {
    this.playbackState.isPlaying = false;
    this.elements.playPauseBtn.classList.remove("playing");

    if (this.playbackTimer) {
      clearInterval(this.playbackTimer);
      this.playbackTimer = null;
    }
  }

  private restart(): void {
    this.pause();
    this.playbackState.currentTime = 0;
    this.playbackState.currentSessionIndex = 0;
    this.playbackState.currentChunkIndex = 0;
    this.lastProcessedTime = 0;
    this.terminal.clear();
    this.updateUI();
  }

  private cycleSpeed(): void {
    const speeds = [0.5, 1, 1.5, 2, 3];
    const currentIndex = speeds.indexOf(this.playbackState.playbackSpeed);
    const nextIndex = (currentIndex + 1) % speeds.length;
    this.playbackState.playbackSpeed = speeds[nextIndex];
    this.elements.speedBtn.textContent = `${this.playbackState.playbackSpeed}x`;
    this.elements.speedBtn.setAttribute(
      "data-speed",
      this.playbackState.playbackSpeed.toString()
    );
  }

  private updatePlayback(): void {
    const now = Date.now();
    const elapsed = (now - this.startTime) * this.playbackState.playbackSpeed;
    this.playbackState.currentTime = elapsed;

    if (this.playbackState.currentTime >= this.playbackState.totalDuration) {
      this.pause();
      return;
    }

    // Only process chunks if we've moved forward significantly
    if (this.playbackState.currentTime > this.lastProcessedTime + 50) {
      this.processChunksUpToTime(this.playbackState.currentTime);
      this.lastProcessedTime = this.playbackState.currentTime;
    }
    this.updateUI();
  }

  private processChunksUpToTime(targetTime: number): void {
    let cumulativeTime = 0;

    // Calculate time up to current session
    for (let i = 0; i < this.playbackState.currentSessionIndex; i++) {
      cumulativeTime += this.sessions[i].duration * 1000;
    }

    for (
      let sessionIndex = this.playbackState.currentSessionIndex;
      sessionIndex < this.sessions.length;
      sessionIndex++
    ) {
      const session = this.sessions[sessionIndex];
      const sessionStartTime = cumulativeTime;

      // Process new session
      if (sessionIndex > this.playbackState.currentSessionIndex) {
        this.playbackState.currentSessionIndex = sessionIndex;
        this.playbackState.currentChunkIndex = 0;
        this.terminal.write(`\r\nrewindtty> ${session.command}\r\n`);
      }

      // Start from current chunk index
      const startChunkIndex = sessionIndex === this.playbackState.currentSessionIndex 
        ? this.playbackState.currentChunkIndex 
        : 0;

      for (
        let chunkIndex = startChunkIndex;
        chunkIndex < session.chunks.length;
        chunkIndex++
      ) {
        const chunk = session.chunks[chunkIndex];
        const chunkTime = sessionStartTime + chunk.time * 1000;

        if (chunkTime <= targetTime) {
          this.terminal.write(chunk.data);
          this.playbackState.currentChunkIndex = chunkIndex + 1;
        } else {
          return;
        }
      }

      // Move to next session
      cumulativeTime += session.duration * 1000;
      
      // If we've processed all chunks in this session and still haven't reached target time,
      // continue to next session
      if (cumulativeTime <= targetTime) {
        continue;
      } else {
        return;
      }
    }
  }

  private seekToPosition(event: MouseEvent): void {
    const rect = this.elements.timeline.getBoundingClientRect();
    const position = (event.clientX - rect.left) / rect.width;
    const targetTime = position * this.playbackState.totalDuration;
    this.seekToTime(targetTime);
  }

  private seekToTime(targetTime: number): void {
    this.pause();
    this.terminal.clear();
    this.playbackState.currentTime = targetTime;
    this.playbackState.currentSessionIndex = 0;
    this.playbackState.currentChunkIndex = 0;
    this.lastProcessedTime = 0;
    this.processChunksUpToTime(targetTime);
    this.updateUI();
  }

  private addBookmark(): void {
    const name = prompt("Enter bookmark name:");
    if (!name) return;

    const bookmark: Bookmark = {
      id: Date.now().toString(),
      name,
      time: this.playbackState.currentTime,
      sessionIndex: this.playbackState.currentSessionIndex,
      chunkIndex: this.playbackState.currentChunkIndex,
    };

    this.bookmarks.push(bookmark);
    this.bookmarks.sort((a, b) => a.time - b.time);
    this.saveBookmarks();
    this.renderBookmarks();
  }

  private clearBookmarks(): void {
    if (confirm("Clear all bookmarks?")) {
      this.bookmarks = [];
      this.saveBookmarks();
      this.renderBookmarks();
    }
  }

  private renderBookmarks(): void {
    this.elements.bookmarksContainer.innerHTML = "";

    this.bookmarks.forEach((bookmark) => {
      const bookmarkEl = document.createElement("div");
      bookmarkEl.className = "bookmark";
      bookmarkEl.textContent = bookmark.name;
      bookmarkEl.title = `${bookmark.name} - ${this.formatTime(bookmark.time)}`;

      bookmarkEl.addEventListener("click", () => {
        this.seekToTime(bookmark.time);
      });

      bookmarkEl.addEventListener("contextmenu", (e) => {
        e.preventDefault();
        if (confirm(`Delete bookmark "${bookmark.name}"?`)) {
          this.bookmarks = this.bookmarks.filter((b) => b.id !== bookmark.id);
          this.saveBookmarks();
          this.renderBookmarks();
        }
      });

      this.elements.bookmarksContainer.appendChild(bookmarkEl);
    });
  }

  private updateUI(): void {
    const progress =
      this.playbackState.totalDuration > 0
        ? (this.playbackState.currentTime / this.playbackState.totalDuration) *
          100
        : 0;

    this.elements.timelineProgress.style.width = `${progress}%`;
    this.elements.timelineScrubber.style.left = `${progress}%`;

    const currentSession =
      this.sessions[this.playbackState.currentSessionIndex];
    if (currentSession) {
      this.elements.currentCommand.textContent = currentSession.command;
    }

    this.elements.sessionTime.textContent = `${this.formatTime(
      this.playbackState.currentTime
    )} / ${this.formatTime(this.playbackState.totalDuration)}`;
  }

  private formatTime(ms: number): string {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds % 60;
    return `${minutes.toString().padStart(2, "0")}:${remainingSeconds
      .toString()
      .padStart(2, "0")}`;
  }

  private saveBookmarks(): void {
    localStorage.setItem("rewindtty-bookmarks", JSON.stringify(this.bookmarks));
  }

  private loadBookmarks(): void {
    const saved = localStorage.getItem("rewindtty-bookmarks");
    if (saved) {
      try {
        this.bookmarks = JSON.parse(saved);
      } catch (error) {
        console.error("Failed to load bookmarks:", error);
        this.bookmarks = [];
      }
    }
  }
}
