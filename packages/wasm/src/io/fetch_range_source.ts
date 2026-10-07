import type { IoSource } from '@aviotrix/types';

export interface FetchRangeSourceOptions {
  fetch?: typeof fetch;
  headers?: HeadersInit;
}

/** Reads a remote file with HTTP Range requests. The server must honor Range (206 responses). */
export class FetchRangeSource implements IoSource {
  private readonly fetchImpl: typeof fetch;

  constructor(
    private readonly url: string,
    private readonly options: FetchRangeSourceOptions = {},
  ) {
    this.fetchImpl = options.fetch ?? globalThis.fetch.bind(globalThis);
  }

  async open(): Promise<number | null> {
    const res = await this.fetchImpl(this.url, {
      method: 'HEAD',
      headers: new Headers(this.options.headers),
    });
    if (!res.ok) throw new Error(`FetchRangeSource: HEAD ${this.url} -> HTTP ${res.status}`);
    const length = res.headers.get('content-length');
    return length === null ? null : Number(length);
  }

  async read(offset: number, length: number): Promise<Uint8Array> {
    const headers = new Headers(this.options.headers);
    headers.set('Range', `bytes=${offset}-${offset + length - 1}`);
    const res = await this.fetchImpl(this.url, { headers });
    if (res.status === 416) return new Uint8Array(0);
    if (res.status !== 206)
      throw new Error(`FetchRangeSource: server ignored Range (HTTP ${res.status})`);
    return new Uint8Array(await res.arrayBuffer());
  }

  close(): void {}
}
