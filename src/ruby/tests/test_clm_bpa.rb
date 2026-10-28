# frozen_string_literal: true

require 'minitest'
require_relative '../clm/bpa'

class ClmBpaTest < CLMTestCase
  CLM_TEST_TITLE = 'CLM - Block Pool Allocation (BPA)'

  def teardown
    CLM::BPA.shutdown
  end

  def test_block_creation_values_and_references
    assert_equal CLM::BPA::FAILURE, CLM::BPA.get(1).first
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(1, CLM::BPA::TYPE_INT)
    assert CLM::BPA.block_existing?(1)
    assert_equal [CLM::BPA::GET_SUCCESS, 0], CLM::BPA.get(1)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.create(1, CLM::BPA::TYPE_INT)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.set(1, 4)
    assert_equal [CLM::BPA::GET_SUCCESS, 4], CLM::BPA.get(1)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.set(1, 1 << 40)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.create(2, CLM::BPA::TYPE_STRUCT)

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.ref(1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.unref(1)
    assert CLM::BPA.block_existing?(1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.unref(1)
    refute CLM::BPA.block_existing?(1)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.unref(1)
  end

  def test_custom_struct_blocks_and_copy
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_sized(2, CLM::BPA::TYPE_STRUCT, 8)
    value = { name: 'CLM', version: 1 }
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.set(2, value)
    value[:version] = 2
    assert_equal [CLM::BPA::GET_SUCCESS, { name: 'CLM', version: 1 }], CLM::BPA.get(2)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_sized(3, CLM::BPA::TYPE_STRUCT, 8)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.copy_block(2, 3)
    assert_equal [CLM::BPA::GET_SUCCESS, { name: 'CLM', version: 1 }], CLM::BPA.get(3)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.create_sized(4, CLM::BPA::TYPE_STRUCT, 0)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.set(2, 'short')
    assert_equal [CLM::BPA::GET_SUCCESS, 'short'], CLM::BPA.get(2)
  end

  def test_string_blocks_and_bounded_reads
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(10, CLM::BPA::TYPE_STRING)
    assert_equal 5, CLM::BPA.set_string(10, "hello\0ignored")
    assert_equal [5, 'hello'.b], CLM::BPA.get_string(10)
    assert_equal [CLM::BPA::FAILURE, nil], CLM::BPA.get_string(10, 5)
    assert_equal 6, CLM::BPA.replace(10, 'world!')
    assert_equal [6, 'world!'.b], CLM::BPA.get_string(10)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.replace(999, 'missing')
    assert_equal CLM::BPA::FAILURE, CLM::BPA.get(10).first
  end

  def test_fixed_array_mutations_capacity_cursor_and_sort
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array(20, CLM::BPA::TYPE_INT, 1)
    [4, 1, 3, 2].each { |value| assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_append(20, value) }
    assert_equal [CLM::BPA::GET_SUCCESS, 4], CLM::BPA.array_count(20)
    status, capacity = CLM::BPA.array_capacity(20)
    assert_equal CLM::BPA::GET_SUCCESS, status
    assert_operator capacity, :>=, 4
    assert_equal [CLM::BPA::GET_SUCCESS, 4], CLM::BPA.array_get_first(20)
    assert_equal [CLM::BPA::GET_SUCCESS, 2], CLM::BPA.array_get_last(20)
    assert_equal [CLM::BPA::FAILURE, nil], CLM::BPA.array_get(20, -1)

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_insert(20, 1, 8)
    assert_equal [CLM::BPA::GET_SUCCESS, 8], CLM::BPA.array_get(20, 1)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.array_set(20, 1, 7)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_move_to(20, 2)
    assert_equal [CLM::BPA::GET_SUCCESS, 2], CLM::BPA.array_current_index(20)
    assert_equal [CLM::BPA::GET_SUCCESS, 1], CLM::BPA.array_get_current(20)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_move_previous(20)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_move_next(20)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_move_last(20)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.array_move_next(20)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_reset(20)

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_extend(20, 2)
    _, extended_capacity = CLM::BPA.array_capacity(20)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_shrink(20, extended_capacity - 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_reverse(20)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_sort_with(20) { |left, right| left <=> right }
    _, values = CLM::BPA.array_slice_to(20, 0, 5)
    assert_equal values.sort, values
    assert_equal CLM::BPA::FAILURE, CLM::BPA.array_sort(20)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_shift_left(20, 1, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_shift_right(20, 1, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_swap(20, 0, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_copy(20, 0, 1)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.array_swap(20, -1, 0)
  end

  def test_sealing_draw_decks_and_array_references
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array(21, CLM::BPA::TYPE_INT, 2)
    [1, 2].each { |value| assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_append(21, value) }
    assert_equal [CLM::BPA::GET_SUCCESS, false], CLM::BPA.array_sealed?(21)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_seal(21)
    assert_equal [CLM::BPA::GET_SUCCESS, true], CLM::BPA.array_sealed?(21)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.array_append(21, 3)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.array_set(21, 0, 3)
    assert_equal [CLM::BPA::GET_SUCCESS, 3], CLM::BPA.array_draw_at(21, 0)
    assert_equal [CLM::BPA::GET_SUCCESS, 2], CLM::BPA.array_draw(21)
    assert_equal [CLM::BPA::GET_SUCCESS, 2], CLM::BPA.array_count(21)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_clear(21)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_append(21, 9)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_unseal(21)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_extend(21, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_remove_first(21)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.array_draw(21).first

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_ref(21)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_unref(21)
    assert CLM::BPA.array_existing?(21)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_unref(21)
    refute CLM::BPA.array_existing?(21)
  end

  def test_string_arrays_and_block_transfers
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(30, CLM::BPA::TYPE_STRING)
    assert_equal 5, CLM::BPA.set_string(30, 'hello')
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array(31, CLM::BPA::TYPE_STRING, 0, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_append_string_from_block(31, 30)
    assert_equal [5, 'hello'.b], CLM::BPA.array_get_string(31, 0)
    assert_equal 6, CLM::BPA.set_string(30, 'world!')
    assert_equal [5, 'hello'.b], CLM::BPA.array_get_string(31, 0)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.array_set_string_from_block(31, 0, 30)
    assert_equal [6, 'world!'.b], CLM::BPA.array_get_string(31, 0)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_insert_string(31, 1, '')
    assert_equal [CLM::BPA::GET_SUCCESS, 0], CLM::BPA.array_length(31)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_remove(31, 1)
    assert_equal [6, 'world!'.b], CLM::BPA.array_get_string(31, 0)

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(32, CLM::BPA::TYPE_INT)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.set(32, 777)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array(33, CLM::BPA::TYPE_INT, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_append_from_block(33, 32)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::BPA.array_get_to_block(33, 0, 32)
    assert_equal [CLM::BPA::GET_SUCCESS, 777], CLM::BPA.get(32)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.array_set_from_block(33, 0, 32)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_insert_from_block(33, 0, 32)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::BPA.array_draw_to_block(33, 32)

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array_sized(34, CLM::BPA::TYPE_STRUCT, 0, 4)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_append_from_block(34, 32, 4)
    assert_equal [CLM::BPA::GET_SUCCESS, 777], CLM::BPA.array_get(34, 0)
  end

  def test_slice_copy_borrow_and_independent_strings
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array(40, CLM::BPA::TYPE_INT, 0)
    [4, 1, 3, 2].each { |value| CLM::BPA.array_append(40, value) }
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_slice(40, 1, 2, 41)
    assert_equal [CLM::BPA::GET_SUCCESS, 2], CLM::BPA.array_count(41)
    assert_equal [CLM::BPA::GET_SUCCESS, 1], CLM::BPA.array_get(41, 0)
    assert_equal [CLM::BPA::SUCCESS, [1, 3]], CLM::BPA.array_slice_to(40, 1, 2)
    status, view = CLM::BPA.array_slice_borrow(40, 1, 2)
    assert_equal CLM::BPA::SUCCESS, status
    assert_equal [1, 3], view.to_a
    assert_equal [CLM::BPA::FAILURE, nil], CLM::BPA.array_slice_to(40, -1, 1)

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(42, CLM::BPA::TYPE_STRING)
    CLM::BPA.set_string(42, 'hello')
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array(43, CLM::BPA::TYPE_STRING)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_append_string_from_block(43, 42)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_slice(43, 0, 1, 44)
    CLM::BPA.array_set_string(43, 0, 'changed')
    assert_equal [5, 'hello'.b], CLM::BPA.array_get_string(44, 0)
    assert_equal [CLM::BPA::FAILURE, nil], CLM::BPA.array_slice_borrow(43, 0, 1)
  end

  def test_advanced_borrow_and_large_ids
    id = 0x1_0000_0001
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array(id, CLM::BPA::TYPE_INT, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_append(id, 17)
    status, borrow = CLM::BPA.advanced_borrow(id)
    assert_equal CLM::BPA::SUCCESS, status
    assert_equal [17], CLM::BPA.advanced_data(borrow)
    CLM::BPA.advanced_data(borrow)[0] = 3
    assert_equal [CLM::BPA::GET_SUCCESS, 3], CLM::BPA.advanced_get(borrow, 0)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.advanced_set(borrow, 0, 4)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.advanced_extend(borrow, 2)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.advanced_append(borrow, 6)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.advanced_insert(borrow, 1, 2)
    assert_equal [4, 2, 6], CLM::BPA.advanced_data(borrow)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.advanced_remove(borrow, 0)
    _, capacity = CLM::BPA.array_capacity(id)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.advanced_shrink(borrow, capacity - 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.advanced_clear(borrow)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.array_unref(id)
    assert CLM::BPA.array_existing?(id)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.advanced_release(borrow)
    refute CLM::BPA.array_existing?(id)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.advanced_release(borrow)

    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create_array(50, CLM::BPA::TYPE_STRING)
    status, string_borrow = CLM::BPA.advanced_borrow(50)
    assert_equal CLM::BPA::SUCCESS, status
    assert_nil CLM::BPA.advanced_data(string_borrow)
    assert_equal CLM::BPA::FAILURE, CLM::BPA.advanced_get(string_borrow, 0).first
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.advanced_release(string_borrow)
  end
end