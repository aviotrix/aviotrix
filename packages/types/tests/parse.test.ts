import { describe, expect, it } from 'vitest';
import { AviotrixError, parseMetadataJson, parseRemuxResultJson } from '../src/index.js';

const metadataJson = JSON.stringify({
  format: 'mpegts',
  formatLongName: 'MPEG-TS (MPEG-2 Transport Stream)',
  startTime: 1.4,
  duration: 3.0,
  bitRate: null,
  tags: {},
  streams: [
    {
      index: 0,
      type: 'video',
      codec: 'h264',
      codecTag: null,
      timeBase: { num: 1, den: 90000 },
      startTime: 1.4,
      duration: null,
      bitRate: null,
      language: null,
      tags: {},
      video: { width: 320, height: 240, frameRate: { num: 30, den: 1 }, pixelFormat: 'yuv420p' },
    },
  ],
});

describe('parseMetadataJson', () => {
  it('returns a typed Metadata', () => {
    const m = parseMetadataJson(metadataJson);
    expect(m.format).toBe('mpegts');
    expect(m.streams[0]?.video?.width).toBe(320);
    expect(m.streams[0]?.audio).toBeUndefined();
    expect(m.bitRate).toBeNull();
  });

  it('rejects malformed input with MALFORMED_RESULT', () => {
    expect(() => parseMetadataJson('{"format":1}')).toThrowError(AviotrixError);
    try {
      parseMetadataJson('not json');
    } catch (e) {
      expect((e as AviotrixError).code).toBe('MALFORMED_RESULT');
    }
  });
});

describe('parseRemuxResultJson', () => {
  it('maps skipped streams', () => {
    const r = parseRemuxResultJson(
      '{"streams":[{"input":0,"output":0},{"input":2,"output":null,"skippedReason":"nope"}],"bytesRead":1,"bytesWritten":2,"packets":3}',
    );
    expect(r.streams[1]?.output).toBeNull();
    expect(r.streams[1]?.skippedReason).toBe('nope');
    expect(r.streams[0]?.skippedReason).toBeUndefined();
    expect(r.packets).toBe(3);
  });
});
