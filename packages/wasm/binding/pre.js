Module['aviotrixHosts'] = new Map();
Module['aviotrixLastHostError'] = '';
// Stringifies whatever a host callback rejected or threw with, and never throws itself: it runs
// inside catch blocks of EM_ASYNC_JS bodies, where a throw would unwind through wasm frames.
// Symbols and null-prototype objects make a plain String(e) or e.message access throw.
Module['aviotrixDescribeError'] = (e) => {
  try {
    if (e !== null && typeof e === 'object' && typeof e.message === 'string' && e.message) {
      return e.message;
    }
    const s = String(e);
    return s || 'host callback failed';
  } catch {
    return 'unprintable rejection';
  }
};
