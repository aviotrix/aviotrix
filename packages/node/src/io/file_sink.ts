import { open, type FileHandle } from 'node:fs/promises';
import type { IoSink } from '@aviotrix/types';

export class FileSink implements IoSink {
  readonly seekable = true;
  private handle: FileHandle | null = null;

  constructor(private readonly path: string) {}

  async open(): Promise<void> {
    this.handle = await open(this.path, 'w');
  }

  async write(offset: number, data: Uint8Array): Promise<void> {
    if (!this.handle) throw new Error('FileSink.write before open');
    let written = 0;
    while (written < data.length) {
      const { bytesWritten } = await this.handle.write(
        data,
        written,
        data.length - written,
        offset + written,
      );
      written += bytesWritten;
    }
  }

  async close(): Promise<void> {
    const handle = this.handle;
    this.handle = null;
    await handle?.close();
  }
}
