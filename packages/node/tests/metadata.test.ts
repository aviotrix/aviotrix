import { readFile } from 'node:fs/promises';
import { describe, expect, it } from 'vitest';
import { AviotrixError, FileSource, MediaReader, readMetadata } from '../src/index.js';
import { fixture } from './helpers/fixtures.js';
import { MemorySource } from './helpers/memory_source.js';

describe('readMetadata', () => {
  it('reads the mp4 fixture', async () => {
    const m = await readMetadata(new FileSource(fixture('h264-aac.mp4')));
    expect(m.format).toBe('mov,mp4,m4a,3gp,3g2,mj2');
    expect(m.streams).toHaveLength(2);
    expect(m.streams[0]?.codec).toBe('h264');
    expect(m.streams[0]?.video?.width).toBe(320);
    expect(m.streams[0]?.video?.frameRate).toEqual({ num: 30, den: 1 });
    expect(m.streams[1]?.audio?.sampleRate).toBe(48000);
    expect(m.duration).toBeGreaterThan(2.9);
  });

  it('reads the ts fixture with parser-derived dimensions', async () => {
    const m = await readMetadata(new FileSource(fixture('h264-ac3.ts')));
    expect(m.format).toBe('mpegts');
    expect(m.streams[0]?.video?.width).toBe(320);
    expect(m.streams[1]?.codec).toBe('ac3');
  });

  it('rejects non-media input with an AviotrixError', async () => {
    await expect(readMetadata(new FileSource(fixture('not-media.txt')))).rejects.toBeInstanceOf(
      AviotrixError,
    );
    await expect(readMetadata(new FileSource(fixture('not-media.txt')))).rejects.toMatchObject({
      code: 'AVERROR_INVALIDDATA',
    });
    await expect(readMetadata(new MemorySource(new Uint8Array(0)))).rejects.toBeInstanceOf(
      AviotrixError,
    );
  });

  it('delivers libav log lines to onLog', async () => {
    const lines: string[] = [];
    const truncated = new Uint8Array(await readFile(fixture('h264-aac.mp4'))).subarray(0, 3000);
    await readMetadata(new MemorySource(truncated), {
      onLog: (_level, text) => lines.push(text),
    }).catch(() => undefined);
    expect(lines.length).toBeGreaterThan(0);
  });

  it('supports await using', async () => {
    let closed = false;
    const src = new FileSource(fixture('h264-aac.mp4'));
    const origClose = src.close.bind(src);
    src.close = async () => {
      closed = true;
      await origClose();
    };
    {
      await using reader = await MediaReader.open(src);
      expect(reader.metadata.streams).toHaveLength(2);
    }
    expect(closed).toBe(true);
  });
});
