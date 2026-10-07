export class AviotrixError extends Error {
  readonly code: string;

  constructor(code: string, message: string) {
    super(message);
    this.name = 'AviotrixError';
    this.code = code;
  }
}

/** Shape both native bindings use to report a failed operation. */
export interface NativeFailure {
  code: string;
  message: string;
}

export function toAviotrixError(failure: NativeFailure): AviotrixError {
  return new AviotrixError(failure.code, failure.message);
}

export function isNativeFailure(value: object): value is NativeFailure {
  return (
    'code' in value &&
    'message' in value &&
    typeof value.code === 'string' &&
    typeof value.message === 'string'
  );
}
