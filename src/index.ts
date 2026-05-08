import ChatFlow from "./core/ChatFlow";
import dotenv from "dotenv";
import { startBatteryStatus } from "./status/battery-status";
import { startWifiStatus } from "./status/wifi-status";
import { startVpnStatus } from "./status/vpn-status";
import { startPiSugarButtonSupport } from "./device/pisugar-button";

dotenv.config();

startBatteryStatus();
startWifiStatus();
startVpnStatus();
void startPiSugarButtonSupport();

new ChatFlow({
  enableCamera: process.env.ENABLE_CAMERA === "true",
});
