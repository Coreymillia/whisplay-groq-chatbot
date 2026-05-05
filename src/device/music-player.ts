import fs from "fs";
import path from "path";
import { spawn, ChildProcessWithoutNullStreams } from "child_process";
import { getAudioDurationInSeconds } from "get-audio-duration";
import { webAudioBridge } from "./web-audio-bridge";
import { musicDir } from "../utils/dir";

// Lazy imports to avoid circular dependencies
const lazyAudio = () => require("./audio") as { releaseAudioPlayer: () => Promise<void>; restoreAudioPlayer: () => void };
const lazyDisplay = () => require("./display") as { display: (s: Record<string, any>) => void };

type Track = {
  filePath: string;
  title: string;
  normalizedTitle: string;
};

type MatchResult = {
  track: Track;
  score: number;
};

type PlaybackMode = "single" | "ordered" | "shuffle";

export type ManagedMusicTrack = {
  fileName: string;
  title: string;
  filePath: string;
};

const DEFAULT_EXTENSIONS = ["mp3", "wav", "flac", "m4a", "aac", "ogg"];
const DEFAULT_MIN_SCORE = 0.35;
const DEFAULT_RESCAN_SECONDS = 30;

const stripFileExtension = (name: string): string => {
  const ext = path.extname(name);
  return ext ? name.slice(0, -ext.length) : name;
};

const prettifyStoredTrackTitle = (name: string): string => {
  return stripFileExtension(name)
    .replace(/^\d{13,}-/, "")
    .replace(/[\-_]+/g, " ")
    .replace(/\s+/g, " ")
    .trim();
};

const normalizeForSearch = (value: string): string => {
  return value
    .toLowerCase()
    .replace(/[\-_\.]+/g, " ")
    .replace(/[^\p{L}\p{N}\s]+/gu, " ")
    .replace(/\s+/g, " ")
    .trim();
};

const levenshteinDistance = (a: string, b: string): number => {
  if (a === b) return 0;
  if (a.length === 0) return b.length;
  if (b.length === 0) return a.length;

  const dp: number[][] = Array.from({ length: a.length + 1 }, () => []);
  for (let i = 0; i <= a.length; i++) dp[i][0] = i;
  for (let j = 0; j <= b.length; j++) dp[0][j] = j;

  for (let i = 1; i <= a.length; i++) {
    for (let j = 1; j <= b.length; j++) {
      const cost = a[i - 1] === b[j - 1] ? 0 : 1;
      dp[i][j] = Math.min(dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost);
    }
  }
  return dp[a.length][b.length];
};

const normalizedSimilarity = (a: string, b: string): number => {
  if (!a || !b) return 0;
  const maxLen = Math.max(a.length, b.length);
  return Math.max(0, 1 - levenshteinDistance(a, b) / maxLen);
};

const scoreTrack = (normalizedQuery: string, track: Track): number => {
  if (!normalizedQuery) return 0;
  const title = track.normalizedTitle;
  if (!title) return 0;
  if (title === normalizedQuery) return 1;
  if (title.includes(normalizedQuery)) {
    const penalty = Math.min(0.2, (title.length - normalizedQuery.length) / 200);
    return Math.max(0, 0.92 - penalty);
  }
  const queryTokens = normalizedQuery.split(" ").filter(Boolean);
  if (queryTokens.length > 0) {
    const tokenHits = queryTokens.filter((token) => title.includes(token)).length;
    const tokenRate = tokenHits / queryTokens.length;
    if (tokenRate >= 0.66) return 0.7 + tokenRate * 0.2;
  }
  return normalizedSimilarity(normalizedQuery, title);
};

const safeSplitCsv = (value: string | undefined): string[] => {
  return (value || "").split(",").map((s) => s.trim()).filter(Boolean);
};

const parseExtensions = (value: string | undefined): Set<string> => {
  const extList = safeSplitCsv(value).map((v) => v.toLowerCase().replace(/^\./, ""));
  return new Set(extList.length > 0 ? extList : DEFAULT_EXTENSIONS);
};

const parseDirectories = (value: string | undefined): string[] => {
  return safeSplitCsv(value).map((dir) => path.resolve(dir));
};

class LocalMusicPlayer {
  private static readonly MAX_PLAYBACK_RETRIES = 10;
  private static readonly RETRY_DELAY_MS = 1000;

