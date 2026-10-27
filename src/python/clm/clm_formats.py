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

"""Compatibility facade for the former clm_formats module.
New code should import clm_format or a format-specific codec directly.
"""
from clm_format import *
from clm_json import clm_json_parse, clm_json_serialize
from clm_yaml import clm_yaml_parse, clm_yaml_serialize
from clm_ini import clm_ini_parse, clm_ini_serialize
from clm_toml import clm_toml_parse, clm_toml_serialize
from clm_csv import clm_csv_parse, clm_csv_serialize
