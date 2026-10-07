import { createRequire } from 'node:module';
import type { MaybePromise } from '@aviotrix/types';

export interface NativeHost {
  sourceOpen(): MaybePromise<number | null>;
  sourceRead(offset: number, length: number): MaybePromise<Uint8Array>;
  sourceClose(): MaybePromise<void>;
  sinkOpen(): MaybePromise<void>;
  sinkWrite(offset: number, data: Uint8Array): MaybePromise<void>;
  sinkClose(): MaybePromise<void>;
  onLog(level: string, text: string): void;
  onProgress(bytesRead: number, bytesWritten: number, timestamp: number | null): void;
}

export interface NativeRemuxOptions {
  format: string;
  streams?: number[];
  failOnIncompatible: boolean;
  fragmented: boolean;
  sinkSeekable: boolean;
  progressIntervalPackets: number;
}

export interface NativeReader {
  open(): Promise<string>;
  remux(options: NativeRemuxOptions): Promise<string>;
  cancel(): void;
  close(): Promise<void>;
}

export interface NativeModule {
  NativeReader: new (host: NativeHost) => NativeReader;
}

const require = createRequire(import.meta.url);

function loadNative(): NativeModule {
  const candidates = ['../build/Release/aviotrix_node.node', '../build/Debug/aviotrix_node.node'];
  let lastError: Error | null = null;
  for (const candidate of candidates) {
    try {
      return require(candidate) as NativeModule;
    } catch (e) {
      lastError = e instanceof Error ? e : new Error(String(e));
    }
  }
  throw new Error(
    `@aviotrix/node: native addon not found (${lastError?.message ?? 'no candidates'}). Run npm run build:native.`,
  );
}

export const native: NativeModule = loadNative();
