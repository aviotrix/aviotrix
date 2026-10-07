import {
  AviotrixError,
  BindingHost,
  OperationQueue,
  parseMetadataJson,
  parseRemuxResultJson,
  type IoSink,
  type IoSource,
  type Metadata,
  type OpenOptions,
  type RemuxOptions,
  type RemuxResult,
} from '@aviotrix/types';
import { loadModule } from './load.js';
import { withCStringAsync, withInt32ArrayAsync } from './memory.js';
import type { AviotrixModule } from './module.js';

// One WASM instance, one thread: all operations across all readers run strictly one at a time, so
// libav log lines and JSPI suspensions never interleave between readers.
const moduleQueue = new OperationQueue();
let nextHostId = 1;

function lastError(mod: AviotrixModule, readerId: number): AviotrixError {
  const code = mod.UTF8ToString(mod._avx_last_error_code(readerId)) || 'UNKNOWN';
  const message = mod.UTF8ToString(mod._avx_last_error_message(readerId)) || 'operation failed';
  return new AviotrixError(code, message);
}

export class MediaReader {
  readonly metadata: Metadata;
  private closed = false;

  private constructor(
    private readonly mod: AviotrixModule,
    private readonly readerId: number,
    private readonly hostId: number,
    private readonly host: BindingHost,
    metadata: Metadata,
  ) {
    this.metadata = metadata;
  }

  static async open(source: IoSource, options: OpenOptions = {}): Promise<MediaReader> {
    const mod = await loadModule();
    return moduleQueue.run(async () => {
      const hostId = nextHostId++;
      const host = new BindingHost(source, options.onLog);
      mod.aviotrixHosts.set(hostId, host);
      let readerId = 0;
      let opened = false;
      try {
        readerId = mod._avx_reader_new(hostId);
        const rc = await mod._avx_reader_open(readerId);
        if (rc !== 0) throw lastError(mod, readerId);
        opened = true;
        const metadata = parseMetadataJson(mod.UTF8ToString(mod._avx_reader_metadata(readerId)));
        return new MediaReader(mod, readerId, hostId, host, metadata);
      } catch (e) {
        if (readerId !== 0) {
          // Never free an open reader: its destructor would call the suspending sourceClose import
          // from a non-promising stack. A reader whose open failed is not open, so free is safe.
          if (opened) await mod._avx_reader_close(readerId).catch(() => undefined);
          mod._avx_reader_free(readerId);
        }
        mod.aviotrixHosts.delete(hostId);
        throw e;
      }
    });
  }

  remux(sink: IoSink, options: RemuxOptions): Promise<RemuxResult> {
    return moduleQueue.run(async () => {
      if (this.closed) throw new AviotrixError('NOT_OPEN', 'reader is closed');
      if (options.signal?.aborted) throw new AviotrixError('ABORTED', 'remux aborted before start');
      this.host.sink = sink;
      this.host.onProgressListener = options.onProgress ?? null;
      const onAbort = (): void => this.mod._avx_reader_cancel(this.readerId);
      options.signal?.addEventListener('abort', onAbort, { once: true });
      try {
        const rc = await withCStringAsync(this.mod, options.format, (formatPtr) =>
          withInt32ArrayAsync(this.mod, options.streams, (streamsPtr, count) =>
            this.mod._avx_reader_remux(
              this.readerId,
              formatPtr,
              streamsPtr,
              count,
              options.onIncompatibleStream === 'fail' ? 1 : 0,
              options.fragmented ? 1 : 0,
              sink.seekable ? 1 : 0,
              100,
            ),
          ),
        );
        if (rc !== 0) throw lastError(this.mod, this.readerId);
        return parseRemuxResultJson(
          this.mod.UTF8ToString(this.mod._avx_reader_result(this.readerId)),
        );
      } finally {
        options.signal?.removeEventListener('abort', onAbort);
        this.host.sink = null;
        this.host.onProgressListener = null;
      }
    });
  }

  close(): Promise<void> {
    return moduleQueue.run(async () => {
      if (this.closed) return;
      this.closed = true;
      // Always close before free: free is not a JSPI export, and an open reader's destructor
      // would call the suspending sourceClose import from a non-promising stack.
      let err: AviotrixError | null = null;
      try {
        const rc = await this.mod._avx_reader_close(this.readerId);
        if (rc !== 0) err = lastError(this.mod, this.readerId);
      } finally {
        this.mod._avx_reader_free(this.readerId);
        this.mod.aviotrixHosts.delete(this.hostId);
      }
      if (err) throw err;
    });
  }

  async [Symbol.asyncDispose](): Promise<void> {
    await this.close();
  }
}

export async function readMetadata(source: IoSource, options?: OpenOptions): Promise<Metadata> {
  const reader = await MediaReader.open(source, options);
  try {
    return reader.metadata;
  } finally {
    await reader.close();
  }
}

export async function remux(
  source: IoSource,
  sink: IoSink,
  options: RemuxOptions,
): Promise<RemuxResult> {
  const reader = await MediaReader.open(source);
  try {
    return await reader.remux(sink, options);
  } finally {
    await reader.close();
  }
}
