import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import type { IoSink, Metadata } from '@aviotrix/types';
import {
  AviotrixError,
  FileSink,
  FileSource,
  MediaReader,
  MemorySink,
  readMetadata,
  remux,
} from '../src/index.js';
import { AdversarialSource } from './helpers/adversarial_source.js';
import { fixture } from './helpers/fixtures.js';
import { MemorySource } from './helpers/memory_source.js';

/**
 * The matroska muxer writes random SegmentUID/TrackUID values (and a date) into the first ~700
 * bytes, so two remuxes of identical input differ only in the header. Compare the rest.
 */
const MUXER_HEADER_BYTES = 1024;
function sameOutput(a: Uint8Array, b: Uint8Array): boolean {
  return (
    a.length === b.length &&
    Buffer.from(a.subarray(MUXER_HEADER_BYTES)).equals(Buffer.from(b.subarray(MUXER_HEADER_BYTES)))
  );
}

/** Remux output starts at 0 and keeps the source's duration (spec section 10.2). */
function expectSameTimeline(output: Metadata, source: Metadata): void {
  expect(output.duration).not.toBeNull();
  expect(source.duration).not.toBeNull();
  expect(Math.abs((output.duration ?? 0) - (source.duration ?? 0))).toBeLessThan(0.2);
  expect(output.startTime ?? 0).toBeLessThanOrEqual(0.1);
}

let tmp = '';
beforeAll(async () => {
  tmp = await mkdtemp(join(tmpdir(), 'aviotrix-'));
});
afterAll(async () => {
  await rm(tmp, { recursive: true, force: true });
});

