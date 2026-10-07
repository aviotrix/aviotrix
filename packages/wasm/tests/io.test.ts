import { describe, expect, it } from 'vitest';
import { BlobSource, FetchRangeSource, MemorySink } from '../src/index.js';
import { fakeRangeFetch } from './helpers/fake_range_server.js';

describe('BlobSource', () => {
  it('reads ranges and reports size', async () => {
    const src = new BlobSource(new Blob([new Uint8Array([1, 2, 3, 4, 5])]));
    expect(await src.open()).toBe(5);
    expect([...(await src.read(3, 10))]).toEqual([4, 5]);
    expect((await src.read(5, 1)).length).toBe(0);
    await src.close();
  });
});

describe('FetchRangeSource', () => {
  const bytes = new Uint8Array(1000).map((_, i) => i % 251);
  it('uses HEAD for size and Range for reads', async () => {
    const src = new FetchRangeSource('https://example.test/f', { fetch: fakeRangeFetch(bytes) });
    expect(await src.open()).toBe(1000);
    const chunk = await src.read(500, 10);
    expect([...chunk]).toEqual([...bytes.subarray(500, 510)]);
  });
  it('returns null size when the server omits content-length', async () => {
    const src = new FetchRangeSource('https://example.test/f', {
      fetch: fakeRangeFetch(bytes, { noLength: true }),
    });
    expect(await src.open()).toBeNull();
  });
  it('throws when the server ignores Range', async () => {
    const src = new FetchRangeSource('https://example.test/f', {
      fetch: fakeRangeFetch(bytes, { ignoreRange: true }),
    });
    await src.open();
    await expect(src.read(10, 5)).rejects.toThrow(/Range/);
  });
});

describe('MemorySink', () => {
  it('grows, overwrites, and exports a Blob', async () => {
    const sink = new MemorySink();
    sink.open();
    sink.write(2, new Uint8Array([5]));
    sink.write(0, new Uint8Array([1, 2]));
    sink.close();
    expect([...sink.bytes()]).toEqual([1, 2, 5]);
    expect(sink.toBlob('application/octet-stream').size).toBe(3);
  });
  it('streaming mode rejects non-sequential writes', () => {
    const sink = new MemorySink({ seekable: false });
    sink.open();
    sink.write(0, new Uint8Array([1]));
    expect(() => sink.write(5, new Uint8Array([2]))).toThrow(/sequential/);
  });
});
