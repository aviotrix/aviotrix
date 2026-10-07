import { describe, expect, it } from 'vitest';
import type { IoSink, IoSource } from '@aviotrix/types';
import { BlobSource, MediaReader, MemorySink, readMetadata, remux } from '../src/index.js';
import { fixtureBlob, fixtureBytes } from './helpers/fixtures.js';

const reopen = (bytes: Uint8Array) => readMetadata(new BlobSource(new Blob([bytes])));

// The Matroska muxer writes random SegmentUID/TrackUIDs/date in the first ~1 KB, so compare the
// length plus everything from offset 1024 onward.
function sameOutput(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) return false;
  for (let i = 1024; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

describe('remux (browser)', () => {
  it('mp4 -> matroska, reopenable', async () => {
    const sink = new MemorySink();
    const result = await remux(new BlobSource(await fixtureBlob('h264-aac.mp4')), sink, {
      format: 'matroska',
    });
    expect(result.streams).toEqual([
      { input: 0, output: 0 },
      { input: 1, output: 1 },
    ]);
    const m = await reopen(sink.bytes());
    expect(m.format).toBe('matroska,webm');
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'aac']);
    expect(sink.toBlob('video/x-matroska').size).toBe(result.bytesWritten);
  });

  it('ts -> mp4, reopenable', async () => {
    const sink = new MemorySink();
    await remux(new BlobSource(await fixtureBlob('h264-ac3.ts')), sink, { format: 'mp4' });
    const m = await reopen(sink.bytes());
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'ac3']);
    expect(m.streams[0]?.video?.width).toBe(320);
  });

  it('skips srt for mp4 by default, fails on request', async () => {
    const reader = await MediaReader.open(new BlobSource(await fixtureBlob('h264-aac-srt.mkv')));
    const result = await reader.remux(new MemorySink(), { format: 'mp4' });
    expect(result.streams[2]).toMatchObject({ input: 2, output: null });
    await expect(
      reader.remux(new MemorySink(), { format: 'mp4', onIncompatibleStream: 'fail' }),
    ).rejects.toMatchObject({
      code: 'INCOMPATIBLE_STREAM',
    });
    await reader.close();
  });

  it('streaming sink needs fragmented for mp4', async () => {
    const reader = await MediaReader.open(new BlobSource(await fixtureBlob('h264-aac.mp4')));
    const streaming = new MemorySink({ seekable: false });
    await expect(reader.remux(streaming, { format: 'mp4' })).rejects.toMatchObject({
      code: 'SINK_NOT_SEEKABLE',
    });
    const result = await reader.remux(streaming, { format: 'mp4', fragmented: true });
    expect(result.packets).toBeGreaterThan(0);
    expect(new TextDecoder('latin1').decode(streaming.bytes()).includes('moof')).toBe(true);
    await reader.close();
  });

  it('aborts via AbortSignal and closes the sink', async () => {
    const reader = await MediaReader.open(new BlobSource(await fixtureBlob('h264-aac.mp4')));
    const controller = new AbortController();
    let closed = false;
    const sink = new MemorySink();
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
    await reader.close();
  });

  it('serializes operations module-wide across readers', async () => {
    const [a, b] = await Promise.all([
      MediaReader.open(new BlobSource(await fixtureBlob('h264-aac.mp4'))),
      MediaReader.open(new BlobSource(await fixtureBlob('vp9-opus.webm'))),
    ]);
    const sa = new MemorySink();
    const sb = new MemorySink();
    const [ra, rb] = await Promise.all([
      a.remux(sa, { format: 'matroska' }),
      b.remux(sb, { format: 'webm' }),
    ]);
    expect(ra.packets).toBeGreaterThan(0);
    expect(rb.packets).toBeGreaterThan(0);
    expect((await reopen(sa.bytes())).streams[0]?.codec).toBe('h264');
    expect((await reopen(sb.bytes())).streams[0]?.codec).toBe('vp9');
    await Promise.all([a.close(), b.close()]);
  });

  it('out-of-range stream index rejects before output IO', async () => {
    let opened = false;
    const sink = new MemorySink();
    const origOpen = sink.open.bind(sink);
    sink.open = () => {
      opened = true;
      return origOpen();
    };
    await expect(
      remux(new BlobSource(await fixtureBlob('h264-aac.mp4')), sink, {
        format: 'matroska',
        streams: [9],
      }),
    ).rejects.toMatchObject({
      code: 'INVALID_ARGUMENT',
    });
    expect(opened).toBe(false);
  });

  it('sink write failure propagates with the original message and closes the sink', async () => {
    let written = 0;
    let closed = false;
    const sink: IoSink = {
      seekable: true,
      open() {},
      write(_o, data) {
        written += data.length;
        if (written > 50000) throw new Error('quota exceeded');
      },
      close() {
        closed = true;
      },
    };
    await expect(
      remux(new BlobSource(await fixtureBlob('h264-aac.mp4')), sink, { format: 'matroska' }),
    ).rejects.toMatchObject({
      code: 'IO_FAILED',
      message: 'quota exceeded',
    });
    expect(closed).toBe(true);
  });

  it('short and over-long reads produce the same output as a plain source', async () => {
    const bytes = await fixtureBytes('h264-aac.mp4');
    const reference = new MemorySink();
    await remux(new BlobSource(new Blob([bytes])), reference, { format: 'matroska' });
    const weird: IoSource = {
      open: () => bytes.length,
      read: (offset, length) => {
        const want = offset % 2 === 0 ? Math.min(length, 999) : length + 100;
        return bytes.subarray(offset, Math.min(bytes.length, offset + want));
      },
      close() {},
    };
    const sink = new MemorySink();
    await remux(weird, sink, { format: 'matroska' });
    expect(sameOutput(sink.bytes(), reference.bytes())).toBe(true);
  });
});
