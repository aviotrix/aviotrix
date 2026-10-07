import { AviotrixError, isNativeFailure } from '@aviotrix/types';

export function wrapNativeError(
  e: object | string | number | boolean | null | undefined,
): AviotrixError {
  if (e instanceof AviotrixError) return e;
  if (e !== null && typeof e === 'object' && isNativeFailure(e))
    return new AviotrixError(e.code, e.message);
  if (e instanceof Error) return new AviotrixError('UNKNOWN', e.message);
  return new AviotrixError('UNKNOWN', String(e));
}
