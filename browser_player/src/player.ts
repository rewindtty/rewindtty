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
  private isDragging: boolean = false;
  private wasPlayingBeforeDrag: boolean = false;

  private elements: {
    currentCommand: HTMLElement;
    sessionTime: HTMLElement;
    playPauseBtn: HTMLButtonElement;
    restartBtn: HTMLButtonElement;
    speedBtn: HTMLButtonElement;
    timeline: HTMLElement;
    timelineProgress: HTMLElement;
    timelineCommands: HTMLElement;
    timelineScrubber: HTMLElement;
    bookmarksContainer: HTMLElement;
    addBookmarkBtn: HTMLButtonElement;
    clearBookmarksBtn: HTMLButtonElement;
    fileInput: HTMLInputElement;
    loadFileBtn: HTMLButtonElement;
    modal: HTMLElement;
    modalFileInput: HTMLInputElement;
    modalLoadFileBtn: HTMLButtonElement;
    commandListBtn: HTMLButtonElement;
    commandSidebar: HTMLElement;
    closeSidebarBtn: HTMLButtonElement;
    commandList: HTMLElement;
    terminalContainer: HTMLElement;
    tooltip: HTMLElement;
  };

  private isJsonLoaded: boolean = false;

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
      modal: document.getElementById("json-modal")!,
      modalFileInput: document.getElementById(
        "modal-file-input"
      ) as HTMLInputElement,
      modalLoadFileBtn: document.getElementById("modal-load-file-btn")!,
      commandListBtn: document.getElementById("command-list-btn")!,
      commandSidebar: document.getElementById("command-sidebar")!,
      closeSidebarBtn: document.getElementById("close-sidebar-btn")!,
      commandList: document.getElementById("command-list")!,
      terminalContainer: document.querySelector(".terminal-container")!,
      tooltip: this.createTooltip(),
    };
  }

  private createTooltip(): HTMLElement {
    const tooltip = document.createElement("div");
    tooltip.className = "command-tooltip";
    document.body.appendChild(tooltip);
    return tooltip;
  }

  private setupEventListeners(): void {
    this.elements.playPauseBtn.addEventListener("click", () =>
      this.togglePlayPause()
    );
    this.elements.restartBtn.addEventListener("click", () => this.restart());
    this.elements.speedBtn.addEventListener("click", () => this.cycleSpeed());
    this.elements.timeline.addEventListener("click", (e) => {
      if (!this.isDragging) {
        this.seekToPosition(e);
      }
    });

    this.elements.timeline.addEventListener("mousedown", (e) =>
      this.handleTimelineMouseDown(e)
    );

    this.elements.timeline.addEventListener("wheel", (e) =>
      this.handleTimelineWheel(e)
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
    this.elements.modalLoadFileBtn.addEventListener("click", () =>
      this.elements.modalFileInput.click()
    );
    this.elements.modalFileInput.addEventListener("change", (e) =>
      this.handleModalFileLoad(e)
    );
    this.elements.commandListBtn.addEventListener("click", () =>
      this.toggleCommandSidebar()
    );
    this.elements.closeSidebarBtn.addEventListener("click", () =>
      this.toggleCommandSidebar()
    );

    window.addEventListener("resize", () => this.fitAddon.fit());

    window.addEventListener("mousemove", (e) => this.handleMouseMove(e));
    window.addEventListener("mouseup", () => this.handleMouseUp());

    window.addEventListener("keydown", (e) => {
      if (!this.isJsonLoaded) return;

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

    // Show modal on startup and disable controls
    this.showModal();
    this.disableControls();
  }

  private showModal(): void {
    this.elements.modal.style.display = "flex";
  }

  private hideModal(): void {
    this.elements.modal.style.display = "none";
  }

  private disableControls(): void {
    this.elements.playPauseBtn.disabled = true;
    this.elements.restartBtn.disabled = true;
    this.elements.speedBtn.disabled = true;
    this.elements.addBookmarkBtn.disabled = true;
    this.elements.clearBookmarksBtn.disabled = true;
    this.elements.commandListBtn.disabled = true;
    this.elements.timeline.style.pointerEvents = "none";
    this.elements.timeline.style.opacity = "0.5";
  }

  private enableControls(): void {
    this.elements.playPauseBtn.disabled = false;
    this.elements.restartBtn.disabled = false;
    this.elements.speedBtn.disabled = false;
    this.elements.addBookmarkBtn.disabled = false;
    this.elements.clearBookmarksBtn.disabled = false;
    this.elements.commandListBtn.disabled = false;
    this.elements.timeline.style.pointerEvents = "auto";
    this.elements.timeline.style.opacity = "1";
    this.elements.loadFileBtn.style.display = "inline-block";
  }

  private handleModalFileLoad(event: Event): void {
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
        this.isJsonLoaded = true;
        this.hideModal();
        this.enableControls();
        this.play();
      } catch (error) {
        console.error("Failed to parse JSON file:", error);
        alert(
          "Error: Invalid JSON file. Please select a valid RewindTTY JSON file."
        );
      }
    };

    reader.readAsText(file);
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
    this.renderCommandList();
    this.updateUI();
  }

  private toggleCommandSidebar(): void {
    const isOpen = this.elements.commandSidebar.classList.contains("open");

    if (isOpen) {
      this.elements.commandSidebar.classList.remove("open");
      this.elements.terminalContainer.classList.remove("sidebar-open");
    } else {
      this.elements.commandSidebar.classList.add("open");
      this.elements.terminalContainer.classList.add("sidebar-open");
    }

    // Resize terminal after animation
    setTimeout(() => this.fitAddon.fit(), 300);
  }

  private renderCommandList(): void {
    this.elements.commandList.innerHTML = "";

    this.timelineCommands.forEach((cmd, index) => {
      const commandItem = document.createElement("div");
      commandItem.className = "command-item";

      const commandText = document.createElement("div");
      commandText.className = "command-text";
      commandText.textContent = cmd.command;

      const commandTime = document.createElement("div");
      commandTime.className = "command-time";
      commandTime.textContent = this.formatTime(cmd.startTime);

      commandItem.appendChild(commandText);
      commandItem.appendChild(commandTime);

      commandItem.addEventListener("click", () => {
        this.seekToTime(cmd.startTime);
        this.updateActiveCommand(index);
      });

      this.elements.commandList.appendChild(commandItem);
    });
  }

  private updateActiveCommand(activeIndex: number): void {
    const commandItems =
      this.elements.commandList.querySelectorAll(".command-item");
    commandItems.forEach((item, index) => {
      if (index === activeIndex) {
        item.classList.add("active");
      } else {
        item.classList.remove("active");
      }
    });
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

      marker.addEventListener("mouseenter", (e) => {
        this.showTooltip(e.target as HTMLElement, cmd.command);
      });

      marker.addEventListener("mouseleave", () => {
        this.hideTooltip();
      });

      this.elements.timelineCommands.appendChild(marker);
    });
  }

  private togglePlayPause(): void {
    if (!this.isJsonLoaded) return;

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
      const startChunkIndex =
        sessionIndex === this.playbackState.currentSessionIndex
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
    if (!this.isJsonLoaded) return;

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
      this.updateActiveCommand(this.playbackState.currentSessionIndex);
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

  private showTooltip(element: HTMLElement, command: string): void {
    this.elements.tooltip.textContent = command;
    this.elements.tooltip.style.display = "block";
    this.elements.tooltip.classList.add("visible");

    const rect = element.getBoundingClientRect();

    // Position tooltip above the marker
    this.elements.tooltip.style.left = `${rect.left + rect.width / 2}px`;
    this.elements.tooltip.style.top = `${rect.top - 8}px`;
    this.elements.tooltip.style.transform =
      "translateX(-50%) translateY(-100%)";
  }

  private hideTooltip(): void {
    this.elements.tooltip.style.display = "none";
    this.elements.tooltip.classList.remove("visible");
  }

  private handleTimelineMouseDown(event: MouseEvent): void {
    if (!this.isJsonLoaded || event.button !== 0) return; // Only handle left mouse button

    this.isDragging = true;
    this.wasPlayingBeforeDrag = this.playbackState.isPlaying;

    if (this.playbackState.isPlaying) {
      this.pause();
    }

    this.seekToPosition(event);
    event.preventDefault();
  }

  private handleMouseMove(event: MouseEvent): void {
    if (!this.isDragging || !this.isJsonLoaded) return;

    const rect = this.elements.timeline.getBoundingClientRect();
    if (event.clientX >= rect.left && event.clientX <= rect.right) {
      this.seekToPosition(event);
    }
  }

  private handleMouseUp(): void {
    if (!this.isDragging) return;

    this.isDragging = false;

    if (this.wasPlayingBeforeDrag) {
      this.play();
    }
  }

  private handleTimelineWheel(event: WheelEvent): void {
    if (!this.isJsonLoaded) return;

    event.preventDefault();

    const wasPlaying = this.playbackState.isPlaying;
    if (wasPlaying) {
      this.pause();
    }

    // Calculate new time based on wheel delta
    const wheelSensitivity = 1000; // milliseconds per wheel tick
    const deltaTime = event.deltaY > 0 ? -wheelSensitivity : wheelSensitivity;
    const newTime = Math.max(
      0,
      Math.min(
        this.playbackState.currentTime + deltaTime,
        this.playbackState.totalDuration
      )
    );

    this.seekToTime(newTime);

    if (wasPlaying) {
      // Resume playing after a short delay
      setTimeout(() => {
        if (!this.isDragging) {
          this.play();
        }
      }, 100);
    }
  }
}