  private tracks: Track[] = [];
  private currentProcess: ChildProcessWithoutNullStreams | null = null;
  private currentTrack: Track | null = null;
  private preloadPromise: Promise<void> | null = null;
  private isPlaying: boolean = false;
  private continuousPlay: boolean = false; // Whether to auto-play next track
  private pendingTrack: Track | null = null;
  private pendingContinuous: boolean = false;
  private playbackMode: PlaybackMode = "single";
  private pendingPlaybackMode: PlaybackMode = "single";
  private currentTrackIndex: number = -1;
  private pendingTrackIndex: number | null = null;
  private playbackHistory: number[] = [];
  private playbackGeneration: number = 0;
  private playbackRetries: number = 0;
  private trackChangeCallback: ((title: string) => void) | null = null;
  private playbackEndCallback: (() => void) | null = null;
  private progressTimer: ReturnType<typeof setInterval> | null = null;
  private playbackStartTime: number = 0;
  private currentTrackDurationMs: number = 0;

  onTrackChange(callback: ((title: string) => void) | null): void {
    this.trackChangeCallback = callback;
  }

  onPlaybackEnd(callback: (() => void) | null): void {
    this.playbackEndCallback = callback;
  }

  constructor(
    private readonly libraryDirs: string[],
    private readonly extensions: Set<string>,
    private readonly minScore: number,
    private readonly rescanSeconds: number,
    private readonly soundCardIndex: string,
  ) {}

  private isConfigured(): boolean {
    return this.libraryDirs.length > 0;
  }

  private async scanTracksIteratively(): Promise<void> {
    const foundTracks: Track[] = [];
    const visitedDirs = new Set<string>();
    const visitedFiles = new Set<string>();

    const normalizedRoots = Array.from(new Set(this.libraryDirs.map((dir) => path.resolve(dir))));

    for (const rootDirRaw of normalizedRoots) {
      if (!fs.existsSync(rootDirRaw)) continue;

      let rootDir = rootDirRaw;
      try {
        rootDir = await fs.promises.realpath(rootDirRaw);
      } catch {}
      if (visitedDirs.has(rootDir)) continue;
      visitedDirs.add(rootDir);

      const stack: string[] = [rootDir];
      while (stack.length > 0) {
        const currentDir = stack.pop();
        if (!currentDir) continue;

        let entries: fs.Dirent[] = [];
        try {
          entries = await fs.promises.readdir(currentDir, { withFileTypes: true });
        } catch {
          continue;
        }

        for (const entry of entries) {
          if (entry.name.startsWith(".")) continue;

          const fullPath = path.join(currentDir, entry.name);
          if (entry.isDirectory()) {
            let normalizedDir = fullPath;
            try {
              normalizedDir = await fs.promises.realpath(fullPath);
            } catch {}
            if (!visitedDirs.has(normalizedDir)) {
              visitedDirs.add(normalizedDir);
              stack.push(normalizedDir);
            }
            continue;
          }

          if (!entry.isFile()) continue;

          const ext = path.extname(entry.name).toLowerCase().replace(/^\./, "");
          if (!this.extensions.has(ext)) continue;

          let normalizedFile = fullPath;
          try {
            normalizedFile = await fs.promises.realpath(fullPath);
          } catch {}
          if (visitedFiles.has(normalizedFile)) continue;
          visitedFiles.add(normalizedFile);

          const title = prettifyStoredTrackTitle(entry.name);
          foundTracks.push({
            filePath: normalizedFile,
            title,
            normalizedTitle: normalizeForSearch(title),
          });
        }
      }
    }

    foundTracks.sort((a, b) => a.filePath.localeCompare(b.filePath));
    this.tracks = foundTracks;
  }

  preloadLibrary(): Promise<void> {
    if (!this.preloadPromise) {
      this.preloadPromise = this.scanTracksIteratively()
        .then(() => {
          console.log(`[Music] Indexed ${this.tracks.length} track(s)`);
        })
        .catch((err) => {
          console.error(`[Music] Failed to index: ${err?.message || err}`);
          this.tracks = [];
        });
    }
    return this.preloadPromise;
  }

  async refreshLibrary(): Promise<void> {
    this.preloadPromise = null;
    await this.preloadLibrary();
  }

