# frozen_string_literal: true

require 'minitest'
require_relative '../clm/console'

module CLMTest
  DIVIDER_SINGLE = '-' * 61
  DIVIDER_DOUBLE = '=' * 61

  @log_file = nil
  @log_failed = false
  @output_failed = false
  @passed = 0
  @failed = 0
  @skipped = 0
  @total_passed = 0
  @total_failed = 0
  @total_skipped = 0
  @current_suite = nil

  class << self
    def start(title, log_path = 'main.log')
      @log_file = File.open(log_path, 'w:UTF-8')
      begin_test(title)
      Minitest.after_run { exit 1 unless finish }
      true
    rescue SystemCallError
      @log_failed = true
      CLM::Console.write_line("Unable to open test log: #{log_path}")
      false
    end

    def write(text)
      @output_failed ||= CLM::Console.write(text) != CLM::Console::SUCCESS
      log_text(text)
    end

    def write_line(text)
      @output_failed ||= CLM::Console.write_line(text) != CLM::Console::SUCCESS
      log_text("#{text}\n", flush: true)
    end

    def write_color(text, color)
      @output_failed ||= CLM::Console.write_color(text, color) != CLM::Console::SUCCESS
      log_text(text)
    end

    def write_format(format_string, *arguments)
      formatted = format_string % arguments
      @output_failed ||= CLM::Console.write_format(format_string, *arguments) != CLM::Console::SUCCESS
      log_text(formatted)
    rescue ArgumentError, TypeError
      @output_failed = true
      @log_failed = true
    end

    def write_line_format(format_string, *arguments)
      formatted = format_string % arguments
      @output_failed ||= CLM::Console.write_line_format(format_string, *arguments) != CLM::Console::SUCCESS
      log_text("#{formatted}\n", flush: true)
    rescue ArgumentError, TypeError
      @output_failed = true
      @log_failed = true
    end

    def begin_test(title)
      write_line('')
      write_line(DIVIDER_DOUBLE)
      write_color(title, CLM::Console::CONSOLE_COLOR_YELLOW)
      write_line('')
      write_line(DIVIDER_DOUBLE)
    end

    def assert(condition, description)
      if condition
        @passed += 1
        write("    ")
        write_color('PASSED:', CLM::Console::CONSOLE_COLOR_GREEN)
        write(' ')
        write_line(description)
      else
        @failed += 1
        write("    ")
        write_color('FAILED:', CLM::Console::CONSOLE_COLOR_RED)
        write(' ')
        write_line(description)
      end
      !!condition
    end

    def record_result(result)
      unless @current_suite == result.class
        summary unless @current_suite.nil?
        @current_suite = result.class
        begin_test(result.class::CLM_TEST_TITLE)
      end

      description = "#{result.class.name}##{result.name}"
      if result.skipped?
        @skipped += 1
        write("\n    ")
        write_color('SKIPPED:', CLM::Console::CONSOLE_COLOR_YELLOW)
        write(' ')
        write_line(description)
      else
        assert(result.passed?, description)
      end
    end

    def summary
      @total_passed += @passed
      @total_failed += @failed
      @total_skipped += @skipped

      write_line(DIVIDER_SINGLE)
      write("    ")
      write_color('PASSED:', CLM::Console::CONSOLE_COLOR_GREEN)
      write_format(" %d  |  ", @passed)
      write_color('FAILED:', CLM::Console::CONSOLE_COLOR_RED)
      write_format(" %d  |  ", @failed)
      write_color('TOTAL:', CLM::Console::CONSOLE_COLOR_CYAN)
      write_line_format(" %d", @passed + @failed + @skipped)
      write_line_format("    SKIPPED: %d", @skipped) if @skipped.positive?

      @passed = 0
      @failed = 0
      @skipped = 0
    end

    def overall_summary(title)
      write_line('')
      write_line(DIVIDER_DOUBLE)
      write_color(title, CLM::Console::CONSOLE_COLOR_YELLOW)
      write_line('')
      write_line(DIVIDER_DOUBLE)
      write('  ')
      write_color('PASSED:', CLM::Console::CONSOLE_COLOR_GREEN)
      write_format(" %d  |  ", @total_passed)
      write_color('FAILED:', CLM::Console::CONSOLE_COLOR_RED)
      write_format(" %d  |  ", @total_failed)
      write_color('TOTAL:', CLM::Console::CONSOLE_COLOR_CYAN)
      write_line_format(" %d", @total_passed + @total_failed + @total_skipped)
      write_line_format("  SKIPPED: %d", @total_skipped) if @total_skipped.positive?
      write_line(DIVIDER_DOUBLE)
      write_line('')
    end

    def finish
      summary
      overall_summary('CLM - Overall Test Summary')
      close_log && !@output_failed
    end

    def close_log
      return false unless @log_file

      @log_file.flush
      @log_file.close
      @log_file = nil
      !@log_failed
    rescue IOError, SystemCallError
      @log_failed = true
      @log_file = nil
      false
    end

    private

    def log_text(text, flush: false)
      return unless @log_file

      @log_file.write(text)
      @log_file.flush if flush
    rescue IOError, SystemCallError, EncodingError, TypeError
      @log_failed = true
    end
  end
end

class CLMTestCase < Minitest::Test
  def after_teardown
    CLMTest.record_result(self)
    super
  end
end