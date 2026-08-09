export interface Transport {
  connect(): Promise<void>;
  send(data: Uint8Array): Promise<void>;
  close(): void;

  onData(callback: (data: Uint8Array) => void): void;
}
