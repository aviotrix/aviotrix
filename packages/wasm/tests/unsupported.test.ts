import { afterEach, describe, expect, it } from 'vitest';
import { _resetForTests, load } from '../src/index.js';

type WasmNamespace = { Suspending?: object };

describe('load without JSPI', () => {
  const ns = WebAssembly as WasmNamespace;
  const saved = ns.Suspending;
  afterEach(() => {
    Object.defineProperty(WebAssembly, 'Suspending', {
      value: saved,
      configurable: true,
      writable: true,
    });
    _resetForTests();
  });

  it('rejects with UNSUPPORTED_RUNTIME', async () => {
    _resetForTests();
    Object.defineProperty(WebAssembly, 'Suspending', {
      value: undefined,
      configurable: true,
      writable: true,
    });
    await expect(load()).rejects.toMatchObject({ code: 'UNSUPPORTED_RUNTIME' });
  });
});