  async listTracks(): Promise<ManagedMusicTrack[]> {
    await this.preloadLibrary();
    return this.tracks.map((track) => ({
      fileName: path.basename(track.filePath),
      title: track.title,
      filePath: track.filePath,
    }));
  }

  private findBestMatch(query: string): MatchResult | null {
    const normalizedQuery = normalizeForSearch(query);
    if (!normalizedQuery) return null;

    let best: MatchResult | null = null;
    for (const track of this.tracks) {
      const score = scoreTrack(normalizedQuery, track);
      if (score < this.minScore) continue;
      if (!best || score > best.score) best = { track, score };
    }
    return best;
  }

  private getRandomTrack(): Track | null {
    if (this.tracks.length === 0) return null;
    const index = Math.floor(Math.random() * this.tracks.length);
    return this.tracks[index];
  }

  private getRandomTrackIndex(excludeCurrent = false): number {
    if (this.tracks.length === 0) return -1;
    if (this.tracks.length === 1) return 0;
    let index = Math.floor(Math.random() * this.tracks.length);
    if (excludeCurrent && this.currentTrackIndex >= 0) {
      while (index === this.currentTrackIndex) {
        index = Math.floor(Math.random() * this.tracks.length);
      }
    }
    return index;
  }

  private getTrackIndex(track: Track): number {
    return this.tracks.findIndex((item) => item.filePath === track.filePath);
  }

  private stopProgressTimer(): void {
    if (this.progressTimer) {
      clearInterval(this.progressTimer);
      this.progressTimer = null;
    }
    // Clear progress bar from display
    try {
      lazyDisplay().display({ music_progress: -1, music_duration_ms: 0 });
    } catch {}
  }

  private resetProgressTimer(): void {
    if (this.progressTimer) {
      clearInterval(this.progressTimer);
      this.progressTimer = null;
    }
    // Restart the timer from 0 with the same duration
    if (this.currentTrackDurationMs > 0) {
      this.startProgressTimer(this.currentTrackDurationMs);
    }
  }

  private startProgressTimer(durationMs: number): void {
    this.stopProgressTimer();
    this.currentTrackDurationMs = durationMs;
    this.playbackStartTime = Date.now();
    // Send initial progress
    try {
      lazyDisplay().display({ music_progress: 0, music_duration_ms: durationMs });
    } catch {}
    this.progressTimer = setInterval(() => {
      if (!this.isPlaying || this.currentTrackDurationMs <= 0) {
        this.stopProgressTimer();
        return;
      }
      const elapsed = Date.now() - this.playbackStartTime;
      const progress = Math.min(1, elapsed / this.currentTrackDurationMs);
      try {
        lazyDisplay().display({ music_progress: progress, music_duration_ms: this.currentTrackDurationMs });
      } catch {}
    }, 1000);
  }

  private async getTrackDurationMs(filePath: string): Promise<number> {
    try {
      const sec = await getAudioDurationInSeconds(filePath);
      return Math.round(sec * 1000);
    } catch {
      return 0;
    }
  }

  private stopCurrentProcess(): void {
    this.playbackGeneration++;

    if (webAudioBridge.isAvailable()) {
      webAudioBridge.stopPlayback();
    }

    if (!this.currentProcess) {
      this.currentTrack = null;
      return;
    }
    try {
      this.currentProcess.kill("SIGINT");
    } catch {
      try {
        this.currentProcess.kill("SIGTERM");
      } catch {}
    }
    this.currentProcess = null;
    this.currentTrack = null;
  }

  private finalizePlayback(): void {
    this.isPlaying = false;
    this.playbackEndCallback?.();
  }

