import type { Provider, Sample } from "./types"

export enum PacketType {
  /* No data */
  ProviderRequest = 0x01,

  /* Uint16 N
   * N * [
   *  Uint16 id
   *  string label
   *  string unit
   *  string dtype
   *  Uint32 dsize
   * ] where string = Uint16 N | N * char
   */
  ProviderResponse = 0x02,

  /* Uint16 id */
  Subscribe = 0x03,
  /* Uint16 id */
  Unsubscribe = 0x04,

  /* Uint16 provider,
   * N * byte value
   */
  Data = 0x05
}

export function encodeProviderRequest(): Uint8Array {
  const data = new Uint8Array(1);

  data[0] = PacketType.ProviderRequest;

  return data;
}

const decoder = new TextDecoder();
function readString(data: DataView, offset: number): [string, number] {
  const size = data.getUint16(offset, true);

  const bytes = new Uint8Array(
    data.buffer,
    data.byteOffset + offset + 2,
    size
  );

  const str = decoder.decode(bytes);

  return [str, offset + 2 + size];
}

export function decodeProviderResponse(data: Uint8Array): Provider[] {
  const view = new DataView(
    data.buffer,
    data.byteOffset,
    data.byteLength,
  );

  const num = view.getUint16(1, true);

  let head = 3;
  const providers: Provider[] = [];

  for (let i = 0; i < num; i++) {
    const id = view.getUint16(head, true);
    head += 2;

    const [label, labelHead] = readString(view, head);
    head = labelHead;

    const [unit, unitHead] = readString(view, head);
    head = unitHead;

    const [dtype, dtypeHead] = readString(view, head);
    head = dtypeHead;

    const dsize = view.getUint32(head, true);
    head += 4;

    providers.push({
      id,
      label,
      unit,
      dtype,
      dsize,
    });
  }

  return providers;
}

export function encodeSubscribe(id: number): Uint8Array {
  const data = new Uint8Array(3);
  const view = new DataView(data.buffer);

  data[0] = PacketType.Subscribe;
  view.setUint16(1, id, true);

  return data;
}

export function encodeUnsubscribe(id: number): Uint8Array {
  const data = new Uint8Array(3);
  const view = new DataView(data.buffer);

  data[0] = PacketType.Unsubscribe;
  view.setUint16(1, id, true);

  return data;
}

export function decodeData(data: Uint8Array): Sample {
  const view = new DataView(
    data.buffer,
    data.byteOffset,
    data.byteLength,
  );

  const provider = view.getUint16(1, true);

  return { provider, value: data.subarray(3) };
}
