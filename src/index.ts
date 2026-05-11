import ChatFlow from "./core/ChatFlow";
import dotenv from "dotenv";
import { startBatteryStatus } from "./status/battery-status";
import { startWifiStatus } from "./status/wifi-status";
import { startVpnStatus } from "./status/vpn-status";
import { syncGroqUsageDisplay } from "./status/groq-usage";
import { startPiSugarButtonSupport } from "./device/pisugar-button";
import { startRoomMonitor } from "./device/room-monitor";

dotenv.config();

startBatteryStatus();
startWifiStatus();
startVpnStatus();
syncGroqUsageDisplay();
void startPiSugarButtonSupport();
startRoomMonitor();

new ChatFlow({
  enableCamera: process.env.ENABLE_CAMERA === "true",
});