describe('remux', () => {
  it('mp4 -> matroska into MemorySink, reopenable', async () => {
    const source = await readMetadata(new FileSource(fixture('h264-aac.mp4')));
    const sink = new MemorySink();
    const result = await remux(new FileSource(fixture('h264-aac.mp4')), sink, {
      format: 'matroska',
    });
    expect(result.streams).toEqual([
      { input: 0, output: 0 },
      { input: 1, output: 1 },
    ]);
    expect(result.packets).toBeGreaterThan(100);
    expect(result.bytesWritten).toBe(sink.bytes().length);
    const m = await readMetadata(new MemorySource(sink.bytes()));
    expect(m.format).toBe('matroska,webm');
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'aac']);
    expectSameTimeline(m, source);
  });

  it('ts -> mp4 into FileSink, reopenable', async () => {
    const source = await readMetadata(new FileSource(fixture('h264-ac3.ts')));
    const out = join(tmp, 'out.mp4');
    await remux(new FileSource(fixture('h264-ac3.ts')), new FileSink(out), { format: 'mp4' });
    const m = await readMetadata(new FileSource(out));
    expect(m.format).toBe('mov,mp4,m4a,3gp,3g2,mj2');
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'ac3']);
    expect(m.streams[0]?.video?.width).toBe(320);
    expectSameTimeline(m, source);
  });

  it('skips the srt stream for mp4 by default and fails on request', async () => {
    const warnings: string[] = [];
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac-srt.mkv')), {
      onLog: (level, text) => {
        if (level === 'warning') warnings.push(text);
      },
    });
    const sink = new MemorySink();
    const result = await reader.remux(sink, { format: 'mp4' });
    expect(result.streams[2]).toEqual({
      input: 2,
      output: null,
      skippedReason: expect.stringContaining('subrip'),
    });
    expect(warnings.some((w) => w.includes('subrip'))).toBe(true);
    await expect(
      reader.remux(new MemorySink(), { format: 'mp4', onIncompatibleStream: 'fail' }),
    ).rejects.toMatchObject({
      code: 'INCOMPATIBLE_STREAM',
    });
    await reader.close();
  });

  it('non-seekable sink needs fragmented for mp4', async () => {
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac.mp4')));
    const streaming = new MemorySink({ seekable: false });
    await expect(reader.remux(streaming, { format: 'mp4' })).rejects.toMatchObject({
      code: 'SINK_NOT_SEEKABLE',
    });
    expect(streaming.bytes().length).toBe(0);
    const result = await reader.remux(streaming, { format: 'mp4', fragmented: true });
    expect(result.packets).toBeGreaterThan(0);
    expect(Buffer.from(streaming.bytes()).includes('moof')).toBe(true);
    await reader.close();
  });

  it('ts (h264 + ac3) -> fragmented mp4 into a non-seekable sink', async () => {
    const streaming = new MemorySink({ seekable: false });
    const result = await remux(new FileSource(fixture('h264-ac3.ts')), streaming, {
      format: 'mp4',
      fragmented: true,
    });
    expect(result.packets).toBeGreaterThan(0);
    expect(Buffer.from(streaming.bytes()).includes('moof')).toBe(true);
    const m = await readMetadata(new MemorySource(streaming.bytes()));
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'ac3']);
  });

  it('aborts via AbortSignal and still closes the sink', async () => {
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac.mp4')));
    const controller = new AbortController();
    const sink = new MemorySink();
    let closed = false;
    const origClose = sink.close.bind(sink);
    sink.close = () => {
      closed = true;
      return origClose();
    };
    await expect(
      reader.remux(sink, {
        format: 'matroska',
        signal: controller.signal,
        onProgress: () => controller.abort(),
      }),
    ).rejects.toMatchObject({ code: 'ABORTED' });
    expect(closed).toBe(true);

    const pre = new AbortController();
    pre.abort();
    await expect(
      reader.remux(new MemorySink(), { format: 'matroska', signal: pre.signal }),
    ).rejects.toMatchObject({ code: 'ABORTED' });
    await reader.close();
  });

  it('queues concurrent calls on one reader', async () => {
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac.mp4')));
    const a = new MemorySink();
    const b = new MemorySink();
    const [ra, rb] = await Promise.all([
      reader.remux(a, { format: 'matroska' }),
      reader.remux(b, { format: 'matroska' }),
    ]);
    expect(ra.packets).toBe(rb.packets);
    expect(a.bytes().length).toBe(b.bytes().length);
    await reader.close();
  });

  it('rejects an out-of-range stream index before any output IO', async () => {
    const sink = new MemorySink();
    let opened = false;
    const origOpen = sink.open.bind(sink);
    sink.open = () => {
      opened = true;
      return origOpen();
    };
    await expect(
      remux(new FileSource(fixture('h264-aac.mp4')), sink, { format: 'matroska', streams: [7] }),
    ).rejects.toMatchObject({
      code: 'INVALID_ARGUMENT',
    });
    expect(opened).toBe(false);
  });

  it('sink write failure propagates and the sink is closed', async () => {
    let closed = false;
    let written = 0;
    const sink: IoSink = {
      seekable: true,
      open() {},
      write(_offset, data) {
        written += data.length;
        if (written > 50000) throw new Error('quota exceeded');
      },
      close() {
        closed = true;
      },
    };
    await expect(
      remux(new FileSource(fixture('h264-aac.mp4')), sink, { format: 'matroska' }),
    ).rejects.toMatchObject({
      code: 'IO_FAILED',
      message: 'quota exceeded',
    });
    expect(closed).toBe(true);
  });

  it('adversarial source (short reads, delays) produces identical output', async () => {
    const reference = new MemorySink();
    await remux(new FileSource(fixture('h264-aac.mp4')), reference, { format: 'matroska' });
    const adversarial = new AdversarialSource(fixture('h264-aac.mp4'), {
      maxChunk: 777,
      maxDelayMs: 2,
    });
    const sink = new MemorySink();
    await remux(adversarial, sink, { format: 'matroska' });
    expect(adversarial.reads).toBeGreaterThan(10);
    expect(sameOutput(sink.bytes(), reference.bytes())).toBe(true);
  });

  it('over-long read is truncated to the requested length', async () => {
    const reference = new MemorySink();
    await remux(new FileSource(fixture('h264-aac.mp4')), reference, { format: 'matroska' });
    const sink = new MemorySink();
    await remux(new AdversarialSource(fixture('h264-aac.mp4'), { overshoot: 100 }), sink, {
      format: 'matroska',
    });
    expect(sameOutput(sink.bytes(), reference.bytes())).toBe(true);
  });

  it('wraps unknown formats as INVALID_ARGUMENT and rejects after close with NOT_OPEN', async () => {
    const reader = await MediaReader.open(new FileSource(fixture('h264-aac.mp4')));
    await expect(reader.remux(new MemorySink(), { format: 'avi' })).rejects.toMatchObject({
      code: 'INVALID_ARGUMENT',
    });
    await reader.close();
    await expect(reader.remux(new MemorySink(), { format: 'matroska' })).rejects.toBeInstanceOf(
      AviotrixError,
    );
  });
});
