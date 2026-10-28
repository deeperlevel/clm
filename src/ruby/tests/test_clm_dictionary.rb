# frozen_string_literal: true

require 'minitest'
require_relative '../clm/dictionary'

class ClmDictionaryTest < CLMTestCase
  CLM_TEST_TITLE = 'CLM - Dictionary'

  def teardown
    CLM::BPA.shutdown
  end

  def test_integer_values_and_bpa_block_transfers
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.new(71_000, CLM::BPA::TYPE_INT)
    assert_equal 0, CLM::Dictionary.count(71_000)
    assert_equal CLM::BPA::FAILURE, CLM::Dictionary.new(71_000, CLM::BPA::TYPE_INT)
    assert_equal CLM::BPA::FAILURE, CLM::Dictionary.new(71_003, CLM::BPA::TYPE_STRING, 1)

    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set_from(71_000, 10, 123)
    assert_equal [CLM::BPA::GET_SUCCESS, 123], CLM::Dictionary.get_to(71_000, 10)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set_from(71_000, 10, 9876)
    assert_equal [CLM::BPA::GET_SUCCESS, 9876], CLM::Dictionary.get_to(71_000, 10)
    assert_equal 1, CLM::Dictionary.count(71_000)

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(71_001, CLM::BPA::TYPE_INT)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.set(71_001, 456)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set(71_000, 0, 71_001)
    assert_equal [CLM::BPA::GET_SUCCESS, 456], CLM::Dictionary.get_to(71_000, 0)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(71_002, CLM::BPA::TYPE_INT)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::Dictionary.get(71_000, 0, 71_002)
    assert_equal [CLM::BPA::GET_SUCCESS, 456], CLM::BPA.get(71_002)
    assert_equal CLM::BPA::FAILURE, CLM::Dictionary.get(71_000, 0, 71_001 + 10)
  end

  def test_growth_removal_and_ordered_iteration
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.new(71_100, CLM::BPA::TYPE_INT)
    40.times do |index|
      assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set_from(71_100, 1000 + index, index * 10)
    end
    assert_equal 40, CLM::Dictionary.count(71_100)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set_from(71_100, 1005, 9876)
    assert_equal 40, CLM::Dictionary.count(71_100)
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.remove(71_100, 1005)
    refute CLM::Dictionary.contains?(71_100, 1005)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set_from(71_100, 99_999, 12_345)

    status, keys = CLM::Dictionary.keys(71_100)
    assert_equal CLM::BPA::GET_SUCCESS, status
    assert_equal [*1000..1004, *1006..1039, 99_999], keys
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.iterate_reset(71_100)
    iterated = []
    loop do
      status, key = CLM::Dictionary.iterate_next(71_100)
      break if status == CLM::BPA::FAILURE
      iterated << key
    end
    assert_equal keys, iterated
    assert_equal CLM::BPA::FAILURE, CLM::Dictionary.iterate_next(71_100).first
    assert_equal CLM::BPA::FAILURE, CLM::Dictionary.get_to(71_100, 1005).first
  end

  def test_add_update_clear_and_reinsert_order
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.new(71_200, CLM::BPA::TYPE_INT)
    ordered_keys = [900, 2, 77, 0]
    ordered_keys.each_with_index do |key, index|
      assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.add_from(71_200, key, index + 100)
    end
    assert_equal CLM::BPA::FAILURE, CLM::Dictionary.add_from(71_200, 2, 5)
    assert_equal CLM::BPA::FAILURE, CLM::Dictionary.update_from(71_200, 333, 5)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.update_from(71_200, 2, 999)
    assert_equal [CLM::BPA::GET_SUCCESS, ordered_keys], CLM::Dictionary.keys(71_200)
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.remove(71_200, 2)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.add_from(71_200, 2, 777)
    assert_equal [CLM::BPA::GET_SUCCESS, [900, 77, 0, 2]], CLM::Dictionary.keys(71_200)
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.clear(71_200)
    assert_equal 0, CLM::Dictionary.count(71_200)
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.delete(71_200)
    assert_equal CLM::BPA::FAILURE, CLM::Dictionary.count(71_200)
  end

  def test_string_and_dynamic_struct_values
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(71_301, CLM::BPA::TYPE_STRING)
    assert_equal 16, CLM::BPA.set_string(71_301, 'dictionary value')
    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.new(71_310, CLM::BPA::TYPE_STRING)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set(71_310, 5, 71_301)
    assert_equal [CLM::BPA::GET_SUCCESS, 'dictionary value'.b], CLM::Dictionary.get_to(71_310, 5)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set_from(71_310, 6, "\0", 1)
    assert_equal [CLM::BPA::GET_SUCCESS, ''.b], CLM::Dictionary.get_to(71_310, 6)

    assert_equal CLM::BPA::SUCCESS, CLM::Dictionary.new(71_320, CLM::BPA::TYPE_STRUCT, 1)
    value = { pair: [11, 22] }
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Dictionary.set_from(71_320, 7, value)
    value[:pair][0] = 99
    assert_equal [CLM::BPA::GET_SUCCESS, { pair: [11, 22] }], CLM::Dictionary.get_to(71_320, 7)
  end
end