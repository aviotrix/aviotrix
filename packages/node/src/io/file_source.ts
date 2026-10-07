import { open, type FileHandle } from 'node:fs/promises';
import type { IoSource } from '@aviotrix/types';

export class FileSource implements IoSource {
  private handle: FileHandle | null = null;

  constructor(private readonly path: string) {}

  async open(): Promise<number> {
    this.handle = await open(this.path, 'r');
    return (await this.handle.stat()).size;
  }

  async read(offset: number, length: number): Promise<Uint8Array> {
    if (!this.handle) throw new Error('FileSource.read before open');
    const buffer = new Uint8Array(length);
    const { bytesRead } = await this.handle.read(buffer, 0, length, offset);
    return buffer.subarray(0, bytesRead);
  }

  async close(): Promise<void> {
    const handle = this.handle;
    this.handle = null;
    await handle?.close();
  }
}
