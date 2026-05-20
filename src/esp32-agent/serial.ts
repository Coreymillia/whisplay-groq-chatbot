import fs from "fs";
import path from "path";

export interface Esp32AgentSerialPortSummary {
  path: string;
  stablePath: string;
  label: string;
  manufacturer: string;
  product: string;
  vendorId: string;
  productId: string;
  serialNumber: string;
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
    };
  });
}
