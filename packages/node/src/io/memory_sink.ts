import type { IoSink } from '@aviotrix/types';

export class MemorySink implements IoSink {
  readonly seekable: boolean;
  private buffer = new Uint8Array(64 * 1024);
  private length = 0;

  constructor(options: { seekable?: boolean } = {}) {
    this.seekable = options.seekable ?? true;
  }

  open(): void {
    this.length = 0;
  }

  write(offset: number, data: Uint8Array): void {
    if (!this.seekable && offset !== this.length) {
      throw new Error(
        `MemorySink: streaming sink requires sequential writes (got ${offset}, expected ${this.length})`,
      );
    }
    const end = offset + data.length;
    if (end > this.buffer.length) {
      const grown = new Uint8Array(Math.max(end, this.buffer.length * 2));
      grown.set(this.buffer.subarray(0, this.length));
      this.buffer = grown;
    }
    this.buffer.set(data, offset);
    this.length = Math.max(this.length, end);
  }

  close(): void {}

  bytes(): Uint8Array {
    return this.buffer.subarray(0, this.length);
  }
}
