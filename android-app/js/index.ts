import { OsmiumClient } from "./osmium/client";
import { WebSocketTransport } from "./osmium/ws_transport.ts";

const moistureValue = document.getElementById("moisture")!;

const transport = new WebSocketTransport("ws://frederickampsup-desktop:8082");
const osmium = new OsmiumClient(transport);

async function main() {
  await osmium.connect();

  const providers = await osmium.providers();

  const moisture = providers.find(
    provider => provider.label === "Moisture"
  );

  if (!moisture) {
    throw new Error("Moisture provider not found");
  }

  await osmium.subscribe(moisture.id, sample => {
    const view = new DataView(
      sample.value.buffer,
      sample.value.byteOffset,
      sample.value.byteLength,
    );

    const value = view.getUint16(0, true) / 65536;

    moistureValue.textContent = `${(value * 100).toFixed(1)}`;
  });
}

main().catch(console.error);
