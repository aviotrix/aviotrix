import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';

const addon = fileURLToPath(new URL('../build/Release/aviotrix_node.node', import.meta.url));
const fixture = fileURLToPath(new URL('../../../fixtures/h264-aac.mp4', import.meta.url));

// Runs in a child `node` process: the process must exit 0 on its own, without closing every reader.
const script = `
import { createRequire } from 'node:module';
import { readFile } from 'node:fs/promises';
const [addonPath, fixturePath, mode] = process.argv.slice(1);
const native = createRequire(import.meta.url)(addonPath);
const file = new Uint8Array(await readFile(fixturePath));
let written = 0;
const host = {
  sourceOpen: () => file.length,
  sourceRead: (offset, length) => file.subarray(offset, offset + length),
  sourceClose() {},
  sinkOpen() {},
  sinkWrite(offset, data) { written = Math.max(written, offset + data.length); },
  sinkClose() {},
  onLog() {},
  onProgress() {},
};
const reader = new native.NativeReader(host);
if (mode === 'opened') {
  await reader.open();
} else if (mode === 'open-and-remux') {
  const opts = { format: 'matroska', failOnIncompatible: false, fragmented: false, sinkSeekable: true, progressIntervalPackets: 100 };
  await Promise.all([reader.open(), reader.remux(opts)]);
}
console.log('done ' + mode + ' ' + written);
`;

interface ChildResult {
  code: number | null;
  signal: NodeJS.Signals | null;
  stdout: string;
  stderr: string;
}

function runChild(mode: string): Promise<ChildResult> {
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, [
      '--input-type=module',
      '-e',
      script,
      addon,
      fixture,
      mode,
    ]);
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (d: Buffer) => (stdout += d.toString()));
    child.stderr.on('data', (d: Buffer) => (stderr += d.toString()));
    child.on('error', reject);
    child.on('close', (code, signal) => resolve({ code, signal, stdout, stderr }));
  });
}

describe('process teardown with unclosed readers', () => {
  it('exits cleanly when a reader was never opened', async () => {
    const r = await runChild('idle');
    expect(r).toMatchObject({ code: 0, signal: null, stderr: '' });
    expect(r.stdout).toContain('done idle');
  });

  it('exits cleanly when an opened reader is never closed', async () => {
    const r = await runChild('opened');
    expect(r).toMatchObject({ code: 0, signal: null, stderr: '' });
    expect(r.stdout).toContain('done opened');
  });

  it('stays alive until overlapping open + remux both settle, then exits cleanly', async () => {
    const r = await runChild('open-and-remux');
    expect(r).toMatchObject({ code: 0, signal: null, stderr: '' });
    const m = /done open-and-remux (\d+)/.exec(r.stdout);
    expect(m).not.toBeNull();
    expect(Number(m?.[1])).toBeGreaterThan(10000);
  });
});
