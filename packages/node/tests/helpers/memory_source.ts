import type { IoSource } from '@aviotrix/types';

export class MemorySource implements IoSource {
  constructor(private readonly bytes: Uint8Array) {}
  open(): number {
    return this.bytes.length;
  }
  read(offset: number, length: number): Uint8Array {
    return this.bytes.subarray(offset, Math.min(this.bytes.length, offset + length));
  }
  close(): void {}
}
