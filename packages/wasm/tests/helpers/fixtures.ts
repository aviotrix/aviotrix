export async function fixtureBytes(name: string): Promise<Uint8Array> {
  // Served raw by the rawFixtures plugin in vitest.config.ts (Vite would transform a .ts fixture).
  const url = `/__fixtures__/${name}`;
  const res = await fetch(url);
  if (!res.ok) throw new Error(`fixture ${name}: HTTP ${res.status}`);
  return new Uint8Array(await res.arrayBuffer());
}

export async function fixtureBlob(name: string): Promise<Blob> {
  return new Blob([await fixtureBytes(name)]);
}
