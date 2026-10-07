import { readFile } from 'node:fs/promises';
import { setTimeout as sleep } from 'node:timers/promises';
import type { IoSource } from '@aviotrix/types';

export interface AdversarialOptions {
  /** Return at most this many bytes per read, regardless of what was asked. */
  maxChunk?: number;
  /** Return this many extra bytes beyond `length` (the core must ignore them). */
  overshoot?: number;
  /** Max random delay per call in ms. */
  maxDelayMs?: number;
}

export class AdversarialSource implements IoSource {
  private bytes: Uint8Array = new Uint8Array(0);
  reads = 0;
  constructor(
    private readonly path: string,
    private readonly opts: AdversarialOptions = {},
  ) {}
  async open(): Promise<number> {
    this.bytes = new Uint8Array(await readFile(this.path));
    return this.bytes.length;
  }
  async read(offset: number, length: number): Promise<Uint8Array> {
    this.reads++;
    await sleep(Math.floor(Math.random() * (this.opts.maxDelayMs ?? 2)));
    const want = Math.min(length, this.opts.maxChunk ?? length) + (this.opts.overshoot ?? 0);
    return this.bytes.subarray(offset, Math.min(this.bytes.length, offset + want));
  }
  async close(): Promise<void> {}
}
