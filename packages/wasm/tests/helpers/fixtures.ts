export async function fixtureBytes(name: string): Promise<Uint8Array> {
  // Served raw by the rawFixtures plugin in vitest.config.ts (Vite would transform a .ts fixture).
  const url = `/__fixtures__/${name}`;
  const res = await fetch(url);
  if (!res.ok) throw new Error(`fixture ${name}: HTTP ${res.status}`);
  return new Uint8Array(await res.arrayBuffer());
}

export async function fixtureBlob(name: string): Promise<Blob> {
  return blobOf(await fixtureBytes(name));
}

/**
 * A Blob of `bytes`. BlobPart needs an ArrayBuffer-backed view (not a possibly shared
 * ArrayBufferLike), so copy into a fresh ArrayBuffer.
 */
export function blobOf(bytes: Uint8Array): Blob {
  const copy = new Uint8Array(new ArrayBuffer(bytes.byteLength));
  copy.set(bytes);
  return new Blob([copy]);
}