  /**
   * Spawn mpg123/sox for a track. On abnormal exit (code != 0), retries
   * the same track up to MAX_PLAYBACK_RETRIES times with a delay.
   * On normal exit, calls onNormalEnd. Stale exits from previous
   * generations are silently ignored.
   */
  private spawnAndPlay(track: Track, gen: number, onNormalEnd: () => void): void {
    const { command, args } = this.buildPlaybackCommand(track.filePath);
    const proc = spawn(command, args);
    this.currentProcess = proc;

    proc.on("error", (err) => {
      console.error(`[Music] Playback spawn error: ${err.message}`);
      if (gen === this.playbackGeneration) {
        this.currentProcess = null;
        this.currentTrack = null;
      }
    });

    proc.on("exit", (code, signal) => {
      if (gen !== this.playbackGeneration) return;

      if (code && code !== 0) {
        console.error(`[Music] Playback exited with code=${code} signal=${signal}`);
        this.currentProcess = null;
        this.resetProgressTimer();
        if (this.isPlaying && this.playbackRetries < LocalMusicPlayer.MAX_PLAYBACK_RETRIES) {
          this.playbackRetries++;
          console.log(`[Music] Retrying "${track.title}" (attempt ${this.playbackRetries}/${LocalMusicPlayer.MAX_PLAYBACK_RETRIES})...`);
          setTimeout(() => {
            if (gen === this.playbackGeneration && this.isPlaying) {
              this.spawnAndPlay(track, gen, onNormalEnd);
            }
          }, LocalMusicPlayer.RETRY_DELAY_MS);
          return;
        }
      }

      this.playbackRetries = 0;
      this.currentProcess = null;
      this.currentTrack = null;
      if (this.isPlaying) {
        onNormalEnd();
      }
    });

    console.log(`[Music] Playing: ${track.title}`);
    try {
      lazyDisplay().display({
        status: "music",
        text: `Now playing: ${track.title}`,
      });
    } catch {}
    this.trackChangeCallback?.(track.title);
  }

  private buildPlaybackCommand(filePath: string): { command: string; args: string[] } {
    const ext = path.extname(filePath).toLowerCase();
    // Use the "dmixed" ALSA device (dmix software mixer) so music playback
    // can coexist with the persistent TTS player without ALSA device conflicts.
    const alsaDevice = "dmixed";
    if (ext === ".mp3") {
      return {
        command: "mpg123",
        args: ["-o", "alsa", "-a", alsaDevice, filePath],
      };
    }
    return {
      command: "sox",
      args: [filePath, "-t", "alsa", alsaDevice],
    };
  }

  private async playViaWeb(filePath: string, onEnded?: () => void): Promise<boolean> {
    if (!webAudioBridge.isAvailable()) return false;

    try {
      const ext = path.extname(filePath).toLowerCase();
      const format = ext === ".mp3" ? "mp3" : "wav";
      const buffer = fs.readFileSync(filePath);
      const fileSizeMB = buffer.length / (1024 * 1024);
      const estimatedDuration = Math.min(600, Math.max(30, fileSizeMB * 40));

      await webAudioBridge.playAudioData(
        { buffer, duration: estimatedDuration * 1000, filePath },
        format as "mp3" | "wav"
      );

      if (onEnded) onEnded();
      return true;
    } catch (err: any) {
      console.error(`[Music] Web playback failed: ${err?.message}`);
      return false;
    }
  }

  private async playNextRandomTrack(): Promise<void> {
    if (!this.isPlaying) return;
    const nextIndex = this.getRandomTrackIndex(true);
    if (nextIndex < 0) {
      this.finalizePlayback();
      return;
    }
    await this.playTrackByIndex(nextIndex, "shuffle", true);
  }

  private async startPlayback(
    track: Track,
    continuous: boolean = false,
    playbackMode: PlaybackMode = continuous ? "shuffle" : "single",
    trackIndex: number = this.getTrackIndex(track),
  ): Promise<void> {
    this.stopCurrentProcess();
    this.stopProgressTimer();
    this.currentTrack = track;
    this.currentTrackIndex = trackIndex;
    this.isPlaying = true;
    this.continuousPlay = continuous;
    this.playbackMode = playbackMode;
    this.playbackRetries = 0;
    preferredMusicPlayer = this;

    const durationMs = await this.getTrackDurationMs(track.filePath);
    if (!this.isPlaying) return;

    // Callback when playback ends normally
    const onEnded = () => {
      this.stopProgressTimer();
      if (!this.isPlaying) {
        return;
      }
      if (this.playbackMode === "ordered") {
        const nextIndex = this.currentTrackIndex + 1;
        if (nextIndex >= 0 && nextIndex < this.tracks.length) {
          void this.playTrackByIndex(nextIndex, "ordered", true);
          return;
        }
      } else if (this.playbackMode === "shuffle" || this.continuousPlay) {
        void this.playNextRandomTrack();
        return;
      }
      this.finalizePlayback();
    };

    const playedViaWeb = await this.playViaWeb(track.filePath, onEnded);
    if (playedViaWeb) {
      if (durationMs > 0) this.startProgressTimer(durationMs);
      return;
    }

    // Release the persistent TTS player so ALSA is completely free.
    // Music uses the "dmixed" device (dmix mixer) which can coexist, but
    // releasing first avoids any contention on the underlying hardware.
    await lazyAudio().releaseAudioPlayer();
    if (!this.isPlaying) return;

    const gen = this.playbackGeneration;
    this.spawnAndPlay(track, gen, onEnded);
    if (durationMs > 0) this.startProgressTimer(durationMs);
  }

