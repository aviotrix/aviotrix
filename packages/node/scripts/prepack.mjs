#!/usr/bin/env node
import { cpSync, mkdirSync, rmSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const pkgDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const root = path.resolve(pkgDir, '../..');
const out = path.join(pkgDir, 'native-src');
rmSync(out, { recursive: true, force: true });
mkdirSync(path.join(out, 'scripts'), { recursive: true });
cpSync(path.join(root, 'core'), path.join(out, 'core'), {
  recursive: true,
  filter: (p) => !p.includes('/tests'),
});
cpSync(path.join(root, 'CMakeLists.txt'), path.join(out, 'CMakeLists.txt'));
cpSync(path.join(root, 'scripts/build-ffmpeg.sh'), path.join(out, 'scripts/build-ffmpeg.sh'));
cpSync(
  path.join(root, 'scripts/ffmpeg-components.sh'),
  path.join(out, 'scripts/ffmpeg-components.sh'),
);
console.log(`prepack: wrote ${out}`);
