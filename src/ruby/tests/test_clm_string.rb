# frozen_string_literal: true

require 'minitest'
require_relative '../clm/string'

class ClmStringTest < CLMTestCase
  CLM_TEST_TITLE = 'CLM - String'

  def setup
    CLM::BPA.shutdown
    CLM::String.set_default_encoding(CLM::String::ENCODING_UTF8)
  end

  def teardown
    CLM::BPA.shutdown
    CLM::String.set_default_encoding(CLM::String::ENCODING_UTF8)
  end

  def test_unicode_codepoint_edits_and_validation
    text = "Aé🐱"
    assert CLM::String.new(90_001, text)
    assert_equal 3, CLM::String.length(90_001)
    assert_equal [CLM::BPA::GET_SUCCESS, CLM::String::ENCODING_UTF8], CLM::BPA.block_encoding(90_001)
    assert_equal 1, CLM::String.append(90_001, '!')
    assert_equal 1, CLM::String.insert(90_001, 1, '中')
    assert_equal 'A中é🐱!', string_value(90_001)
    assert_equal 1, CLM::String.remove(90_001, 1, 1)
    assert_equal 'Aé🐱!', string_value(90_001)
    assert_equal 1, CLM::String.replace(90_001, 'é', '中')
    assert_equal 'A中🐱!', string_value(90_001)
    assert_equal 1, CLM::String.replace(90_001, '!', '')
    assert CLM::String.contains?(90_001, '🐱')
    assert_equal 2, CLM::String.append_line(90_001, '!')
    assert CLM::String.contains?(90_001, "\n")
    assert_operator CLM::String.length(90_001), :>, 0
    assert_equal CLM::BPA::FAILURE, CLM::String.remove(90_001, -1, 1)
    assert_equal CLM::BPA::FAILURE, CLM::String.insert(90_001, 99, 'x')
    assert_equal CLM::BPA::FAILURE, CLM::String.replace(90_001, '', 'x')
    invalid_utf8 = "\xC0\xAF".b.force_encoding(Encoding::UTF_8)
    refute CLM::String.new(90_008, invalid_utf8)
    refute CLM::BPA.block_existing?(90_008)
  end

  def test_external_encodings_and_safe_byte_queries
    latin_text = "café"
    latin_bytes = latin_text.encode(Encoding::ISO_8859_1).b
    expected_utf16 = "Aé🐱".encode(Encoding::UTF_16LE).b
    malformed_utf16 = "\x00\xD8".b
    embedded_nul = "A\0B".b

    assert_equal CLM::String::ENCODING_UTF8, CLM::String.get_default_encoding
    assert_equal CLM::BPA::SUCCESS, CLM::String.set_default_encoding(CLM::String::ENCODING_LATIN1)
    assert CLM::String.new(91_100, latin_text)
    assert_equal [CLM::BPA::GET_SUCCESS, CLM::String::ENCODING_LATIN1], CLM::BPA.block_encoding(91_100)
    assert_equal [4, latin_bytes], CLM::String.get_bytes(91_100)
    assert_equal [CLM::BPA::FAILURE, nil], CLM::String.get_bytes(91_100, 3)
    assert_equal [CLM::BPA::SUCCESS, latin_text.encode(Encoding::UTF_8).b],
                 CLM::String.from_bytes(91_101, CLM::String::ENCODING_LATIN1, latin_bytes).then { |status| [status, string_value(91_101)&.b] }

    assert_equal CLM::BPA::FAILURE, CLM::String.from_bytes(91_101, CLM::String::ENCODING_UTF8, invalid_utf8_bytes)
    assert_equal latin_text, string_value(91_101)
    assert_equal CLM::BPA::FAILURE, CLM::String.from_bytes(91_102, CLM::String::ENCODING_UTF16LE, malformed_utf16)
    refute CLM::BPA.block_existing?(91_102)
    assert_equal CLM::BPA::FAILURE, CLM::String.from_bytes(91_102, CLM::String::ENCODING_UTF8, embedded_nul)
    assert_equal CLM::BPA::FAILURE, CLM::String.from_bytes(91_102, CLM::String::ENCODING_UTF16LE, "A".b)
    assert_equal CLM::BPA::FAILURE, CLM::String.set_default_encoding(0xFF)
    assert_equal CLM::String::ENCODING_LATIN1, CLM::String.get_default_encoding

    assert CLM::String.new(91_103, "Aé🐱")
    assert_equal CLM::BPA::SUCCESS, CLM::String.change_encoding(91_103, CLM::String::ENCODING_UTF16LE)
    assert_equal [expected_utf16.bytesize, expected_utf16], CLM::String.get_bytes(91_103)
    assert_equal CLM::BPA::SUCCESS, CLM::String.from_bytes(91_104, CLM::String::ENCODING_UTF16LE, expected_utf16)
    assert_equal "Aé🐱", string_value(91_104)
    assert_equal CLM::BPA::FAILURE, CLM::String.change_encoding(91_103, CLM::String::ENCODING_LATIN1)
    assert_equal [CLM::BPA::GET_SUCCESS, CLM::String::ENCODING_UTF16LE], CLM::BPA.block_encoding(91_103)
  end

  def test_copy_and_borrowed_slices
    assert CLM::String.new(92_000, "Aé🐱")
    assert_equal [CLM::BPA::SUCCESS, "é🐱"], CLM::String.slice_borrow(92_000, 1, 2)
    assert_equal [CLM::BPA::SUCCESS, ''], CLM::String.slice_borrow(92_000, 3, 0)
    assert_equal CLM::BPA::FAILURE, CLM::String.slice_borrow(92_000, 4, 0).first
    assert_equal CLM::BPA::SUCCESS, CLM::String.slice(92_000, 1, 2, 92_001)
    assert_equal 'é🐱', string_value(92_001)
    assert_equal [CLM::BPA::GET_SUCCESS, CLM::String::ENCODING_UTF8], CLM::BPA.block_encoding(92_001)
    assert_equal CLM::BPA::FAILURE, CLM::String.slice(92_000, 4, 0, 92_002)
    assert_equal CLM::BPA::FAILURE, CLM::String.slice(92_000, -1, 1, 92_002)
    assert_equal CLM::BPA::FAILURE, CLM::String.slice(92_000, 0, 1, 92_001)

    assert_equal 7, CLM::String.set(92_000, 'changed')
    assert_equal 'é🐱', string_value(92_001)
  end

  def test_bpa_backed_string_array_operations
    assert CLM::String.new(93_004, 'pear')
    assert CLM::String.new(93_005, 'apple')
    assert CLM::String.new(93_006, 'banana')
    assert CLM::String.new(93_007, '')
    assert CLM::String.array_new(93_003)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_append(93_003, 93_004)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_append(93_003, 93_005)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_insert(93_003, 1, 93_006)
    assert_equal 3, CLM::String.array_count(93_003)
    assert_equal 1, CLM::String.array_find(93_003, 93_006)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::String.array_get(93_003, 1, 93_007)
    assert_equal 'banana', string_value(93_007)
    assert_equal 6, CLM::String.array_length_at(93_003, 1)
    assert CLM::String.array_contains_at(93_003, 1, 'ana')
    assert_equal 1, CLM::String.array_append_at(93_003, 1, '!')
    assert_equal 2, CLM::String.array_append_line_at(93_003, 1, '.')
    assert_equal 1, CLM::String.array_insert_at(93_003, 1, 0, '!')
    assert_equal 1, CLM::String.array_remove_at(93_003, 1, 0, 1)
    assert_equal 3, CLM::String.array_replace_at(93_003, 1, 'a', 'A')
    assert_equal CLM::BPA::GET_SUCCESS, CLM::String.array_get(93_003, 1, 93_007)
    assert_equal "bAnAnA!.\n", string_value(93_007)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::String.array_set(93_003, 0, 93_005)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::String.array_set(93_003, 1, 93_006)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_sort(93_003)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_move_last(93_003)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::String.array_get_current(93_003, 93_007)
    assert_equal 'banana', string_value(93_007)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_move_first(93_003)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_move_next(93_003)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_move_previous(93_003)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_move_to(93_003, 2)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_remove(93_003, 1)
    assert_equal 2, CLM::String.array_count(93_003)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_shuffle(93_003)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::String.array_draw_at(93_003, 0, 93_007)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::String.array_draw(93_003, 93_007)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::String.array_draw(93_003, 93_007)
    assert_equal CLM::BPA::FAILURE, CLM::String.array_draw_at(93_003, 99, 93_007)
    assert_equal CLM::BPA::SUCCESS, CLM::String.array_clear(93_003)
    assert_equal 0, CLM::String.array_count(93_003)
    assert CLM::String.array_delete(93_003)
  end

  private

  def string_value(id)
    length, value = CLM::BPA.get_string(id)
    length.negative? ? nil : value.force_encoding(Encoding::UTF_8)
  end

  def invalid_utf8_bytes
    "\xC0\xAF".b
  end
end