  private async playTrackByIndex(
    index: number,
    mode: PlaybackMode,
    pushHistory: boolean,
  ): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    await this.preloadLibrary();
    if (this.tracks.length === 0) {
      return { ok: false, message: "No music files found." };
    }
    if (index < 0 || index >= this.tracks.length) {
      return { ok: false, message: "Track index out of range." };
    }
    if (pushHistory && this.currentTrackIndex >= 0) {
      this.playbackHistory.push(this.currentTrackIndex);
    }
    const track = this.tracks[index];
    await this.startPlayback(track, mode !== "single", mode, index);
    return {
      ok: true,
      message: `Playing: ${track.title}`,
      trackPath: track.filePath,
      trackTitle: track.title,
    };
  }

  async playByQuery(query: string, continuous: boolean = false): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    if (!this.isConfigured()) {
      return { ok: false, message: "Music library not configured." };
    }

    await this.preloadLibrary();
    if (this.tracks.length === 0) {
      return { ok: false, message: "No music files found." };
    }

    const best = this.findBestMatch(query);
    if (!best) {
      return { ok: false, message: `No matching track found for "${query}"` };
    }

    await this.startPlayback(
      best.track,
      continuous,
      continuous ? "shuffle" : "single",
      this.getTrackIndex(best.track),
    );

    return {
      ok: true,
      message: `Playing: ${best.track.title}`,
      trackPath: best.track.filePath,
      trackTitle: best.track.title,
    };
  }

  async playRandom(continuous: boolean = true): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    if (!this.isConfigured()) {
      return { ok: false, message: "Music library not configured." };
    }

    await this.preloadLibrary();
    if (this.tracks.length === 0) {
      return { ok: false, message: "No music files found." };
    }

    const track = this.getRandomTrack();
    if (!track) {
      return { ok: false, message: "Could not select a random track." };
    }

    await this.startPlayback(
      track,
      continuous,
      continuous ? "shuffle" : "single",
      this.getTrackIndex(track),
    );

    return {
      ok: true,
      message: `Playing: ${track.title}`,
      trackPath: track.filePath,
      trackTitle: track.title,
    };
  }

  async findByQuery(query: string, continuous: boolean = false): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    if (!this.isConfigured()) {
      return { ok: false, message: "Music library not configured." };
    }

    await this.preloadLibrary();
    if (this.tracks.length === 0) {
      return { ok: false, message: "No music files found." };
    }

    const best = this.findBestMatch(query);
    if (!best) {
      return { ok: false, message: `No matching track found for "${query}"` };
    }

    this.pendingTrack = best.track;
    this.pendingContinuous = continuous;
    this.pendingPlaybackMode = continuous ? "shuffle" : "single";
    this.pendingTrackIndex = this.getTrackIndex(best.track);
    preferredMusicPlayer = this;

    return {
      ok: true,
      message: `Playing: ${best.track.title}`,
      trackPath: best.track.filePath,
      trackTitle: best.track.title,
    };
  }

  async findRandom(continuous: boolean = true): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    if (!this.isConfigured()) {
      return { ok: false, message: "Music library not configured." };
    }

    await this.preloadLibrary();
    if (this.tracks.length === 0) {
      return { ok: false, message: "No music files found." };
    }

    const track = this.getRandomTrack();
    if (!track) {
      return { ok: false, message: "Could not select a random track." };
    }

    this.pendingTrack = track;
    this.pendingContinuous = continuous;
    this.pendingPlaybackMode = continuous ? "shuffle" : "single";
    this.pendingTrackIndex = this.getTrackIndex(track);
    preferredMusicPlayer = this;

    return {
      ok: true,
      message: `Playing: ${track.title}`,
      trackPath: track.filePath,
      trackTitle: track.title,
    };
  }

  startPendingPlayback(): void {
    if (!this.pendingTrack) return;
    const track = this.pendingTrack;
    const continuous = this.pendingContinuous;
    const playbackMode = this.pendingPlaybackMode;
    const trackIndex = this.pendingTrackIndex ?? this.getTrackIndex(track);
    this.pendingTrack = null;
    this.pendingTrackIndex = null;
    void this.startPlayback(track, continuous, playbackMode, trackIndex);
  }

  hasPendingPlayback(): boolean {
    return Boolean(this.pendingTrack);
  }

  async prepareManagedLibraryPlayback(
    shuffle: boolean,
  ): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    if (!this.isConfigured()) {
      return { ok: false, message: "Music library not configured." };
    }
    await this.preloadLibrary();
    if (this.tracks.length === 0) {
      return { ok: false, message: "No MP3 files uploaded yet." };
    }
    this.playbackHistory = [];
    const nextIndex = shuffle ? this.getRandomTrackIndex(false) : 0;
    if (nextIndex < 0) {
      return { ok: false, message: "No music files found." };
    }
    this.pendingTrack = this.tracks[nextIndex];
    this.pendingContinuous = true;
    this.pendingPlaybackMode = shuffle ? "shuffle" : "ordered";
    this.pendingTrackIndex = nextIndex;
    preferredMusicPlayer = this;
    return {
      ok: true,
      message: `Playing: ${this.tracks[nextIndex].title}`,
      trackPath: this.tracks[nextIndex].filePath,
      trackTitle: this.tracks[nextIndex].title,
    };
  }

  async playManagedLibrary(
    shuffle: boolean,
  ): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    const prepared = await this.prepareManagedLibraryPlayback(shuffle);
    if (!prepared.ok) {
      return prepared;
    }
    this.startPendingPlayback();
    return prepared;
  }

  async prepareNextTrack(): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    await this.preloadLibrary();
    if (this.tracks.length === 0) {
      return { ok: false, message: "No MP3 files uploaded yet." };
    }
    const baseMode = this.playbackMode === "shuffle" ? "shuffle" : "ordered";
    const nextIndex =
      baseMode === "shuffle"
        ? this.getRandomTrackIndex(true)
        : this.currentTrackIndex >= 0
          ? (this.currentTrackIndex + 1) % this.tracks.length
          : 0;
    if (nextIndex < 0) {
      return { ok: false, message: "No next track available." };
    }
    if (this.currentTrackIndex >= 0) {
      this.playbackHistory.push(this.currentTrackIndex);
    }
    this.pendingTrack = this.tracks[nextIndex];
    this.pendingContinuous = true;
    this.pendingPlaybackMode = baseMode;
    this.pendingTrackIndex = nextIndex;
    preferredMusicPlayer = this;
    return {
      ok: true,
      message: `Playing: ${this.tracks[nextIndex].title}`,
      trackPath: this.tracks[nextIndex].filePath,
      trackTitle: this.tracks[nextIndex].title,
    };
  }

  async nextTrack(): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    const prepared = await this.prepareNextTrack();
    if (!prepared.ok) {
      return prepared;
    }
    this.startPendingPlayback();
    return prepared;
  }

  async preparePreviousTrack(): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    await this.preloadLibrary();
    if (this.tracks.length === 0) {
      return { ok: false, message: "No MP3 files uploaded yet." };
    }
    let previousIndex = this.playbackHistory.pop();
    if (previousIndex === undefined) {
      previousIndex =
        this.currentTrackIndex > 0
          ? this.currentTrackIndex - 1
          : 0;
    }
    this.pendingTrack = this.tracks[previousIndex];
    this.pendingContinuous = true;
    this.pendingPlaybackMode = this.playbackMode === "shuffle" ? "shuffle" : "ordered";
    this.pendingTrackIndex = previousIndex;
    preferredMusicPlayer = this;
    return {
      ok: true,
      message: `Playing: ${this.tracks[previousIndex].title}`,
      trackPath: this.tracks[previousIndex].filePath,
      trackTitle: this.tracks[previousIndex].title,
    };
  }

  async previousTrack(): Promise<{
    ok: boolean;
    message: string;
    trackPath?: string;
    trackTitle?: string;
  }> {
    const prepared = await this.preparePreviousTrack();
    if (!prepared.ok) {
      return prepared;
    }
    this.startPendingPlayback();
    return prepared;
  }

  stop(): void {
    this.isPlaying = false;
    this.pendingTrack = null;
    this.pendingTrackIndex = null;
    this.stopProgressTimer();
    this.stopCurrentProcess();
    // Restore the persistent TTS player after releasing
    lazyAudio().restoreAudioPlayer();
    console.log("[Music] Playback stopped");
  }

  isMusicPlaying(): boolean {
    return this.isPlaying;
  }

  getCurrentTrack(): Track | null {
    return this.currentTrack;
  }
}

