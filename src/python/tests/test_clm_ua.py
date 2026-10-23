# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by Deeperlevel, developed with community contributions.
# https://github.com/deeperlevel/clm
# Contact: deeparchive@outlook.com
#
# Public Domain — CC0 1.0 Universal
# https://creativecommons.org/publicdomain/zero/1.0/
#
# Free to use, modify, redistribute, and bundle for any purpose.
# Attribution and retention of this notice are appreciated, not required.
#
# Provided AS IS, without warranty or liability.
#
# Don't start from scratch, bro.
# =============================================================================

import unittest

from clm_ua import (
    clm_ua_parse,
    clm_ua_browser,
    clm_ua_is_bot
)


class UserAgentTests(unittest.TestCase):
    def test_empty_and_unknown(self):
        for input_value in (None, "", "unrecognized-agent"):
            parsed = clm_ua_parse(input_value)
            self.assertEqual(parsed.browser, "Unknown")
            self.assertEqual(parsed.os, "Unknown")
            self.assertEqual(parsed.device_type, "Desktop")
            self.assertTrue(parsed.is_desktop)
            self.assertFalse(parsed.is_bot)
        self.assertEqual(clm_ua_parse(None).raw, "")

    def test_chrome_windows(self):
        ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36"
        result = clm_ua_parse(ua)
        self.assertEqual((result.browser, result.browser_version), ("Chrome", "125.0.0.0"))
        self.assertEqual((result.os, result.os_version), ("Windows", "10.0"))
        self.assertTrue(result.is_desktop)

    def test_edge_and_opera_precede_chrome(self):
        base = "Mozilla/5.0 Chrome/125.0.0.0 Safari/537.36 "
        self.assertEqual(clm_ua_browser(base + "Edg/125.0.1")[0], "Edge")
        self.assertEqual(clm_ua_browser(base + "Edge/18.0")[0], "Edge")
        self.assertEqual(clm_ua_browser(base + "OPR/110.0")[0], "Opera")
        self.assertEqual(clm_ua_browser(base + "SamsungBrowser/25.0")[0], "Samsung Internet")

    def test_safari_macos(self):
        ua = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.5 Safari/605.1.15"
        result = clm_ua_parse(ua)
        self.assertEqual((result.browser, result.browser_version), ("Safari", "17.5"))
        self.assertEqual((result.os, result.os_version), ("macOS", "10.15.7"))

    def test_firefox_linux(self):
        ua = "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:126.0) Gecko/20100101 Firefox/126.0"
        result = clm_ua_parse(ua)
        self.assertEqual(result.browser, "Firefox")
        self.assertEqual(result.os, "Linux")
        self.assertEqual(result.os_version, "Unknown")

    def test_ios_iphone_and_ipad(self):
        iphone = "Mozilla/5.0 (iPhone; CPU iPhone OS 17_4 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4 Mobile/15E148 Safari/604.1"
        ipad = "Mozilla/5.0 (iPad; CPU OS 16_6 like Mac OS X) AppleWebKit/605.1.15 Version/16.6 Mobile/15E148 Safari/604.1"
        self.assertEqual(clm_ua_parse(iphone).os, "iOS")
        self.assertEqual(clm_ua_parse(iphone).os_version, "17.4")
        self.assertTrue(clm_ua_parse(iphone).is_mobile)
        self.assertEqual(clm_ua_parse(ipad).os, "iOS")
        self.assertTrue(clm_ua_parse(ipad).is_tablet)
        self.assertFalse(clm_ua_parse(ipad).is_mobile)

    def test_android_phone_and_tablet(self):
        phone = "Mozilla/5.0 (Linux; Android 14; Pixel 7) AppleWebKit/537.36 Chrome/125.0 Mobile Safari/537.36"
        tablet = "Mozilla/5.0 (Linux; Android 13; SM-X700) AppleWebKit/537.36 Chrome/125.0 Safari/537.36"
        self.assertEqual(clm_ua_parse(phone).os, "Android")
        self.assertTrue(clm_ua_parse(phone).is_mobile)
        self.assertTrue(clm_ua_parse(tablet).is_tablet)

    def test_ios_browser_tokens(self):
        self.assertEqual(clm_ua_browser("CriOS/125.0 Mobile Safari/605.1")[0], "Chrome")
        self.assertEqual(clm_ua_browser("FxiOS/120.0 Mobile Safari/605.1")[0], "Firefox")
        self.assertEqual(clm_ua_browser("EdgiOS/125.0 Mobile Safari/605.1")[0], "Edge")

    def test_bot(self):
        self.assertTrue(clm_ua_parse("Googlebot/2.1 (+https://google.com/bot.html)").is_bot)
        self.assertTrue(clm_ua_is_bot("SomeCrawler/1.0"))
        self.assertFalse(clm_ua_is_bot("Mozilla/5.0 Safari/605.1"))

    def test_invalid_argument(self):
        with self.assertRaises(TypeError):
            clm_ua_parse(123)


if __name__ == "__main__":
    unittest.main()
