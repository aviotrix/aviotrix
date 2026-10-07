import { open as fsOpen } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { native, type NativeHost } from '../src/native.js';

const fixture = (name: string): string =>
  fileURLToPath(new URL(`../../../fixtures/${name}`, import.meta.url));

async function fileHost(
  path: string,
): Promise<{ host: NativeHost; output: () => Uint8Array; closes: () => number }> {
  const fh = await fsOpen(path, 'r');
  const chunks: Array<{ offset: number; data: Uint8Array }> = [];
  let closes = 0;
  const host: NativeHost = {
    async sourceOpen() {
      return (await fh.stat()).size;
    },
    async sourceRead(offset, length) {
      const buf = new Uint8Array(length);
      const { bytesRead } = await fh.read(buf, 0, length, offset);
      return buf.subarray(0, bytesRead);
    },
    async sourceClose() {
      closes++;
      await fh.close();
    },
    sinkOpen() {},
    sinkWrite(offset, data) {
      chunks.push({ offset, data: new Uint8Array(data) });
    },
    sinkClose() {},
    onLog() {},
    onProgress() {},
  };
  const output = (): Uint8Array => {
    let size = 0;
    for (const c of chunks) size = Math.max(size, c.offset + c.data.length);
    const out = new Uint8Array(size);
    for (const c of chunks) out.set(c.data, c.offset);
    return out;
  };
  return { host, output, closes: () => closes };
}

describe('native addon', () => {
  it('opens a fixture and returns metadata JSON', async () => {
    const { host, closes } = await fileHost(fixture('h264-aac.mp4'));
    const reader = new native.NativeReader(host);
    const json = await reader.open();
    expect(json).toContain('"codec":"h264"');
    expect(json).toContain('"width":320');
    await reader.close();
    expect(closes()).toBe(1);
  });

  it('remuxes to matroska through the host sink', async () => {
    const { host, output } = await fileHost(fixture('h264-aac.mp4'));
    const reader = new native.NativeReader(host);
    await reader.open();
    const json = await reader.remux({
      format: 'matroska',
      failOnIncompatible: false,
      fragmented: false,
      sinkSeekable: true,
      progressIntervalPackets: 100,
    });
    expect(json).toContain('"packets":');
    const bytes = output();
    expect(bytes.length).toBeGreaterThan(10000);
    // EBML magic
    expect(Array.from(bytes.subarray(0, 4))).toEqual([0x1a, 0x45, 0xdf, 0xa3]);
    await reader.close();
  });

  it('rejects with a coded error on non-media input', async () => {
    const { host } = await fileHost(fixture('not-media.txt'));
    const reader = new native.NativeReader(host);
    await expect(reader.open()).rejects.toMatchObject({
      code: 'AVERROR_INVALIDDATA',
      message: expect.stringContaining('avformat_open_input'),
    });
    await reader.close();
  });

  it('propagates a host read rejection as IO_FAILED with the original message', async () => {
    const host: NativeHost = {
      sourceOpen: () => 1000,
      sourceRead: () => Promise.reject(new Error('network down')),
      sourceClose() {},
      sinkOpen() {},
      sinkWrite() {},
      sinkClose() {},
      onLog() {},
      onProgress() {},
    };
    const reader = new native.NativeReader(host);
    await expect(reader.open()).rejects.toMatchObject({
      code: 'IO_FAILED',
      message: 'network down',
    });
    await reader.close();
  });

  it('cancel() aborts a running remux', async () => {
    const { host } = await fileHost(fixture('h264-aac.mp4'));
    const reader = new native.NativeReader(host);
    host.onProgress = () => reader.cancel();
    await reader.open();
    await expect(
      reader.remux({
        format: 'matroska',
        failOnIncompatible: false,
        fragmented: false,
        sinkSeekable: true,
        progressIntervalPackets: 5,
      }),
    ).rejects.toMatchObject({ code: 'ABORTED' });
    await reader.close();
  });
});
