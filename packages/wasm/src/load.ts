import { AviotrixError } from '@aviotrix/types';
import type { AviotrixModule, ModuleFactory } from './module.js';

export interface LoadOptions {
  /** Override where the .wasm is fetched from. Default: next to the module script. */
  wasmUrl?: string;
}

let modulePromise: Promise<AviotrixModule> | null = null;

function hasJspi(): boolean {
  const ns = WebAssembly as { Suspending?: object };
  return typeof ns.Suspending === 'function';
}

export function loadModule(options: LoadOptions = {}): Promise<AviotrixModule> {
  if (!modulePromise) {
    modulePromise = (async () => {
      if (!hasJspi()) {
        throw new AviotrixError(
          'UNSUPPORTED_RUNTIME',
          '@aviotrix/wasm needs WebAssembly JavaScript Promise Integration (WebAssembly.Suspending). Chrome 137+, Safari 27+, Firefox 153+.',
        );
      }
      // Emscripten-generated glue without a declaration file.
      // @ts-expect-error TS7016: no types for generated module
      const glue: { default: ModuleFactory } = await import('../dist/aviotrix.mjs');
      const createAviotrixModule = glue.default;
      const moduleOptions = options.wasmUrl
        ? {
            locateFile: (path: string) =>
              path.endsWith('.wasm') ? (options.wasmUrl ?? path) : path,
          }
        : {};
      return createAviotrixModule(moduleOptions);
    })();
    modulePromise.catch(() => {
      modulePromise = null; // allow a retry after a failed load
    });
  }
  return modulePromise;
}

/** Fetches and instantiates the WASM once. Optional: MediaReader.open calls it implicitly. */
export async function load(options: LoadOptions = {}): Promise<void> {
  await loadModule(options);
}

/** @internal */
export function _resetForTests(): void {
  modulePromise = null;
}
