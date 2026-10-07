export interface RemuxProgress {
  bytesRead: number;
  bytesWritten: number;
  timestamp: number | null;
}

export interface RemuxOptions {
  /** Output container: "mp4", "mov", "matroska", "webm", "mpegts". Required: a sink has no filename to sniff. */
  format: string;
  /** Input stream indices to include. Default: all. */
  streams?: number[];
  /** Default 'skip': drop streams the muxer rejects and report them in the result. */
  onIncompatibleStream?: 'skip' | 'fail';
  /** Fragmented MP4 (frag_keyframe+empty_moov). Required when the sink is not seekable. */
  fragmented?: boolean;
  signal?: AbortSignal;
  onProgress?: (progress: RemuxProgress) => void;
}

export interface RemuxStreamMapping {
  input: number;
  output: number | null;
  skippedReason?: string;
}

export interface RemuxResult {
  streams: RemuxStreamMapping[];
  bytesRead: number;
  bytesWritten: number;
  packets: number;
}
