import type { Transport } from "./transport"

export class WebSocketTransport implements Transport {
  private socket: WebSocket;

  constructor(url: string) {
    this.socket = new WebSocket(url);
    this.socket.binaryType = "arraybuffer";
  }

  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.socket.onopen = () => resolve();
      this.socket.onerror = () => reject(new Error("WebSocket connection failed"));
    });
  }

  send(data: Uint8Array): void {
    this.socket.send(data);
  }

  close(): void {
    this.socket.close();
  }

  onData(callback: (data: Uint8Array) => void): void {
    this.socket.onmessage = event => {
      callback(new Uint8Array(event.data));
    };
  }
}
