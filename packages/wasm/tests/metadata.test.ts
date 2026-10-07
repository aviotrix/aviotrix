import { describe, expect, it } from 'vitest';
import {
  AviotrixError,
  BlobSource,
  FetchRangeSource,
  MediaReader,
  load,
  readMetadata,
} from '../src/index.js';
import { loadModule } from '../src/load.js';
import { fakeRangeFetch } from './helpers/fake_range_server.js';
import { fixtureBlob, fixtureBytes } from './helpers/fixtures.js';

describe('readMetadata (browser)', () => {
  it('loads once and reads the mp4 fixture via BlobSource', async () => {
    await load();
    const m = await readMetadata(new BlobSource(await fixtureBlob('h264-aac.mp4')));
    expect(m.format).toBe('mov,mp4,m4a,3gp,3g2,mj2');
    expect(m.streams[0]?.video).toEqual({
      width: 320,
      height: 240,
      frameRate: { num: 30, den: 1 },
      // libav leaves codecpar->format unset for mp4 video without decoding; Node reports null too.
      pixelFormat: null,
    });
    expect(m.streams[1]?.audio?.sampleRate).toBe(48000);
  });

  it('reads the ts fixture via FetchRangeSource with range requests', async () => {
    const bytes = await fixtureBytes('h264-ac3.ts');
    const src = new FetchRangeSource('https://example.test/video.ts', {
      fetch: fakeRangeFetch(bytes),
    });
    const m = await readMetadata(src);
    expect(m.format).toBe('mpegts');
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'ac3']);
  });

  it('rejects non-media input with a coded AviotrixError and still closes the source', async () => {
    let closed = false;
    const src = new BlobSource(await fixtureBlob('not-media.txt'));
    const origClose = src.close.bind(src);
    src.close = async () => {
      closed = true;
      await origClose();
    };
    await expect(MediaReader.open(src)).rejects.toMatchObject({ code: 'AVERROR_INVALIDDATA' });
    await expect(MediaReader.open(new BlobSource(new Blob([])))).rejects.toBeInstanceOf(
      AviotrixError,
    );
    expect(closed).toBe(true);
  });

  it('delivers log lines', async () => {
    const lines: string[] = [];
    const head = (await fixtureBytes('h264-aac.mp4')).slice(0, 3000);
    await readMetadata(new BlobSource(new Blob([head])), {
      onLog: (_l, t) => lines.push(t),
    }).catch(() => undefined);
    expect(lines.length).toBeGreaterThan(0);
  });
});

describe('host bookkeeping', () => {
  it('leaves no host entries behind after opening and closing readers', async () => {
    const mod = await loadModule();
    const before = mod.aviotrixHosts.size;
    const blob = await fixtureBlob('h264-aac.mp4');
    for (let i = 0; i < 20; i++) {
      const reader = await MediaReader.open(new BlobSource(blob));
      await reader.close();
    }
    await MediaReader.open(new BlobSource(new Blob([]))).catch(() => undefined);
    expect(mod.aviotrixHosts.size).toBe(before);
  });
});
