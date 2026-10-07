import { readFile } from 'node:fs/promises';
import { describe, expect, it } from 'vitest';
import type { IoSource } from '@aviotrix/types';
import { readMetadata } from '../src/index.js';
import { fixture } from './helpers/fixtures.js';
import { MemorySource } from './helpers/memory_source.js';

// Host callbacks may reject or throw with anything. None of it may crash the process or hang the
// reader: every case must surface as IO_FAILED with a non-empty message.
type Failure = () => Promise<never>;

const failures: ReadonlyArray<[string, Failure]> = [
  ['Symbol', () => Promise.reject(Symbol('x'))],
  ['null-prototype object', () => Promise.reject(Object.create(null))],
  ['null', () => Promise.reject(null)],
  ['string', () => Promise.reject('plain string')],
  [
    'synchronous throw Symbol()',
    () => {
      throw Symbol();
    },
  ],
];

function failingOpen(fail: Failure): IoSource {
  return { open: fail, read: () => new Uint8Array(0), close() {} };
}

function failingRead(size: number, fail: Failure): IoSource {
  return { open: () => size, read: fail, close() {} };
}

describe('host callbacks rejecting with non-Error values', () => {
  for (const [name, fail] of failures) {
    it(`open() rejecting with ${name} -> IO_FAILED`, async () => {
      const err = await readMetadata(failingOpen(fail)).catch((e: Error) => e);
      expect(err).toMatchObject({ code: 'IO_FAILED' });
      expect((err as Error).message.length).toBeGreaterThan(0);
    });

    it(`read() rejecting with ${name} -> IO_FAILED`, async () => {
      const err = await readMetadata(failingRead(1000, fail)).catch((e: Error) => e);
      expect(err).toMatchObject({ code: 'IO_FAILED' });
      expect((err as Error).message.length).toBeGreaterThan(0);
    });
  }

  it('the addon still works afterwards', async () => {
    const bytes = new Uint8Array(await readFile(fixture('h264-aac.mp4')));
    const m = await readMetadata(new MemorySource(bytes));
    expect(m.streams.map((s) => s.codec)).toEqual(['h264', 'aac']);
  });

  it('open() returning NaN or a negative size -> IO_FAILED', async () => {
    for (const size of [Number.NaN, -1, Number.POSITIVE_INFINITY]) {
      await expect(
        readMetadata(failingRead(size, () => Promise.reject(new Error('unused')))),
      ).rejects.toMatchObject({
        code: 'IO_FAILED',
        message: 'IoSource.open must return a non-negative finite number or null',
      });
    }
  });
});
