import { describe, expect, it } from 'vitest';
import { BindingHost } from '../src/index.js';
import type { IoSource } from '../src/index.js';

const source: IoSource = { open: () => 0, read: () => new Uint8Array(0), close() {} };

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
});
