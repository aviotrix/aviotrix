import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { native, type NativeHost, type NativeRemuxOptions } from '../src/native.js';

const fixture = fileURLToPath(new URL('../../../fixtures/h264-aac.mp4', import.meta.url));

const remuxOptions: NativeRemuxOptions = {
  format: 'matroska',
  failOnIncompatible: false,
  fragmented: false,
  sinkSeekable: true,
  progressIntervalPackets: 100,
};

// Synchronous in-memory host: every callback returns a plain value, not a Promise.
async function memoryHost(): Promise<{ host: NativeHost; written: () => Uint8Array }> {
  const file = new Uint8Array(await readFile(fixture));
  let out = new Uint8Array(0);
  const host: NativeHost = {
    sourceOpen: () => file.length,
    sourceRead: (offset, length) => file.subarray(offset, offset + length),
    sourceClose() {},
    sinkOpen() {},
    sinkWrite(offset, data) {
      const end = offset + data.length;
      if (end > out.length) {
        const grown = new Uint8Array(end);
        grown.set(out);
        out = grown;
      }
      out.set(data, offset);
    },
    sinkClose() {},
    onLog() {},
    onProgress() {},
  };
  return { host, written: () => out };
}

describe('native addon: overlapping operations', () => {
  it('runs remux queued behind open without awaiting in between', async () => {
    const { host, written } = await memoryHost();
    const reader = new native.NativeReader(host);
    const [metadata, result] = await Promise.all([reader.open(), reader.remux(remuxOptions)]);
    expect(metadata).toContain('"codec":"h264"');
    expect(result).toContain('"packets":');
    const bytes = written();
    expect(bytes.length).toBeGreaterThan(10000);
    expect(Array.from(bytes.subarray(0, 4))).toEqual([0x1a, 0x45, 0xdf, 0xa3]);
    await reader.close();
  });

  it('rejects open() issued after close() with NOT_OPEN', async () => {
    const { host } = await memoryHost();
    const reader = new native.NativeReader(host);
    const closed = reader.close();
    const opened = reader.open();
    await expect(opened).rejects.toMatchObject({ code: 'NOT_OPEN' });
    await closed;
  });

  it('rejects remux() issued after close() with NOT_OPEN', async () => {
    const { host } = await memoryHost();
    const reader = new native.NativeReader(host);
    await reader.open();
    const closed = reader.close();
    const remuxed = reader.remux(remuxOptions);
    await expect(remuxed).rejects.toMatchObject({ code: 'NOT_OPEN' });
    await closed;
  });

  it('survives host onLog/onProgress callbacks that throw', async () => {
    const { host, written } = await memoryHost();
    host.onProgress = () => {
      throw new Error('progress boom');
    };
    host.onLog = () => {
      throw new Error('log boom');
    };
    const reader = new native.NativeReader(host);
    await reader.open();
    await reader.remux({ ...remuxOptions, progressIntervalPackets: 5 });
    expect(written().length).toBeGreaterThan(10000);
    await reader.close();
  });
});
