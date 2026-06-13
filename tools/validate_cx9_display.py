from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DISPLAY = (ROOT / "main" / "display_driver.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "main" / "main.cpp").read_text(encoding="utf-8")
DEFAULTS = (ROOT / "sdkconfig.defaults").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


require("0x03" in DISPLAY and "kXl9555ConfigPort0" in DISPLAY, "XL9555 port0 config must match atk-dnesp32s3")
require("0xF0" in DISPLAY and "kXl9555ConfigPort1" in DISPLAY, "XL9555 port1 config must match atk-dnesp32s3")
require("SetXl9555Output(8, true)" in DISPLAY, "LCD enable/backlight bit 8 must be driven high")
require("SetXl9555Output(2, false)" in DISPLAY, "LCD reset/control bit 2 must be driven low")
require("esp_lcd_new_panel_st7789" in DISPLAY, "ST7789 must use ESP-IDF esp_lcd panel driver")
require("esp_lcd_panel_invert_color(panel_, true)" in DISPLAY, "ST7789 inversion command must be sent for this panel")
require("RunSolidColorSelfTest()" in DISPLAY, "Display init must run a solid-color SPI smoke test")
require("MALLOC_CAP_INTERNAL" in DISPLAY, "LVGL buffers need internal-RAM fallback")
require("CONFIG_LV_COLOR_16_SWAP=y" in DEFAULTS, "LVGL RGB565 byte swap must match esp_lcd ST7789")
require("CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y" in DEFAULTS, "USB-Serial-JTAG must be primary console in defaults")
require("esp_rom_printf" in MAIN, "app_main must emit early ROM debug output")

print("CX-9 display validation passed")
