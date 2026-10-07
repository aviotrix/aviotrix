import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { FileSink, FileSource, MemorySink } from '../src/index.js';
import { fixture } from './helpers/fixtures.js';

let tmp = '';
beforeAll(async () => {
  tmp = await mkdtemp(join(tmpdir(), 'aviotrix-io-'));
});
afterAll(async () => {
  await rm(tmp, { recursive: true, force: true });
});

describe('FileSource', () => {
  it('reports size and reads ranges', async () => {
    const src = new FileSource(fixture('subs.srt'));
    const size = await src.open();
    expect(size).toBeGreaterThan(10);
    const head = await src.read(0, 1);
    expect(new TextDecoder().decode(head)).toBe('1');
    const past = await src.read(size ?? 0, 10);
    expect(past.length).toBe(0);
    await src.close();
  });
});

describe('FileSink', () => {
  it('writes at offsets, including backwards', async () => {
    const path = join(tmp, 'sink.bin');
    const sink = new FileSink(path);
    expect(sink.seekable).toBe(true);
    await sink.open();
    await sink.write(0, new Uint8Array([1, 2, 3, 4]));
    await sink.write(1, new Uint8Array([9, 9]));
    await sink.close();
    expect([...(await readFile(path))]).toEqual([1, 9, 9, 4]);
  });
});

describe('MemorySink', () => {
  it('seekable mode grows and overwrites', async () => {
    const sink = new MemorySink();
    await sink.open();
    await sink.write(2, new Uint8Array([5]));
    await sink.write(0, new Uint8Array([1, 2]));
    await sink.close();
    expect([...sink.bytes()]).toEqual([1, 2, 5]);
  });
  it('streaming mode rejects non-sequential writes', async () => {
    const sink = new MemorySink({ seekable: false });
    await sink.open();
    await sink.write(0, new Uint8Array([1]));
    await expect(async () => sink.write(5, new Uint8Array([2]))).rejects.toThrow(/sequential/);
  });
});