let localMusicPlayerInstance: LocalMusicPlayer | null = null;
let localMusicPlayerKey = "";
let managedMusicPlayerInstance: LocalMusicPlayer | null = null;
let preferredMusicPlayer: LocalMusicPlayer | null = null;

export const getLocalMusicPlayer = (env: Record<string, string | undefined>): LocalMusicPlayer => {
  const dirs = parseDirectories(env.MUSIC_LIBRARY_DIRS);
  const extensions = parseExtensions(env.MUSIC_FILE_EXTENSIONS);
  const minScoreRaw = parseFloat(env.MUSIC_FUZZY_MIN_SCORE || "");
  const minScore = Number.isFinite(minScoreRaw) ? Math.min(1, Math.max(0, minScoreRaw)) : DEFAULT_MIN_SCORE;
  const rescanRaw = parseInt(env.MUSIC_RESCAN_SECONDS || "", 60);
  const rescanSeconds = Number.isFinite(rescanRaw) && rescanRaw > 0 ? rescanRaw : DEFAULT_RESCAN_SECONDS;
  const soundCardIndex = env.SOUND_CARD_INDEX || "1";

  const key = JSON.stringify({
    dirs,
    extensions: Array.from(extensions.values()).sort(),
    minScore,
    rescanSeconds,
    soundCardIndex,
  });

  if (!localMusicPlayerInstance || key !== localMusicPlayerKey) {
    localMusicPlayerInstance = new LocalMusicPlayer(dirs, extensions, minScore, rescanSeconds, soundCardIndex);
    localMusicPlayerKey = key;
    void localMusicPlayerInstance.preloadLibrary();
  }

  return localMusicPlayerInstance;
};

