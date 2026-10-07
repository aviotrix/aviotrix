#!/usr/bin/env node
// Runs on `npm install`. Order: skip in the monorepo -> prebuilt download -> source build.
import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const pkgDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const FFMPEG_TAG = 'n8.1.3';

function run(cmd, args, opts = {}) {
  const r = spawnSync(cmd, args, {
    stdio: 'inherit',
    cwd: pkgDir,
    shell: process.platform === 'win32',
    ...opts,
  });
  return r.status === 0;
}

if (process.env.AVIOTRIX_SKIP_NATIVE === '1') process.exit(0);
if (existsSync(path.join(pkgDir, 'build/Release/aviotrix_node.node'))) process.exit(0);

const inMonorepo = existsSync(path.join(pkgDir, '../../core/CMakeLists.txt'));
if (inMonorepo) {
  console.log(
    '@aviotrix/node: monorepo checkout detected; run `npm run build:native -w @aviotrix/node` to build the addon.',
  );
  process.exit(0);
}

if (run('npx', ['--no-install', 'prebuild-install', '-r', 'napi'])) process.exit(0);

console.warn(
  '@aviotrix/node: no prebuilt binary for this platform; building from source. This needs cmake, make, nasm and a C++20 compiler and takes several minutes.',
);
for (const tool of ['cmake', 'make', 'nasm']) {
  if (!run(tool, ['--version'], { stdio: 'ignore' }) && !run(tool, ['-v'], { stdio: 'ignore' })) {
    console.error(`@aviotrix/node: required tool not found: ${tool}`);
    process.exit(1);
  }
}
const nativeSrc = path.join(pkgDir, 'native-src');
const ffmpegDir = path.join(nativeSrc, 'third_party/ffmpeg');
if (!existsSync(path.join(ffmpegDir, 'configure'))) {
  if (
    !run('git', [
      'clone',
      '--depth',
      '1',
      '--branch',
      FFMPEG_TAG,
      'https://github.com/FFmpeg/FFmpeg.git',
      ffmpegDir,
    ])
  ) {
    console.error('@aviotrix/node: could not clone FFmpeg');
    process.exit(1);
  }
}
if (!run('npx', ['--no-install', 'cmake-js', 'compile', `--CDAVIOTRIX_ROOT=${nativeSrc}`])) {
  console.error('@aviotrix/node: source build failed');
  process.exit(1);
}
