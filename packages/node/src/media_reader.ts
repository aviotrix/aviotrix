import {
  AviotrixError,
  BindingHost as Host,
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
import { wrapNativeError } from './errors.js';
import { native, type NativeReader, type NativeRemuxOptions } from './native.js';

export class MediaReader {
  readonly metadata: Metadata;
  private readonly queue = new OperationQueue();
  private closed = false;

  private constructor(
    private readonly nativeReader: NativeReader,
    private readonly host: Host,
    metadata: Metadata,
  ) {
    this.metadata = metadata;
  }

  static async open(source: IoSource, options: OpenOptions = {}): Promise<MediaReader> {
    const host = new Host(source, options.onLog);
    const nativeReader = new native.NativeReader(host);
    try {
      const json = await nativeReader.open();
      return new MediaReader(nativeReader, host, parseMetadataJson(json));
    } catch (e) {
      await nativeReader.close().catch(() => undefined);
      throw wrapNativeError(e as object);
    }
  }

  remux(sink: IoSink, options: RemuxOptions): Promise<RemuxResult> {
    return this.queue.run(async () => {
      if (this.closed) throw new AviotrixError('NOT_OPEN', 'reader is closed');
      if (options.signal?.aborted) throw new AviotrixError('ABORTED', 'remux aborted before start');
      const nativeOptions: NativeRemuxOptions = {
        format: options.format,
        failOnIncompatible: options.onIncompatibleStream === 'fail',
        fragmented: options.fragmented ?? false,
        sinkSeekable: sink.seekable,
        progressIntervalPackets: 100,
      };
      if (options.streams) nativeOptions.streams = options.streams;

      this.host.sink = sink;
      this.host.onProgressListener = options.onProgress ?? null;
      const onAbort = (): void => this.nativeReader.cancel();
      options.signal?.addEventListener('abort', onAbort, { once: true });
      try {
        return parseRemuxResultJson(await this.nativeReader.remux(nativeOptions));
      } catch (e) {
        throw wrapNativeError(e as object);
      } finally {
        options.signal?.removeEventListener('abort', onAbort);
        this.host.sink = null;
        this.host.onProgressListener = null;
      }
    });
  }

  close(): Promise<void> {
    return this.queue.run(async () => {
      if (this.closed) return;
      this.closed = true;
      try {
        await this.nativeReader.close();
      } catch (e) {
        throw wrapNativeError(e as object);
      }
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
