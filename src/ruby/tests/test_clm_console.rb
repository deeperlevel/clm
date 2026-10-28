# frozen_string_literal: true

require 'minitest'
require 'stringio'
require_relative '../clm/base'
require_relative '../clm/console'

class ClmConsoleTest < CLMTestCase
  CLM_TEST_TITLE = 'CLM - Console'

  def test_initialization_and_standard_output_operations
    assert_equal CLM::Console::SUCCESS, CLM::Console.init
    assert_equal CLM::Console::SUCCESS, CLM::Console.restore

    capture_stdout do |output|
      assert_equal CLM::Console::SUCCESS, CLM::Console.flush
      assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write(nil)
      assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write_line(nil)
      assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write_character(nil)
      assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write_effect(nil, 0)
      assert_equal CLM::Console::SUCCESS, CLM::Console.write('text')
      assert_equal CLM::Console::SUCCESS, CLM::Console.write_line('line')
      assert_equal CLM::Console::SUCCESS, CLM::Console.write_format(' %d %s', 42, 'ok')
      assert_equal CLM::Console::SUCCESS, CLM::Console.write_line_format('line:%02d', 7)
      assert_equal "textline\n 42 okline:07\n", output.string
    end

    assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.wait(-1)
    assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.wait(Float::NAN)
    assert_equal CLM::Console::SUCCESS, CLM::Console.wait(0)
  end

  def test_stream_output_formatting_and_ansi_controls
    output = StringIO.new

    assert_equal CLM::Console::SUCCESS, CLM::Console.clear_to(output)
    assert_equal CLM::Console::SUCCESS, CLM::Console.set_cursor_position_to(output, CLM::Base::Position.new(x: 2, y: 4))
    assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.set_cursor_position_to(output, CLM::Base::Position.new(x: -1, y: 0))
    assert_equal CLM::Console::SUCCESS, CLM::Console.set_cursor_visible_to(output, false)
    assert_equal CLM::Console::SUCCESS, CLM::Console.set_cursor_visible_to(output, true)
    assert_equal CLM::Console::SUCCESS, CLM::Console.write_to(output, 'text')
    assert_equal CLM::Console::SUCCESS, CLM::Console.write_line_to(output, 'line')
    assert_equal CLM::Console::SUCCESS, CLM::Console.write_format_to(output, ' value=%d %s', 42, 'ok')
    assert_equal CLM::Console::SUCCESS, CLM::Console.write_line_format_to(output, 'line:%02d', 7)
    assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write_format_to(output, nil)
    assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write_line_format_to(nil, '%d', 1)
    assert_equal CLM::Console::SUCCESS, CLM::Console.write_color_to(output, 'red', CLM::Console::CONSOLE_COLOR_RED)
    assert_equal CLM::Console::SUCCESS, CLM::Console.write_effect_to(output, 'go', 0)

    expected = "\e[2J\e[H\e[5;3H\e[?25l\e[?25h" \
               "textline\n value=42 okline:07\n" \
               "\e[31mred\e[0mgo"
    assert_equal expected, output.string
  end

  def test_line_reading_and_overlong_line_recovery
    input = StringIO.new("hello\r\nlast")

    assert_equal [CLM::Console::SUCCESS, 'hello'.b], CLM::Console.read_line_from(input)
    assert_equal [CLM::Console::SUCCESS, 'last'.b], CLM::Console.read_line_from(input)
    assert_equal [CLM::Console::FAILURE, nil], CLM::Console.read_line_from(input)
    assert_equal [CLM::Console::INVALID_ARGUMENT, nil], CLM::Console.read_line_from(StringIO.new, 1)

    input = StringIO.new("123456789\nok\n")
    assert_equal [CLM::Console::FAILURE, nil], CLM::Console.read_line_from(input, 8)
    assert_equal [CLM::Console::SUCCESS, 'ok'.b], CLM::Console.read_line_from(input, 8)
  end

  def test_character_reading_and_utf8_character_writing
    assert_equal [CLM::Console::SUCCESS, 'Q'.b], CLM::Console.read_character_from(StringIO.new('Q'))
    assert_equal [CLM::Console::FAILURE, nil], CLM::Console.read_character_from(StringIO.new(''))

    output = StringIO.new
    assert_equal CLM::Console::SUCCESS, CLM::Console.write_character_to(output, 'X')
    assert_equal CLM::Console::SUCCESS, CLM::Console.write_character_to(output, 'é')
    assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write_character_to(output, '')
    assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write_character_to(output, 'XY')
    assert_equal CLM::Console::INVALID_ARGUMENT, CLM::Console.write_character_to(output, "\0")
    assert_equal "Xé", output.string.force_encoding(Encoding::UTF_8)
  end

  def test_confirmation_responses
    output = StringIO.new
    assert_equal [CLM::Console::SUCCESS, true], CLM::Console.confirm_from(StringIO.new(" yes \n"), output, 'Continue?')
    assert_equal "Continue? [y/n]: ", output.string
    assert_equal [CLM::Console::SUCCESS, false], CLM::Console.confirm_from(StringIO.new("no\n"), StringIO.new)
    assert_equal [CLM::Console::INVALID_ARGUMENT, nil], CLM::Console.confirm_from(StringIO.new("yesterday\n"), StringIO.new)
  end

  private

  def capture_stdout
    original_stdout = $stdout
    output = StringIO.new
    $stdout = output
    yield output
  ensure
    $stdout = original_stdout
  end
end