import type { BindingHostCallbacks as WasmHost } from '@aviotrix/types';

export type { BindingHostCallbacks as WasmHost } from '@aviotrix/types';

export type Pointer = number;

export interface AviotrixModule {
  HEAPU8: Uint8Array;
  HEAP32: Int32Array;
  aviotrixHosts: Map<number, WasmHost>;
  aviotrixLastHostError: string;
  _malloc(size: number): Pointer;
  _free(ptr: Pointer): void;
  UTF8ToString(ptr: Pointer): string;
  stringToUTF8(str: string, ptr: Pointer, maxBytes: number): void;
  lengthBytesUTF8(str: string): number;
  _avx_reader_new(hostId: number): number;
  _avx_reader_open(readerId: number): Promise<number>;
  _avx_reader_metadata(readerId: number): Pointer;
  _avx_reader_remux(
    readerId: number,
    format: Pointer,
    streams: Pointer,
    streamCount: number,
    failOnIncompatible: number,
    fragmented: number,
    sinkSeekable: number,
    progressInterval: number,
  ): Promise<number>;
  _avx_reader_result(readerId: number): Pointer;
  _avx_reader_cancel(readerId: number): void;
  _avx_reader_close(readerId: number): Promise<number>;
  _avx_reader_free(readerId: number): void;
  _avx_last_error_code(readerId: number): Pointer;
  _avx_last_error_message(readerId: number): Pointer;
}

export interface ModuleOptions {
  locateFile?: (path: string, prefix: string) => string;
}

export type ModuleFactory = (options?: ModuleOptions) => Promise<AviotrixModule>;
