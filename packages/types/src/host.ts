import type { IoSink, IoSource, MaybePromise } from './io.js';
import type { LogFn, LogLevel } from './log.js';
import type { RemuxProgress } from './remux.js';

export interface BindingHostCallbacks {
  sourceOpen(): MaybePromise<number | null>;
  sourceRead(offset: number, length: number): MaybePromise<Uint8Array>;
  sourceClose(): MaybePromise<void>;
  sinkOpen(): MaybePromise<void>;
  sinkWrite(offset: number, data: Uint8Array): MaybePromise<void>;
  sinkClose(): MaybePromise<void>;
  onLog(level: string, text: string): void;
  onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void;
}

const logLevels: ReadonlySet<string> = new Set([
  'quiet',
  'panic',
  'fatal',
  'error',
  'warning',
  'info',
  'verbose',
  'debug',
  'trace',
]);

/** Routes a binding's host callbacks to the current IoSource, IoSink, and listeners. */
export class BindingHost implements BindingHostCallbacks {
  sink: IoSink | null = null;
  onProgressListener: ((progress: RemuxProgress) => void) | null = null;

  constructor(
    private readonly source: IoSource,
    private readonly onLogListener: LogFn | undefined,
  ) {}

  sourceOpen(): ReturnType<IoSource['open']> {
    return this.source.open();
  }
  sourceRead(offset: number, length: number): ReturnType<IoSource['read']> {
    return this.source.read(offset, length);
  }
  sourceClose(): ReturnType<IoSource['close']> {
    return this.source.close();
  }
  sinkOpen(): ReturnType<IoSink['open']> {
    return this.requireSink().open();
  }
  sinkWrite(offset: number, data: Uint8Array): ReturnType<IoSink['write']> {
    return this.requireSink().write(offset, data);
  }
  sinkClose(): ReturnType<IoSink['close']> {
    return this.requireSink().close();
  }
  onLog(level: string, text: string): void {
    if (!this.onLogListener) return;
    const lvl: LogLevel = logLevels.has(level) ? (level as LogLevel) : 'info';
    try {
      this.onLogListener(lvl, text);
    } catch {
      // A throwing user callback must never cross into native/wasm code.
    }
  }
  onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void {
    try {
      this.onProgressListener?.({ bytesRead, bytesWritten, timestamp });
    } catch {
      // A throwing user callback must never cross into native/wasm code.
    }
  }

  private requireSink(): IoSink {
    if (!this.sink) throw new Error('aviotrix: sink callback with no active sink');
    return this.sink;
  }
}
