import { AviotrixError } from './error.js';
import type {
  AudioStreamInfo,
  Metadata,
  Rational,
  StreamInfo,
  StreamType,
  VideoStreamInfo,
} from './metadata.js';
import type { RemuxResult, RemuxStreamMapping } from './remux.js';

type JsonValue = string | number | boolean | null | JsonValue[] | { [key: string]: JsonValue };
type JsonObject = { [key: string]: JsonValue };

function malformed(what: string): AviotrixError {
  return new AviotrixError('MALFORMED_RESULT', `malformed result from native code: ${what}`);
}

function parseJson(text: string): JsonValue {
  try {
    return JSON.parse(text) as JsonValue;
  } catch {
    throw malformed('not valid JSON');
  }
}

function obj(v: JsonValue, what: string): JsonObject {
  if (v === null || typeof v !== 'object' || Array.isArray(v))
    throw malformed(`${what} is not an object`);
  return v;
}
function arr(v: JsonValue | undefined, what: string): JsonValue[] {
  if (!Array.isArray(v)) throw malformed(`${what} is not an array`);
  return v;
}
function str(v: JsonValue | undefined, what: string): string {
  if (typeof v !== 'string') throw malformed(`${what} is not a string`);
  return v;
}
function num(v: JsonValue | undefined, what: string): number {
  if (typeof v !== 'number') throw malformed(`${what} is not a number`);
  return v;
}
function numOrNull(v: JsonValue | undefined, what: string): number | null {
  if (v === null || v === undefined) return null;
  return num(v, what);
}
function strOrNull(v: JsonValue | undefined, what: string): string | null {
  if (v === null || v === undefined) return null;
  return str(v, what);
}
function tags(v: JsonValue | undefined, what: string): Record<string, string> {
  const o = obj(v ?? {}, what);
  const out: Record<string, string> = {};
  for (const [k, val] of Object.entries(o)) out[k] = str(val, `${what}.${k}`);
  return out;
}
function rational(v: JsonValue | undefined, what: string): Rational {
  const o = obj(v ?? null, what);
  return { num: num(o['num'], `${what}.num`), den: num(o['den'], `${what}.den`) };
}
function rationalOrNull(v: JsonValue | undefined, what: string): Rational | null {
  if (v === null || v === undefined) return null;
  return rational(v, what);
}

const streamTypes: ReadonlySet<string> = new Set([
  'video',
  'audio',
  'subtitle',
  'data',
  'attachment',
]);

function streamType(v: JsonValue | undefined, what: string): StreamType {
  const s = str(v, what);
  if (!streamTypes.has(s)) throw malformed(`${what} has unknown stream type ${s}`);
  return s as StreamType;
}

function video(v: JsonValue, what: string): VideoStreamInfo {
  const o = obj(v, what);
  return {
    width: num(o['width'], `${what}.width`),
    height: num(o['height'], `${what}.height`),
    frameRate: rationalOrNull(o['frameRate'], `${what}.frameRate`),
    pixelFormat: strOrNull(o['pixelFormat'], `${what}.pixelFormat`),
  };
}

function audio(v: JsonValue, what: string): AudioStreamInfo {
  const o = obj(v, what);
  return {
    sampleRate: num(o['sampleRate'], `${what}.sampleRate`),
    channels: num(o['channels'], `${what}.channels`),
    channelLayout: strOrNull(o['channelLayout'], `${what}.channelLayout`),
  };
}

function stream(v: JsonValue, what: string): StreamInfo {
  const o = obj(v, what);
  const info: StreamInfo = {
    index: num(o['index'], `${what}.index`),
    type: streamType(o['type'], `${what}.type`),
    codec: str(o['codec'], `${what}.codec`),
    codecTag: strOrNull(o['codecTag'], `${what}.codecTag`),
    timeBase: rational(o['timeBase'], `${what}.timeBase`),
    startTime: numOrNull(o['startTime'], `${what}.startTime`),
    duration: numOrNull(o['duration'], `${what}.duration`),
    bitRate: numOrNull(o['bitRate'], `${what}.bitRate`),
    language: strOrNull(o['language'], `${what}.language`),
    tags: tags(o['tags'], `${what}.tags`),
  };
  if (o['video'] !== undefined) info.video = video(o['video'], `${what}.video`);
  if (o['audio'] !== undefined) info.audio = audio(o['audio'], `${what}.audio`);
  return info;
}

export function parseMetadataJson(text: string): Metadata {
  const o = obj(parseJson(text), 'metadata');
  return {
    format: str(o['format'], 'metadata.format'),
    formatLongName: str(o['formatLongName'], 'metadata.formatLongName'),
    startTime: numOrNull(o['startTime'], 'metadata.startTime'),
    duration: numOrNull(o['duration'], 'metadata.duration'),
    bitRate: numOrNull(o['bitRate'], 'metadata.bitRate'),
    tags: tags(o['tags'], 'metadata.tags'),
    streams: arr(o['streams'], 'metadata.streams').map((s, i) =>
      stream(s, `metadata.streams[${i}]`),
    ),
  };
}

function mapping(v: JsonValue, what: string): RemuxStreamMapping {
  const o = obj(v, what);
  const m: RemuxStreamMapping = {
    input: num(o['input'], `${what}.input`),
    output: numOrNull(o['output'], `${what}.output`),
  };
  if (o['skippedReason'] !== undefined && o['skippedReason'] !== null) {
    m.skippedReason = str(o['skippedReason'], `${what}.skippedReason`);
  }
  return m;
}

export function parseRemuxResultJson(text: string): RemuxResult {
  const o = obj(parseJson(text), 'remuxResult');
  return {
    streams: arr(o['streams'], 'remuxResult.streams').map((s, i) =>
      mapping(s, `remuxResult.streams[${i}]`),
    ),
    bytesRead: num(o['bytesRead'], 'remuxResult.bytesRead'),
    bytesWritten: num(o['bytesWritten'], 'remuxResult.bytesWritten'),
    packets: num(o['packets'], 'remuxResult.packets'),
  };
}
