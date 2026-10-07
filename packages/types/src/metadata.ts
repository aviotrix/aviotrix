export interface Rational {
  num: number;
  den: number;
}

export type StreamType = 'video' | 'audio' | 'subtitle' | 'data' | 'attachment';

export interface VideoStreamInfo {
  width: number;
  height: number;
  frameRate: Rational | null;
  pixelFormat: string | null;
}

export interface AudioStreamInfo {
  sampleRate: number;
  channels: number;
  channelLayout: string | null;
}

export interface StreamInfo {
  index: number;
  type: StreamType;
  codec: string;
  codecTag: string | null;
  timeBase: Rational;
  startTime: number | null;
  duration: number | null;
  bitRate: number | null;
  language: string | null;
  tags: Record<string, string>;
  video?: VideoStreamInfo;
  audio?: AudioStreamInfo;
}

export interface Metadata {
  format: string;
  formatLongName: string;
  startTime: number | null;
  duration: number | null;
  bitRate: number | null;
  tags: Record<string, string>;
  streams: StreamInfo[];
}
