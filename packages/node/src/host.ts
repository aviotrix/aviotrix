import type { IoSink, IoSource, LogFn, LogLevel, RemuxProgress } from '@aviotrix/types';
import type { NativeHost } from './native.js';

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

/** Routes the native binding's host callbacks to the current IoSource, IoSink, and listeners. */
export class Host implements NativeHost {
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
    this.onLogListener(lvl, text);
  }
  onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void {
    this.onProgressListener?.({ bytesRead, bytesWritten, timestamp });
  }

  private requireSink(): IoSink {
    if (!this.sink) throw new Error('aviotrix: sink callback with no active sink');
    return this.sink;
  }
}
