import type { Transport } from "./transport";
import type { Provider, Sample } from "./types";
import * as protocol from "./protocol";

export class OsmiumClient {
  private readonly dataCallbacks = new Map<number, ((sample: Sample) => void)[]>();
  private providerResponse?: (providers: Provider[]) => void;
  constructor(private readonly transport: Transport) {
    this.transport.onData(data => this.onData(data));
  }

  onData(data: Uint8Array): void {
    console.log(data);
    switch (data[0]) {
      case protocol.PacketType.ProviderResponse:
        if (this.providerResponse) {
          this.providerResponse(protocol.decodeProviderResponse(data));
          this.providerResponse = undefined;
        }
        break;
      case protocol.PacketType.Data:
        const sample = protocol.decodeData(data);

        const callbacks = this.dataCallbacks.get(sample.provider) ?? [];
        for (const callback of callbacks)
          callback(sample);
        break;
      default:
        console.error(`Unknown packet type ${data[0]}!`);
        break;
    }
  }

  async connect(): Promise<void> {
    await this.transport.connect();
  }

  async providers(): Promise<Provider[]> {
    const promise = new Promise<Provider[]>(resolve => {
      this.providerResponse = resolve;
    });

    await this.transport.send(protocol.encodeProviderRequest());

    return await promise;
  }

  async subscribe(provider: number, callback: (sample: Sample) => void): Promise<void> {
    await this.transport.send(protocol.encodeSubscribe(provider));

    const callbacks = this.dataCallbacks.get(provider) ?? [];
    callbacks.push(callback);
    this.dataCallbacks.set(provider, callbacks);
  }

  async unsubscribe(provider: number): Promise<void> {
    await this.transport.send(protocol.encodeUnsubscribe(provider));
  }
}
