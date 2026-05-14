#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <M5Stack.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

static const char CORE1_AP_SSID[] = "Core1Display-Setup";
static const char CORE1_DEFAULT_HOST[] = "http://10.160.0.203";
static const uint16_t CORE1_DEFAULT_POLL_MS = 1500;
static const uint16_t CORE1_MIN_POLL_MS = 500;
static const uint16_t CORE1_MAX_POLL_MS = 5000;
static const unsigned long CORE1_WIFI_RETRY_MS = 10000;
static const unsigned long CORE1_LONG_PRESS_MS = 1200;
static const uint16_t CORE1_DEFAULT_AUTO_SCROLL_MS = 0;

enum class DisplayMode : uint8_t {
  Split = 0,
  FullReply = 1,
};

enum class ScreenMode : uint8_t {
  Viewer,
  Settings,
};

enum class SettingsField : uint8_t {
  BorderColor,
  FontColor,
  AutoScroll,
  FontSize,
  DisplayMode,
};

struct ColorTheme {
  const char *label;
  uint16_t accent;
  uint16_t title;
  uint16_t body;
};

static constexpr ColorTheme CORE1_COLOR_THEMES[] = {
  {"Green", TFT_GREEN, TFT_CYAN, TFT_WHITE},
  {"Amber", TFT_YELLOW, TFT_ORANGE, TFT_WHITE},
  {"Ice", TFT_CYAN, TFT_BLUE, TFT_WHITE},
  {"Rose", TFT_MAGENTA, TFT_PINK, TFT_WHITE},
};

struct FontColorOption {
  const char *label;
  uint16_t color;
};

static constexpr FontColorOption CORE1_FONT_COLORS[] = {
  {"White", TFT_WHITE},
  {"Green", TFT_GREEN},
  {"Amber", TFT_YELLOW},
  {"Cyan", TFT_CYAN},
  {"Pink", TFT_MAGENTA},
};

static constexpr uint16_t CORE1_AUTO_SCROLL_OPTIONS[] = {0, 400, 700, 1000, 1500, 2200};

static char core1_wifi_ssid[64] = "";
static char core1_wifi_pass[64] = "";
static char core1_groqputer_url[128] = "";
static uint16_t core1_poll_ms = CORE1_DEFAULT_POLL_MS;
static uint16_t core1_auto_scroll_ms = CORE1_DEFAULT_AUTO_SCROLL_MS;
static uint8_t core1_font_scale = 1;
static uint8_t core1_color_theme_index = 0;
static uint8_t core1_font_color_index = 0;
static DisplayMode core1_display_mode = DisplayMode::Split;
static bool core1_has_settings = false;
static unsigned long core1_last_wifi_retry_ms = 0;

static DNSServer *core1_portal_dns = nullptr;
static WebServer *core1_portal_server = nullptr;
static String core1_portal_notice = "";

enum class FocusPane : uint8_t {
  Prompt,
  Reply,
};

static ScreenMode activeScreen = ScreenMode::Viewer;
static SettingsField activeSettingsField = SettingsField::BorderColor;
static FocusPane activePane = FocusPane::Reply;
static String latestUserMessage = "";
static String latestReplyMessage = "";
static String latestModelTag = "BOT";
static String latestPersona = "Unknown";
static String latestStatus = "idle";
static String latestPollError = "";
static unsigned long latestUpdatedAtMs = 0;
static unsigned long lastPollAttemptMs = 0;
static unsigned long lastSuccessfulPollMs = 0;
static int promptScrollOffset = 0;
static int replyScrollOffset = 0;
static bool uiDirty = true;
static bool footerDirty = true;
static wl_status_t lastDrawnWifiStatus = WL_IDLE_STATUS;
static long lastDrawnSyncAgeSeconds = -1;
static unsigned long lastAutoScrollMs = 0;
static unsigned long autoScrollPauseStartedMs = 0;

static String normalizeBaseUrl(const String &value) {
  String normalized = value;
  normalized.trim();
  while (normalized.endsWith("/")) {
    normalized.remove(normalized.length() - 1);
  }
  return normalized;
}

