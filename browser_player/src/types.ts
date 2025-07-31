export interface SessionChunk {
    time: number;
    size: number;
    data: string;
}

export interface Session {
    command: string;
    start_time: number;
    end_time: number;
    duration: number;
    chunks: SessionChunk[];
}

export interface Bookmark {
    id: string;
    name: string;
    time: number;
    sessionIndex: number;
    chunkIndex: number;
}

export interface PlaybackState {
    isPlaying: boolean;
    currentTime: number;
    totalDuration: number;
    playbackSpeed: number;
    currentSessionIndex: number;
    currentChunkIndex: number;
}

export interface TimelineCommand {
    sessionIndex: number;
    command: string;
    startTime: number;
    position: number;
}