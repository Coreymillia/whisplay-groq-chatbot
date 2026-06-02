import fs from "fs";
import path from "path";
import { getEsp32AgentBoardById } from "./boards";

export interface Esp32AgentSerialPortSummary {
  path: string;
  stablePath: string;
  label: string;
  manufacturer: string;
  product: string;
  vendorId: string;
  productId: string;
  serialNumber: string;
  suggestedBoardId: string;
  suggestedBoardLabel: string;
  suggestionReason: string;
}

const DEV_ROOT = "/dev";
const SERIAL_BY_ID_ROOT = "/dev/serial/by-id";
const TTY_USB_PATTERN = /^tty(?:USB|ACM)\d+$/;

function readTextFile(filePath: string): string {
  try {
    return fs.readFileSync(filePath, "utf8").trim();
  } catch {
    return "";
  }
}

function collectUsbAttributes(ttyName: string): {
  manufacturer: string;
  product: string;
  vendorId: string;
  productId: string;
  serialNumber: string;
} {
  const deviceRoot = path.join("/sys/class/tty", ttyName, "device");
  if (!fs.existsSync(deviceRoot)) {
    return {
      manufacturer: "",
      product: "",
      vendorId: "",
      productId: "",
      serialNumber: "",
    };
  }

  let currentPath = fs.realpathSync(deviceRoot);
  for (let depth = 0; depth < 6; depth += 1) {
    const manufacturer = readTextFile(path.join(currentPath, "manufacturer"));
    const product = readTextFile(path.join(currentPath, "product"));
    const vendorId = readTextFile(path.join(currentPath, "idVendor"));
    const productId = readTextFile(path.join(currentPath, "idProduct"));
    const serialNumber = readTextFile(path.join(currentPath, "serial"));
    if (manufacturer || product || vendorId || productId || serialNumber) {
      return {
        manufacturer,
        product,
        vendorId,
        productId,
        serialNumber,
      };
    }
    const parentPath = path.dirname(currentPath);
    if (parentPath === currentPath) {
      break;
    }
    currentPath = parentPath;
  }

  return {
    manufacturer: "",
    product: "",
    vendorId: "",
    productId: "",
    serialNumber: "",
  };
}

function listStableSerialPaths(): Map<string, string> {
  const stablePaths = new Map<string, string>();
  if (!fs.existsSync(SERIAL_BY_ID_ROOT)) {
    return stablePaths;
  }

  const entries = fs.readdirSync(SERIAL_BY_ID_ROOT, { withFileTypes: true });
  for (const entry of entries) {
    const stablePath = path.join(SERIAL_BY_ID_ROOT, entry.name);
    try {
      const resolvedPath = fs.realpathSync(stablePath);
      stablePaths.set(path.basename(resolvedPath), stablePath);
    } catch {
      continue;
    }
  }

  return stablePaths;
}

function suggestBoardForAttributes(input: {
  manufacturer: string;
  product: string;
  vendorId: string;
  productId: string;
  serialNumber: string;
}): {
  boardId: string;
  boardLabel: string;
  reason: string;
} | null {
  const manufacturer = input.manufacturer.toLowerCase();
  const product = input.product.toLowerCase();
  const serialNumber = input.serialNumber.toLowerCase();
  const searchable = `${manufacturer} ${product} ${serialNumber}`.trim();
  const hasText = (needle: string) => searchable.includes(needle);
  const boardLabel = (boardId: string) =>
    getEsp32AgentBoardById(boardId)?.label || boardId;

  if (hasText("core2")) {
    return {
      boardId: "m5stack-core2",
      boardLabel: boardLabel("m5stack-core2"),
      reason: "USB device text mentions Core2.",
    };
  }

  if (hasText("adafruit") && (hasText("feather") || hasText("tft"))) {
    return {
      boardId: "adafruit_feather_esp32s3_tft",
      boardLabel: boardLabel("adafruit_feather_esp32s3_tft"),
      reason: "USB device text looks like an Adafruit Feather TFT board.",
    };
  }

  if (hasText("esp32-cam") || hasText("esp32cam") || hasText("ai thinker")) {
    return {
      boardId: "esp32cam",
      boardLabel: boardLabel("esp32cam"),
      reason: "USB device text looks like an ESP32-CAM style board.",
    };
  }

  if (hasText("esp32-c3") || hasText("esp32c3") || hasText(" c3 ")) {
    return {
      boardId: "esp32c3dev",
      boardLabel: boardLabel("esp32c3dev"),
      reason: "USB device text looks like an ESP32-C3 board.",
    };
  }

  if (hasText("esp32-s3") || hasText("esp32s3") || hasText(" s3 ")) {
    return {
      boardId: "esp32-s3-devkitc-1",
      boardLabel: boardLabel("esp32-s3-devkitc-1"),
      reason: "USB device text looks like an ESP32-S3 board.",
    };
  }

  if (
    input.vendorId.toLowerCase() === "303a" ||
    hasText("silicon labs") ||
    hasText("cp210") ||
    hasText("wch") ||
    hasText("ch340") ||
    hasText("ch9102")
  ) {
    return {
      boardId: "esp32dev",
      boardLabel: boardLabel("esp32dev"),
      reason: "USB bridge looks like a common generic ESP32 dev-board adapter.",
    };
  }

  return null;
}

export function listEsp32AgentSerialPorts(): Esp32AgentSerialPortSummary[] {
  const stablePaths = listStableSerialPaths();
  const ttyEntries = fs
    .readdirSync(DEV_ROOT, { withFileTypes: true })
    .filter((entry) => entry.isCharacterDevice() || entry.isFile())
    .map((entry) => entry.name)
    .filter((name) => TTY_USB_PATTERN.test(name))
    .sort((left, right) => left.localeCompare(right));

  return ttyEntries.map((ttyName) => {
    const devicePath = path.join(DEV_ROOT, ttyName);
    const attrs = collectUsbAttributes(ttyName);
    const suggestion = suggestBoardForAttributes(attrs);
    const labelParts = [
      devicePath,
      attrs.product || attrs.manufacturer || "USB Serial Device",
      attrs.vendorId && attrs.productId
        ? `${attrs.vendorId}:${attrs.productId}`
        : "",
    ].filter(Boolean);

    return {
      path: devicePath,
      stablePath: stablePaths.get(ttyName) || "",
      label: labelParts.join(" - "),
      manufacturer: attrs.manufacturer,
      product: attrs.product,
      vendorId: attrs.vendorId,
      productId: attrs.productId,
      serialNumber: attrs.serialNumber,
      suggestedBoardId: suggestion?.boardId || "",
      suggestedBoardLabel: suggestion?.boardLabel || "",
      suggestionReason: suggestion?.reason || "",
    };
  });
}
