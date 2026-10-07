import { describe, expect, it } from 'vitest';
import { BindingHost } from '../src/index.js';
import type { IoSource } from '../src/index.js';

const source: IoSource = { open: () => 0, read: () => new Uint8Array(0), close() {} };

const hostFor = (size: number | null): BindingHost =>
  new BindingHost({ ...source, open: () => size }, undefined);

describe('BindingHost', () => {
  it('swallows exceptions thrown by the user onLog callback', () => {
    const host = new BindingHost(source, () => {
      throw new Error('boom');
    });
    expect(() => host.onLog('info', 'text')).not.toThrow();
  });

  it('swallows exceptions thrown by the progress listener', () => {
    const host = new BindingHost(source, undefined);
    host.onProgressListener = () => {
      throw new Error('boom');
    };
    expect(() => host.onProgress(1, 2, null)).not.toThrow();
  });

  it('maps unknown log levels to info', () => {
    const seen: string[] = [];
    const host = new BindingHost(source, (level) => seen.push(level));
    host.onLog('bogus', 'x');
    expect(seen).toEqual(['info']);
  });

  describe('sourceOpen validates the size', () => {
    it('passes null through and floors finite non-negative numbers', async () => {
      await expect(hostFor(null).sourceOpen()).resolves.toBeNull();
      await expect(hostFor(0).sourceOpen()).resolves.toBe(0);
      await expect(hostFor(1234.9).sourceOpen()).resolves.toBe(1234);
    });

    it('awaits async sources', async () => {
      const host = new BindingHost({ ...source, open: () => Promise.resolve(42) }, undefined);
      await expect(host.sourceOpen()).resolves.toBe(42);
    });

    it('rejects NaN, infinities, negatives and non-numbers', async () => {
      const bad: Array<number | null> = [Number.NaN, Number.POSITIVE_INFINITY, -1];
      // An untyped JS source can return anything; JSON.parse gives such values a typed path in here.
      bad.push(JSON.parse('"12"') as number, JSON.parse('{}') as number);
      for (const size of bad) {
        await expect(hostFor(size).sourceOpen()).rejects.toThrow(
          'IoSource.open must return a non-negative finite number or null',
        );
      }
    });
  });
});