export const getManagedMusicPlayer = (
  env: Record<string, string | undefined>,
): LocalMusicPlayer => {
  if (!managedMusicPlayerInstance) {
    managedMusicPlayerInstance = new LocalMusicPlayer(
      [musicDir],
      new Set(["mp3"]),
      DEFAULT_MIN_SCORE,
      DEFAULT_RESCAN_SECONDS,
      env.SOUND_CARD_INDEX || "1",
    );
    void managedMusicPlayerInstance.preloadLibrary();
  }
  return managedMusicPlayerInstance;
};

const getPreferredMusicPlayer = (): LocalMusicPlayer | null => {
  if (preferredMusicPlayer) {
    return preferredMusicPlayer;
  }
  if (managedMusicPlayerInstance?.isMusicPlaying() || managedMusicPlayerInstance?.hasPendingPlayback()) {
    return managedMusicPlayerInstance;
  }
  if (localMusicPlayerInstance?.isMusicPlaying() || localMusicPlayerInstance?.hasPendingPlayback()) {
    return localMusicPlayerInstance;
  }
  return managedMusicPlayerInstance || localMusicPlayerInstance;
};

export const stopMusicPlayback = (): void => {
  managedMusicPlayerInstance?.stop();
  localMusicPlayerInstance?.stop();
};

export const isMusicPlaying = (): boolean => {
  return Boolean(
    managedMusicPlayerInstance?.isMusicPlaying() ||
    localMusicPlayerInstance?.isMusicPlaying(),
  );
};

export const getCurrentTrackTitle = (): string => {
  return (
    managedMusicPlayerInstance?.getCurrentTrack()?.title ||
    localMusicPlayerInstance?.getCurrentTrack()?.title ||
    ""
  );
};

export const startPendingMusicPlayback = (): void => {
  getPreferredMusicPlayer()?.startPendingPlayback();
};

export const onMusicTrackChange = (callback: ((title: string) => void) | null): void => {
  getPreferredMusicPlayer()?.onTrackChange(callback);
};

export const onMusicPlaybackEnd = (callback: (() => void) | null): void => {
  getPreferredMusicPlayer()?.onPlaybackEnd(callback);
};
