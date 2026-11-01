#pragma once

#include "test.hpp"
#include "../clm/ua.hpp"

static int clm_ua_tests(void)
{
	clm_test_result result = {0};
	clm_test_begin("CLM - User-Agent Parser");
	const clm_id source_id = 96000;
	const clm_id browser_id = 96001;
	const clm_id browser_version_id = 96002;
	const clm_id os_id = 96003;
	const clm_id os_version_id = 96004;
	const char desktop_agent[] =
		"Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
		"AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
	clm_ua_info info;
	char browser[CLM_UA_NAME_MAX];
	char browser_version[CLM_UA_VERSION_MAX];
	bool is_bot = true;
	clm_ua_device device = CLM_UA_MOBILE;

	CLM_ASSERT(clm_string_new(source_id, desktop_agent) &&
		clm_string_new(browser_id, "") &&
		clm_string_new(browser_version_id, "") &&
		clm_string_new(os_id, "") && clm_string_new(os_version_id, "") &&
		clm_ua_parse(source_id, &info) == CLM_UA_OPERATION_SUCCESS &&
		info.raw_string_id == source_id &&
		strcmp(info.browser, "Chrome") == 0 &&
		strcmp(info.browser_version, "120.0.0.0") == 0 &&
		strcmp(info.os, "Windows") == 0 &&
		strcmp(info.os_version, "10.0") == 0 &&
		info.device_type == CLM_UA_DESKTOP && info.is_desktop && !info.is_bot,
		"parse a CLM string ID into the User-Agent summary");
	CLM_ASSERT(clm_ua_browser(source_id, browser_id, browser_version_id) ==
		CLM_UA_OPERATION_SUCCESS &&
		clm_string_contains(browser_id, "Chrome") &&
		clm_string_contains(browser_version_id, "120.0.0.0"),
		"extract browser data into CLM string IDs");
	CLM_ASSERT(clm_ua_browser_to(source_id, browser, sizeof(browser),
		browser_version, sizeof(browser_version)) == CLM_UA_OPERATION_SUCCESS &&
		strcmp(browser, "Chrome") == 0 &&
		strcmp(browser_version, "120.0.0.0") == 0 &&
		clm_ua_os(source_id, os_id, os_version_id) == CLM_UA_OPERATION_SUCCESS &&
		clm_ua_is_bot(source_id, &is_bot) == CLM_UA_OPERATION_SUCCESS && !is_bot &&
		clm_ua_device_type(source_id, &device) == CLM_UA_OPERATION_SUCCESS &&
		device == CLM_UA_DESKTOP,
		"use _to buffers and ID-based OS/device helpers");
	CLM_ASSERT(clm_ua_parse_from(
		"Mozilla/5.0 (iPhone; CPU iPhone OS 17_2 like Mac OS X) "
		"Version/17.2 Mobile/15E148 Safari/604.1", &info) ==
		CLM_UA_OPERATION_SUCCESS && info.raw_string_id == 0 &&
		strcmp(info.browser, "Safari") == 0 &&
		strcmp(info.browser_version, "17.2") == 0 &&
		strcmp(info.os, "iOS") == 0 && strcmp(info.os_version, "17.2") == 0 &&
		info.device_type == CLM_UA_MOBILE && info.is_mobile,
		"parse raw UTF-8 input through the _from variant");
	CLM_ASSERT(clm_ua_is_bot_from("ExampleBot/1.0", &is_bot) ==
		CLM_UA_OPERATION_SUCCESS && is_bot &&
		clm_ua_device_type_from("Android 14; Tablet", &device) ==
		CLM_UA_OPERATION_SUCCESS && device == CLM_UA_TABLET &&
		clm_ua_parse(source_id, NULL) == CLM_UA_OPERATION_FAILURE,
		"classify bots/devices and reject invalid output pointers");
	CLM_ASSERT(clm_string_delete(source_id) && clm_string_delete(browser_id) &&
		clm_string_delete(browser_version_id) && clm_string_delete(os_id) &&
		clm_string_delete(os_version_id),
		"release User-Agent strings");

	clm_test_summary(result.passed, result.failed);
	return result.failed == 0 ? 0 : 1;
}

static int test_clm_ua(void)
{
    return clm_ua_tests();
}

