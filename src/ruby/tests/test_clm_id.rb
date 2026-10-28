# frozen_string_literal: true

require 'minitest'
require_relative '../clm/id'

class ClmIdTest < CLMTestCase
  CLM_TEST_TITLE = 'CLM - ID'

  def setup
    CLM::ID.shutdown
    CLM::ID.set_case_sensitive(true)
    CLM::ID.clear
  end

  def teardown
    CLM::ID.shutdown
    CLM::ID.set_case_sensitive(true)
    CLM::ID.shutdown
  end

  def test_register_map_lookup_and_duplicate_names
    assert CLM::ID.case_sensitive?
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('host', 1001)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.map('mapped-name', 1002)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('default_host', 1001)
    assert_equal 1001, CLM::ID.get('host')
    assert_equal 1001, CLM::ID.get('default_host')
    assert_equal CLM::ID::NOT_FOUND, CLM::ID.get('HOST')
    assert_equal CLM::BPA::FAILURE, CLM::ID.register('host', 1002)
    assert_equal CLM::ID::NOT_FOUND, CLM::ID.get("\xC0\xAF".b.force_encoding(Encoding::UTF_8))
    assert_equal [CLM::BPA::GET_SUCCESS, 1001], CLM::ID.try_get('host')
    assert_equal [CLM::BPA::FAILURE, nil], CLM::ID.try_get('missing')
  end

  def test_update_remove_aliases_and_case_mode_collisions
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('host', 1001)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('default_host', 1001)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.set('host', 2001)
    assert_equal 2001, CLM::ID.get('host')
    assert_equal 1001, CLM::ID.get('default_host')
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.set('new_name', 3001)
    assert_equal CLM::BPA::SUCCESS, CLM::ID.remove('new_name')
    assert_equal CLM::ID::NOT_FOUND, CLM::ID.get('new_name')
    assert_equal CLM::BPA::FAILURE, CLM::ID.remove('missing')

    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('Host', 4001)
    assert_equal CLM::BPA::FAILURE, CLM::ID.set_case_sensitive(false)
    assert CLM::ID.case_sensitive?
    assert_equal 4001, CLM::ID.get('Host')
    assert_equal 2001, CLM::ID.get('host')
    assert_equal CLM::BPA::SUCCESS, CLM::ID.remove('Host')
    assert_equal CLM::BPA::SUCCESS, CLM::ID.set_case_sensitive(false)
    refute CLM::ID.case_sensitive?
    assert_equal 2001, CLM::ID.get('HOST')
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.set('HOST', 2002)
    assert_equal 2002, CLM::ID.get('host')
  end

  def test_non_ascii_names_remain_case_sensitive_and_alias_unmap
    upper = "\xC3\x84".b.force_encoding(Encoding::UTF_8)
    lower = "\xC3\xA4".b.force_encoding(Encoding::UTF_8)

    assert_equal CLM::BPA::SUCCESS, CLM::ID.set_case_sensitive(false)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register(upper, 6001)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register(lower, 6002)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('default_host', 1001)
    assert_equal 6001, CLM::ID.get(upper)
    assert_equal 6002, CLM::ID.get(lower)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('alias', 2002)
    assert_equal CLM::BPA::SUCCESS, CLM::ID.unmap(2002)
    assert_equal CLM::ID::NOT_FOUND, CLM::ID.get('host')
    assert_equal CLM::ID::NOT_FOUND, CLM::ID.get('alias')
    assert_equal CLM::BPA::SUCCESS, CLM::ID.unmap_name('default_host')
    assert_equal CLM::ID::NOT_FOUND, CLM::ID.get('default_host')
  end

  def test_generator_clear_shutdown_and_lazy_recreation
    assert_equal [0, 1, 2], [CLM::ID.gen_next, CLM::ID.gen_next, CLM::ID.gen_next]
    CLM::ID.gen_reset
    assert_equal 0, CLM::ID.gen_next

    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('before-clear', 5001)
    assert_equal CLM::BPA::SUCCESS, CLM::ID.set_case_sensitive(false)
    assert_equal CLM::BPA::SUCCESS, CLM::ID.clear
    assert_equal CLM::ID::NOT_FOUND, CLM::ID.get('before-clear')
    refute CLM::ID.case_sensitive?
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('after-clear', 5002)
    assert_equal 5002, CLM::ID.get('AFTER-CLEAR')

    assert_equal CLM::BPA::SUCCESS, CLM::ID.shutdown
    assert_equal CLM::ID::NOT_FOUND, CLM::ID.get('after-clear')
    assert_equal CLM::BPA::SET_SUCCESS, CLM::ID.register('after-shutdown', 5003)
    assert_equal 5003, CLM::ID.get('after-shutdown')
    assert_equal CLM::BPA::SUCCESS, CLM::ID.shutdown
  end
end