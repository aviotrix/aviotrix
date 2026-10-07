import type { IoSource } from '@aviotrix/types';

export class BlobSource implements IoSource {
  constructor(private readonly blob: Blob) {}
  open(): number {
    return this.blob.size;
  }
  async read(offset: number, length: number): Promise<Uint8Array> {
    if (offset >= this.blob.size) return new Uint8Array(0);
    return new Uint8Array(
      await this.blob.slice(offset, Math.min(this.blob.size, offset + length)).arrayBuffer(),
    );
  }
  close(): void {}
}
