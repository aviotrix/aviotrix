import type { AviotrixModule, Pointer } from './module.js';

export async function withCStringAsync<T>(
  mod: AviotrixModule,
  text: string,
  fn: (ptr: Pointer) => Promise<T>,
): Promise<T> {
  const size = mod.lengthBytesUTF8(text) + 1;
  const ptr = mod._malloc(size);
  try {
    mod.stringToUTF8(text, ptr, size);
    return await fn(ptr);
  } finally {
    mod._free(ptr);
  }
}

export async function withInt32ArrayAsync<T>(
  mod: AviotrixModule,
  values: number[] | undefined,
  fn: (ptr: Pointer, count: number) => Promise<T>,
): Promise<T> {
  if (!values) return fn(0, -1);
  const ptr = mod._malloc(Math.max(4, values.length * 4));
  try {
    mod.HEAP32.set(values, ptr >> 2);
    return await fn(ptr, values.length);
  } finally {
    mod._free(ptr);
  }
}
