declare module '*/aviotrix.mjs' {
  import type { ModuleFactory } from './module.js';
  const createAviotrixModule: ModuleFactory;
  export default createAviotrixModule;
}
