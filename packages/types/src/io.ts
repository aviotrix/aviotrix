export type MaybePromise<T> = T | Promise<T>;

/** Positioned read access. The core tracks the stream position; implementations never do. */
export interface IoSource {
  /** Total size in bytes, or null when unknown (the input becomes unseekable). */
  open(): MaybePromise<number | null>;
  /** Up to `length` bytes at `offset`. Short reads are fine; an empty array means EOF. */
  read(offset: number, length: number): MaybePromise<Uint8Array>;
  close(): MaybePromise<void>;
}

/** Positioned write access. */
export interface IoSink {
  readonly seekable: boolean;
  open(): MaybePromise<void>;
  write(offset: number, data: Uint8Array): MaybePromise<void>;
  close(): MaybePromise<void>;
}
