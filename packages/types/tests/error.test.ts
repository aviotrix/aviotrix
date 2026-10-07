import { describe, expect, it } from 'vitest';
import { AviotrixError, toAviotrixError } from '../src/index.js';

describe('AviotrixError', () => {
  it('carries a code and is an Error', () => {
    const e = new AviotrixError('SINK_NOT_SEEKABLE', 'mp4 needs a seekable sink');
    expect(e).toBeInstanceOf(Error);
    expect(e).toBeInstanceOf(AviotrixError);
    expect(e.code).toBe('SINK_NOT_SEEKABLE');
    expect(e.message).toBe('mp4 needs a seekable sink');
    expect(e.name).toBe('AviotrixError');
  });

  it('converts a native failure record', () => {
    const e = toAviotrixError({
      code: 'AVERROR_INVALIDDATA',
      message: 'avformat_open_input: Invalid data',
    });
    expect(e.code).toBe('AVERROR_INVALIDDATA');
    expect(e.message).toContain('Invalid data');
  });
});