static uint16_t clampPollMs(int value) {
  if (value < CORE1_MIN_POLL_MS) return CORE1_MIN_POLL_MS;
  if (value > CORE1_MAX_POLL_MS) return CORE1_MAX_POLL_MS;
  return static_cast<uint16_t>(value);
}

static uint16_t clampAutoScrollMs(int value) {
  if (value < 0) return CORE1_DEFAULT_AUTO_SCROLL_MS;
  return static_cast<uint16_t>(value);
}

static uint8_t clampFontScale(int value) {
  if (value < 1) return 1;
  if (value > 3) return 3;
  return static_cast<uint8_t>(value);
}

static uint8_t clampThemeIndex(int value) {
  if (value < 0) return 0;
  if (value >= static_cast<int>(sizeof(CORE1_COLOR_THEMES) / sizeof(CORE1_COLOR_THEMES[0]))) {
    return static_cast<uint8_t>((sizeof(CORE1_COLOR_THEMES) / sizeof(CORE1_COLOR_THEMES[0])) - 1);
  }
  return static_cast<uint8_t>(value);
}

static uint8_t clampFontColorIndex(int value) {
  if (value < 0) return 0;
  if (value >= static_cast<int>(sizeof(CORE1_FONT_COLORS) / sizeof(CORE1_FONT_COLORS[0]))) {
    return static_cast<uint8_t>((sizeof(CORE1_FONT_COLORS) / sizeof(CORE1_FONT_COLORS[0])) - 1);
  }
  return static_cast<uint8_t>(value);
}

static ColorTheme currentTheme() {
  return CORE1_COLOR_THEMES[clampThemeIndex(core1_color_theme_index)];
}

static uint16_t currentFontColor() {
  return CORE1_FONT_COLORS[clampFontColorIndex(core1_font_color_index)].color;
}

static int currentBodyCharWidth() {
  return 6 * core1_font_scale;
}

static int currentBodyLineHeight() {
  return (8 * core1_font_scale) + 2;
}

