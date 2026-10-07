import { readFile } from 'node:fs/promises';
import { basename, resolve } from 'node:path';
import { playwright } from '@vitest/browser-playwright';
import { defineConfig, type Plugin } from 'vitest/config';

const fixturesDir = resolve(import.meta.dirname, '../../fixtures');

// Vite's transform middleware treats any `.ts` request (including the h264-ac3.ts MPEG-TS fixture)
// as TypeScript source and answers 500. Serve fixtures verbatim under /__fixtures__/ instead.
function rawFixtures(): Plugin {
  return {
    name: 'aviotrix-raw-fixtures',
    configureServer(server) {
      server.middlewares.use('/__fixtures__', (req, res, next) => {
        const name = basename((req.url ?? '').split('?')[0] ?? '');
        readFile(resolve(fixturesDir, name)).then(
          (data) => {
            res.setHeader('Content-Type', 'application/octet-stream');
            res.end(data);
          },
          () => next(),
        );
      });
    },
  };
}

export default defineConfig({
  plugins: [rawFixtures()],
  server: { fs: { allow: ['../..'] } },
  test: {
    include: ['tests/**/*.test.ts'],
    testTimeout: 60_000,
    browser: {
      enabled: true,
      headless: true,
      provider: playwright(),
      instances: [{ browser: 'chromium' }],
    },
  },
});
