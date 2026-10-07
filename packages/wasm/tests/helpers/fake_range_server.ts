export function fakeRangeFetch(
  bytes: Uint8Array,
  options: { ignoreRange?: boolean; noLength?: boolean } = {},
): typeof fetch {
  return async (_input, init) => {
    const headers = new Headers(init?.headers);
    const range = headers.get('range');
    if (init?.method === 'HEAD') {
      const h = new Headers({ 'accept-ranges': 'bytes' });
      if (!options.noLength) h.set('content-length', String(bytes.length));
      return new Response(null, { status: 200, headers: h });
    }
    if (range && !options.ignoreRange) {
      const m = /^bytes=(\d+)-(\d+)$/.exec(range);
      if (!m) return new Response('bad range', { status: 416 });
      const start = Number(m[1]);
      const end = Math.min(Number(m[2]), bytes.length - 1);
      return new Response(bytes.slice(start, end + 1), {
        status: 206,
        headers: { 'content-range': `bytes ${start}-${end}/${bytes.length}` },
      });
    }
    return new Response(bytes, {
      status: 200,
      headers: { 'content-length': String(bytes.length) },
    });
  };
}