static String escapeHtml(const String &value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("\"", "&quot;");
  escaped.replace("'", "&#39;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  return escaped;
}

static void loadSettings() {
  Preferences prefs;
  prefs.begin("core1disp", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String groqputerUrl = prefs.getString("groqputerUrl", CORE1_DEFAULT_HOST);
  core1_poll_ms = clampPollMs(static_cast<int>(prefs.getUInt("pollMs", CORE1_DEFAULT_POLL_MS)));
  core1_auto_scroll_ms = clampAutoScrollMs(static_cast<int>(prefs.getUInt("autoScrollMs", CORE1_DEFAULT_AUTO_SCROLL_MS)));
  core1_font_scale = clampFontScale(static_cast<int>(prefs.getUChar("fontScale", 1)));
  core1_color_theme_index = clampThemeIndex(static_cast<int>(prefs.getUChar("themeIdx", 0)));
  core1_font_color_index = clampFontColorIndex(static_cast<int>(prefs.getUChar("fontColorIdx", 0)));
  core1_display_mode = prefs.getBool("fullReply", false) ? DisplayMode::FullReply : DisplayMode::Split;
  prefs.end();

  ssid.toCharArray(core1_wifi_ssid, sizeof(core1_wifi_ssid));
  pass.toCharArray(core1_wifi_pass, sizeof(core1_wifi_pass));
  normalizeBaseUrl(groqputerUrl).toCharArray(core1_groqputer_url, sizeof(core1_groqputer_url));
  core1_has_settings = core1_wifi_ssid[0] != '\0' && core1_groqputer_url[0] != '\0';
}

static void saveSettings(const String &ssid, const String &pass, const String &groqputerUrl, uint16_t pollMs) {
  String normalizedUrl = normalizeBaseUrl(groqputerUrl);
  Preferences prefs;
  prefs.begin("core1disp", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("groqputerUrl", normalizedUrl);
  prefs.putUInt("pollMs", clampPollMs(pollMs));
  prefs.putUInt("autoScrollMs", core1_auto_scroll_ms);
  prefs.putUChar("fontScale", core1_font_scale);
  prefs.putUChar("themeIdx", core1_color_theme_index);
  prefs.putUChar("fontColorIdx", core1_font_color_index);
  prefs.putBool("fullReply", core1_display_mode == DisplayMode::FullReply);
  prefs.end();

  ssid.toCharArray(core1_wifi_ssid, sizeof(core1_wifi_ssid));
  pass.toCharArray(core1_wifi_pass, sizeof(core1_wifi_pass));
  normalizedUrl.toCharArray(core1_groqputer_url, sizeof(core1_groqputer_url));
  core1_poll_ms = clampPollMs(pollMs);
  core1_has_settings = core1_wifi_ssid[0] != '\0' && core1_groqputer_url[0] != '\0';
}

static void persistViewerSettings() {
  Preferences prefs;
  prefs.begin("core1disp", false);
  prefs.putUInt("autoScrollMs", core1_auto_scroll_ms);
  prefs.putUChar("fontScale", core1_font_scale);
  prefs.putUChar("themeIdx", core1_color_theme_index);
  prefs.putUChar("fontColorIdx", core1_font_color_index);
  prefs.putBool("fullReply", core1_display_mode == DisplayMode::FullReply);
  prefs.end();
}

static String buildPortalHtml() {
  String html;
  html.reserve(2600);
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Core1 Display Setup</title><style>";
  html += "body{background:#081018;color:#d9f0ff;font-family:Arial,sans-serif;max-width:560px;margin:auto;padding:20px;}";
  html += "h1{color:#67f056;}label{display:block;margin-top:14px;font-weight:bold;color:#9fd2ff;}";
  html += "input{width:100%;box-sizing:border-box;padding:10px;border-radius:6px;border:1px solid #35516f;background:#111a24;color:#f2fbff;}";
  html += "button{width:100%;padding:14px;margin-top:18px;border:none;border-radius:8px;background:#1b77ff;color:#fff;font-weight:bold;}";
  html += ".hint{font-size:.95em;color:#9ab4c9;}.notice{margin:16px 0;padding:12px;border-radius:8px;background:#143050;}";
  html += "</style></head><body>";
  html += "<h1>Core1 Display Setup</h1>";
  html += "<p class='hint'>Connect the Core1 to Wi-Fi and point it at the Groqputer companion API.</p>";
  if (core1_portal_notice.length()) {
    html += "<div class='notice'>";
    html += escapeHtml(core1_portal_notice);
    html += "</div>";
  }
  html += "<form method='post' action='/save'>";
  html += "<label>WiFi SSID</label><input name='ssid' maxlength='63' required value='" + escapeHtml(String(core1_wifi_ssid)) + "'>";
  html += "<label>WiFi Password</label><input name='pass' type='password' maxlength='63' value='" + escapeHtml(String(core1_wifi_pass)) + "'>";
  html += "<label>Groqputer Base URL</label><input name='groqputerUrl' maxlength='127' required placeholder='http://10.160.0.203' value='" + escapeHtml(String(core1_groqputer_url[0] ? core1_groqputer_url : CORE1_DEFAULT_HOST)) + "'>";
  html += "<label>Poll Interval (ms)</label><input name='pollMs' type='number' min='500' max='5000' required value='" + String(core1_poll_ms) + "'>";
  html += "<button type='submit'>Save & Reboot</button></form>";
  html += "<p class='hint'>The Core1 polls <code>/api/companion/chat</code> on the Groqputer and renders the latest prompt + reply locally.</p>";
  html += "</body></html>";
  return html;
}

static void handlePortalRoot() {
  core1_portal_server->send(200, "text/html", buildPortalHtml());
}

static void handlePortalSave() {
  String ssid = core1_portal_server->arg("ssid");
  String pass = core1_portal_server->arg("pass");
  String groqputerUrl = core1_portal_server->arg("groqputerUrl");
  uint16_t pollMs = clampPollMs(core1_portal_server->arg("pollMs").toInt());
  ssid.trim();
  groqputerUrl.trim();
  if (!ssid.length()) {
    core1_portal_notice = "WiFi SSID is required.";
    handlePortalRoot();
    return;
  }
  if (!groqputerUrl.length()) {
    core1_portal_notice = "Groqputer base URL is required.";
    handlePortalRoot();
    return;
  }

  saveSettings(ssid, pass, groqputerUrl, pollMs);
  core1_portal_server->send(200, "text/html", "<html><body style='background:#081018;color:#d9f0ff;font-family:Arial;padding:24px'><h2>Saved. Rebooting...</h2></body></html>");
  delay(1200);
  ESP.restart();
}

static void runSetupPortal() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(CORE1_AP_SSID);

  if (!core1_portal_dns) {
    core1_portal_dns = new DNSServer();
  }
  if (!core1_portal_server) {
    core1_portal_server = new WebServer(80);
  }

  core1_portal_dns->start(53, "*", WiFi.softAPIP());
  core1_portal_server->on("/", handlePortalRoot);
  core1_portal_server->on("/save", HTTP_POST, handlePortalSave);
  core1_portal_server->onNotFound(handlePortalRoot);
  core1_portal_server->begin();

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setCursor(16, 20);
  M5.Lcd.print("Core1 Display Setup");
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setCursor(16, 56);
  M5.Lcd.print("Connect to:");
  M5.Lcd.setCursor(16, 80);
  M5.Lcd.print(CORE1_AP_SSID);
  M5.Lcd.setCursor(16, 116);
  M5.Lcd.print("Open:");
  M5.Lcd.setCursor(16, 140);
  M5.Lcd.print("http://192.168.4.1");

  while (true) {
    core1_portal_dns->processNextRequest();
    core1_portal_server->handleClient();
    delay(2);
  }
}

static void startWifiStation() {
  if (core1_wifi_ssid[0] == '\0') {
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(core1_wifi_ssid, core1_wifi_pass);
  core1_last_wifi_retry_ms = millis();
}

static bool ensureWifiConnected(bool forceRetry = false) {
  if (core1_wifi_ssid[0] == '\0') {
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  unsigned long now = millis();
  if (forceRetry || now - core1_last_wifi_retry_ms >= CORE1_WIFI_RETRY_MS) {
    WiFi.disconnect();
    WiFi.begin(core1_wifi_ssid, core1_wifi_pass);
    core1_last_wifi_retry_ms = now;
  }
  return false;
}

static String normalizeMessage(const String &value, const String &fallback) {
  String normalized = value;
  normalized.replace("\r", " ");
  normalized.replace("\n", " ");
  normalized.trim();
  return normalized.length() ? normalized : fallback;
}

static bool fetchCompanionChat(String &errorOut) {
  errorOut = "";
  if (!ensureWifiConnected()) {
    errorOut = "WiFi disconnected";
    latestStatus = "offline";
    latestPollError = errorOut;
    return false;
  }

  HTTPClient http;
  String endpoint = normalizeBaseUrl(String(core1_groqputer_url)) + "/api/companion/chat";
  if (!http.begin(endpoint)) {
    errorOut = "HTTP begin failed";
    latestStatus = "error";
    latestPollError = errorOut;
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  String response = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    errorOut = response.length() ? response : String("HTTP ") + code;
    latestStatus = "error";
    latestPollError = errorOut;
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    errorOut = "JSON parse failed";
    latestStatus = "error";
    latestPollError = errorOut;
    return false;
  }

  bool ready = doc["ready"] | false;
  latestStatus = String(doc["status"] | "idle");
  latestModelTag = normalizeMessage(String(doc["modelTag"] | "BOT"), "BOT");
  latestPersona = normalizeMessage(String(doc["persona"] | "Unknown"), "Unknown");
  latestUserMessage = normalizeMessage(String(doc["latestUser"] | ""), "Waiting for the first prompt.");
  latestReplyMessage = normalizeMessage(String(doc["latestReply"] | ""), "Waiting for the first reply.");
  latestUpdatedAtMs = millis();
  if (!ready) {
    errorOut = "Groqputer not ready";
    latestPollError = errorOut;
    return false;
  }

  latestPollError = "";
  lastSuccessfulPollMs = millis();
  return true;
}

static void fillWrappedLines(const String &sourceText, String *lines, int &lineCount, int maxLines, int maxChars) {
  lineCount = 0;
  String source = sourceText;
  source.replace("\r", "");
  String currentLine;
  for (size_t i = 0; i < source.length(); i++) {
    char c = source.charAt(i);
    if (c == '\n') {
      if (lineCount < maxLines) lines[lineCount++] = currentLine;
      currentLine = "";
      continue;
    }
    currentLine += c;
    if (currentLine.length() >= static_cast<size_t>(maxChars)) {
      int breakPos = currentLine.lastIndexOf(' ');
      if (breakPos > maxChars / 2) {
        if (lineCount < maxLines) lines[lineCount++] = currentLine.substring(0, breakPos);
        currentLine = currentLine.substring(breakPos + 1);
      } else {
        if (lineCount < maxLines) lines[lineCount++] = currentLine;
        currentLine = "";
      }
    }
  }
  if (currentLine.length() && lineCount < maxLines) {
    lines[lineCount++] = currentLine;
  }
}

static int scrollOffsetForPane(FocusPane pane) {
  return pane == FocusPane::Prompt ? promptScrollOffset : replyScrollOffset;
}

static void setScrollOffsetForPane(FocusPane pane, int value) {
  if (pane == FocusPane::Prompt) {
    promptScrollOffset = value;
  } else {
    replyScrollOffset = value;
  }
}

static int maxScrollForText(const String &text, int width, int height, int maxCharsPerLine, int lineHeight) {
  String lines[128];
  int lineCount = 0;
  fillWrappedLines(text, lines, lineCount, 128, maxCharsPerLine);
  int visibleLines = max(1, height / lineHeight);
  return max(0, lineCount - visibleLines);
}

static int paneTextHeight(FocusPane pane) {
  if (core1_display_mode == DisplayMode::FullReply) {
    return 182 - 38;
  }
  return pane == FocusPane::Prompt ? (72 - 38) : (102 - 38);
}

static void resetScrollLoopState() {
  autoScrollPauseStartedMs = 0;
  lastAutoScrollMs = 0;
}

static void drawPane(int x, int y, int w, int h, const char *title, const String &text, bool active, FocusPane pane) {
  ColorTheme theme = currentTheme();
  uint16_t borderColor = active ? theme.accent : TFT_DARKGREY;
  M5.Lcd.drawRoundRect(x, y, w, h, 6, borderColor);
  M5.Lcd.setTextColor(active ? theme.accent : theme.title, TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(x + 8, y + 6);
  M5.Lcd.print(title);
  M5.Lcd.drawFastHLine(x + 6, y + 24, w - 12, borderColor);

  const int textX = x + 8;
  const int textY = y + 30;
  const int textW = w - 16;
  const int textH = h - 38;
  const int maxChars = max(8, textW / currentBodyCharWidth());
  const int lineHeight = currentBodyLineHeight();
  const int visibleLines = max(1, textH / lineHeight);
  String lines[128];
  int lineCount = 0;
  fillWrappedLines(text, lines, lineCount, 128, maxChars);
  int maxScroll = max(0, lineCount - visibleLines);
  int scrollOffset = constrain(scrollOffsetForPane(pane), 0, maxScroll);
  setScrollOffsetForPane(pane, scrollOffset);

  M5.Lcd.setTextColor(currentFontColor(), TFT_BLACK);
  M5.Lcd.setTextFont(1);
  M5.Lcd.setTextSize(core1_font_scale);
  int cursorY = textY;
  for (int i = scrollOffset; i < min(lineCount, scrollOffset + visibleLines); i++) {
    M5.Lcd.setCursor(textX, cursorY);
    M5.Lcd.print(lines[i]);
    cursorY += lineHeight;
  }
  M5.Lcd.setTextSize(1);

  if (maxScroll > 0) {
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.setCursor(x + w - 40, y + h - 10);
    M5.Lcd.print(String(scrollOffset + 1) + "/" + String(maxScroll + 1));
  }
}

static String currentFooterInfoText() {
  String infoText = latestPollError.length()
    ? latestPollError
    : (lastSuccessfulPollMs ? String((millis() - lastSuccessfulPollMs) / 1000UL) + "s ago" : "never synced");
  if (infoText.length() > 28) {
    infoText = infoText.substring(0, 28);
  }
  return infoText;
}

static void drawFooterArea() {
  M5.Lcd.fillRect(0, 215, 320, 25, TFT_BLACK);
  M5.Lcd.drawFastHLine(0, 214, 320, TFT_DARKGREY);
  M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Lcd.setTextFont(1);

  String hostText = normalizeBaseUrl(String(core1_groqputer_url));
  if (hostText.length() > 28) {
    hostText = hostText.substring(0, 28);
  }
  M5.Lcd.setCursor(8, 222);
  M5.Lcd.print(hostText.length() ? hostText : "No Groqputer host set");

  M5.Lcd.setCursor(8, 232);
  M5.Lcd.print(currentFooterInfoText());

  M5.Lcd.setCursor(150, 232);
  M5.Lcd.print("A up B dn C pane");
  footerDirty = false;
}

static String settingsFieldLabel(SettingsField field) {
  switch (field) {
    case SettingsField::BorderColor:
      return "Border Color";
    case SettingsField::FontColor:
      return "Font Color";
    case SettingsField::AutoScroll:
      return "Auto Scroll";
    case SettingsField::FontSize:
      return "Font Size";
    case SettingsField::DisplayMode:
      return "Display Mode";
  }
  return "";
}

static String autoScrollLabel() {
  if (core1_auto_scroll_ms == 0) {
    return "Off";
  }
  return String(core1_auto_scroll_ms) + " ms";
}

static String settingsFieldValue(SettingsField field) {
  switch (field) {
    case SettingsField::BorderColor:
      return CORE1_COLOR_THEMES[core1_color_theme_index].label;
    case SettingsField::FontColor:
      return CORE1_FONT_COLORS[core1_font_color_index].label;
    case SettingsField::AutoScroll:
      return autoScrollLabel();
    case SettingsField::FontSize:
      return String(core1_font_scale) + "x";
    case SettingsField::DisplayMode:
      return core1_display_mode == DisplayMode::Split ? "Split" : "Full BOT";
  }
  return "";
}

static void drawSettingsUi() {
  ColorTheme theme = currentTheme();
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(theme.accent, TFT_BLACK);
  M5.Lcd.setCursor(10, 8);
  M5.Lcd.print("Core1 Settings");
  M5.Lcd.drawFastHLine(0, 26, 320, TFT_DARKGREY);

  int cursorY = 40;
  const SettingsField fields[] = {
    SettingsField::BorderColor,
    SettingsField::FontColor,
    SettingsField::AutoScroll,
    SettingsField::FontSize,
    SettingsField::DisplayMode,
  };

  for (SettingsField field : fields) {
    bool active = field == activeSettingsField;
    M5.Lcd.setTextColor(active ? theme.accent : TFT_CYAN, TFT_BLACK);
    M5.Lcd.setCursor(12, cursorY);
    M5.Lcd.print(active ? "> " : "  ");
    M5.Lcd.print(settingsFieldLabel(field));
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(170, cursorY);
    M5.Lcd.print(settingsFieldValue(field));
    cursorY += 28;
  }

  M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Lcd.drawFastHLine(0, 214, 320, TFT_DARKGREY);
  M5.Lcd.setTextFont(1);
  M5.Lcd.setCursor(8, 222);
  M5.Lcd.print("A next  B change  C close");
  M5.Lcd.setCursor(8, 232);
  M5.Lcd.print("Hold A settings  Hold C AP");
  uiDirty = false;
  footerDirty = false;
}

static void drawUi() {
  if (activeScreen == ScreenMode::Settings) {
    drawSettingsUi();
    return;
  }
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);
  ColorTheme theme = currentTheme();
  M5.Lcd.setTextColor(theme.accent, TFT_BLACK);
  M5.Lcd.setCursor(10, 8);
  M5.Lcd.print("Core1 Chat");

  M5.Lcd.setTextColor(WiFi.status() == WL_CONNECTED ? TFT_GREEN : TFT_RED, TFT_BLACK);
  M5.Lcd.setCursor(116, 8);
  M5.Lcd.print(WiFi.status() == WL_CONNECTED ? "WiFi" : "NoWiFi");

  M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Lcd.setCursor(188, 8);
  M5.Lcd.print(latestModelTag);

  M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Lcd.setCursor(232, 8);
  M5.Lcd.print(latestStatus);
  M5.Lcd.drawFastHLine(0, 26, 320, TFT_DARKGREY);

  if (core1_display_mode == DisplayMode::Split) {
    drawPane(8, 34, 304, 72, "YOU", latestUserMessage, activePane == FocusPane::Prompt, FocusPane::Prompt);
    drawPane(8, 114, 304, 102, "BOT", latestReplyMessage, activePane == FocusPane::Reply, FocusPane::Reply);
  } else {
    drawPane(8, 34, 304, 182, "BOT", latestReplyMessage, true, FocusPane::Reply);
  }
  drawFooterArea();
  lastDrawnWifiStatus = WiFi.status();
  lastDrawnSyncAgeSeconds = lastSuccessfulPollMs ? static_cast<long>((millis() - lastSuccessfulPollMs) / 1000UL) : -1;
  uiDirty = false;
}

static void scrollActivePane(int delta) {
  FocusPane targetPane = core1_display_mode == DisplayMode::FullReply ? FocusPane::Reply : activePane;
  const String &text = targetPane == FocusPane::Prompt ? latestUserMessage : latestReplyMessage;
  int maxScroll = maxScrollForText(text, 288, paneTextHeight(targetPane), max(8, 288 / currentBodyCharWidth()), currentBodyLineHeight());
  int current = scrollOffsetForPane(targetPane);
  int next = constrain(current + delta, 0, maxScroll);
  if (next != current) {
    setScrollOffsetForPane(targetPane, next);
    resetScrollLoopState();
    uiDirty = true;
  }
}

static void cycleSettingsField() {
  int index = static_cast<int>(activeSettingsField);
  index = (index + 1) % 5;
  activeSettingsField = static_cast<SettingsField>(index);
  uiDirty = true;
}

static void adjustSettingValue() {
  switch (activeSettingsField) {
    case SettingsField::BorderColor:
      core1_color_theme_index = (core1_color_theme_index + 1) % (sizeof(CORE1_COLOR_THEMES) / sizeof(CORE1_COLOR_THEMES[0]));
      break;
    case SettingsField::FontColor:
      core1_font_color_index = (core1_font_color_index + 1) % (sizeof(CORE1_FONT_COLORS) / sizeof(CORE1_FONT_COLORS[0]));
      break;
    case SettingsField::AutoScroll: {
      size_t count = sizeof(CORE1_AUTO_SCROLL_OPTIONS) / sizeof(CORE1_AUTO_SCROLL_OPTIONS[0]);
      size_t currentIndex = 0;
      for (size_t i = 0; i < count; i++) {
        if (CORE1_AUTO_SCROLL_OPTIONS[i] == core1_auto_scroll_ms) {
          currentIndex = i;
          break;
        }
      }
      currentIndex = (currentIndex + 1) % count;
      core1_auto_scroll_ms = CORE1_AUTO_SCROLL_OPTIONS[currentIndex];
      break;
    }
    case SettingsField::FontSize:
      core1_font_scale = core1_font_scale >= 3 ? 1 : core1_font_scale + 1;
      break;
    case SettingsField::DisplayMode:
      core1_display_mode = core1_display_mode == DisplayMode::Split ? DisplayMode::FullReply : DisplayMode::Split;
      activePane = FocusPane::Reply;
      break;
  }
  persistViewerSettings();
  resetScrollLoopState();
  uiDirty = true;
  footerDirty = true;
}

static void pollAutoScroll() {
  if (activeScreen != ScreenMode::Viewer || core1_auto_scroll_ms == 0) {
    return;
  }
  unsigned long now = millis();
  if (lastAutoScrollMs != 0 && now - lastAutoScrollMs < core1_auto_scroll_ms) {
    return;
  }

  FocusPane targetPane = core1_display_mode == DisplayMode::FullReply ? FocusPane::Reply : activePane;
  const String &text = targetPane == FocusPane::Prompt ? latestUserMessage : latestReplyMessage;
  int maxScroll = maxScrollForText(text, 288, paneTextHeight(targetPane), max(8, 288 / currentBodyCharWidth()), currentBodyLineHeight());
  int current = scrollOffsetForPane(targetPane);
  if (maxScroll > 0 && current < maxScroll) {
    setScrollOffsetForPane(targetPane, current + 1);
    if (current + 1 >= maxScroll) {
      autoScrollPauseStartedMs = now;
    }
    uiDirty = true;
  } else if (maxScroll > 0 && current >= maxScroll) {
    if (autoScrollPauseStartedMs == 0) {
      autoScrollPauseStartedMs = now;
    } else if (now - autoScrollPauseStartedMs >= 1600) {
      setScrollOffsetForPane(targetPane, 0);
      autoScrollPauseStartedMs = 0;
      uiDirty = true;
    }
  }
  lastAutoScrollMs = now;
}

static void handleButtons() {
  static bool aLongHandled = false;
  static bool cLongHandled = false;

  if (M5.BtnA.wasPressed()) {
    aLongHandled = false;
  }
  if (M5.BtnA.pressedFor(CORE1_LONG_PRESS_MS) && !aLongHandled) {
    aLongHandled = true;
    activeScreen = ScreenMode::Settings;
    resetScrollLoopState();
    uiDirty = true;
  }
  if (M5.BtnA.wasReleased() && !aLongHandled) {
    if (activeScreen == ScreenMode::Settings) {
      cycleSettingsField();
    } else {
      scrollActivePane(-1);
    }
  }
  if (M5.BtnB.wasPressed()) {
    if (activeScreen == ScreenMode::Settings) {
      adjustSettingValue();
    } else {
      scrollActivePane(1);
    }
  }

  if (M5.BtnC.wasPressed()) {
    cLongHandled = false;
  }
  if (M5.BtnC.pressedFor(CORE1_LONG_PRESS_MS) && !cLongHandled) {
    cLongHandled = true;
    runSetupPortal();
  }
  if (M5.BtnC.wasReleased() && !cLongHandled) {
    if (activeScreen == ScreenMode::Settings) {
      activeScreen = ScreenMode::Viewer;
      resetScrollLoopState();
      uiDirty = true;
    } else if (core1_display_mode == DisplayMode::Split) {
      activePane = activePane == FocusPane::Prompt ? FocusPane::Reply : FocusPane::Prompt;
      resetScrollLoopState();
      uiDirty = true;
    }
  }
}

void setup() {
  M5.begin();
  M5.Power.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setCursor(16, 20);
  M5.Lcd.print("Core1 Display");

  loadSettings();
  if (!core1_has_settings) {
    runSetupPortal();
  }

  latestUserMessage = "Waiting for the first prompt.";
  latestReplyMessage = "Waiting for the first reply.";
  startWifiStation();
  drawUi();
}

void loop() {
  M5.update();
  handleButtons();
  ensureWifiConnected();
  pollAutoScroll();

  if (WiFi.status() != lastDrawnWifiStatus) {
    uiDirty = true;
  }

  if (millis() - lastPollAttemptMs >= core1_poll_ms) {
    lastPollAttemptMs = millis();
    String previousUser = latestUserMessage;
    String previousReply = latestReplyMessage;
    String previousModelTag = latestModelTag;
    String previousStatus = latestStatus;
    String previousPersona = latestPersona;
    String previousError = latestPollError;
    unsigned long previousLastSuccess = lastSuccessfulPollMs;
    String error;
    fetchCompanionChat(error);
    if (latestUserMessage != previousUser ||
        latestReplyMessage != previousReply ||
        latestModelTag != previousModelTag ||
        latestStatus != previousStatus ||
        latestPersona != previousPersona) {
      if (latestUserMessage != previousUser) {
        promptScrollOffset = 0;
      }
      if (latestReplyMessage != previousReply) {
        replyScrollOffset = 0;
      }
      resetScrollLoopState();
      uiDirty = true;
    }
    if (latestPollError != previousError || lastSuccessfulPollMs != previousLastSuccess) {
      footerDirty = true;
    }
  }

  long syncAgeSeconds = lastSuccessfulPollMs ? static_cast<long>((millis() - lastSuccessfulPollMs) / 1000UL) : -1;
  if (syncAgeSeconds != lastDrawnSyncAgeSeconds) {
    footerDirty = true;
    lastDrawnSyncAgeSeconds = syncAgeSeconds;
  }

  if (uiDirty) {
    drawUi();
  } else if (footerDirty) {
    drawFooterArea();
  }
  delay(25);
}
