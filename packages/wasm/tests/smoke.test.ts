import { describe, expect, it } from 'vitest';
import createAviotrixModule from '../dist/aviotrix.mjs';

describe('wasm module', () => {
  it('runs in a JSPI-capable browser and instantiates', async () => {
    expect('Suspending' in WebAssembly).toBe(true);
    const mod = await createAviotrixModule({
      locateFile: (p: string) => new URL(`../dist/${p}`, import.meta.url).href,
    });
    expect(mod.aviotrixHosts).toBeInstanceOf(Map);
    const id = mod._avx_reader_new(42);
    expect(id).toBeGreaterThan(0);
    // open without a registered host fails cleanly with a message, no throw
    const rc = await mod._avx_reader_open(id);
    expect(rc).not.toBe(0);
    expect(mod.UTF8ToString(mod._avx_last_error_code(id))).toBe('IO_FAILED');
    expect(mod.UTF8ToString(mod._avx_last_error_message(id))).toContain('host');
    mod._avx_reader_free(id);
  });
});
