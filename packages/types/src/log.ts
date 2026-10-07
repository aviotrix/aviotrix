export type LogLevel =
  'quiet' | 'panic' | 'fatal' | 'error' | 'warning' | 'info' | 'verbose' | 'debug' | 'trace';

export type LogFn = (level: LogLevel, text: string) => void;

export interface OpenOptions {
  onLog?: LogFn;
}
