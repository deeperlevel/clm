# frozen_string_literal: true

require 'minitest/autorun'
require_relative 'tests/test'

abort 'Unable to start CLM test reporting.' unless CLMTest.start('CLM Tests Summary')

require_relative 'tests/test_clm_base'
require_relative 'tests/test_clm_console'
require_relative 'tests/test_clm_bpa'
require_relative 'tests/test_clm_dictionary'
require_relative 'tests/test_clm_id'
require_relative 'tests/test_clm_string'
require_relative 'tests/test_clm_storage'
