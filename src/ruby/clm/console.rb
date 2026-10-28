# frozen_string_literal: true

module CLM
end

## Console operations for standard streams and IO-like objects.
module CLM::Console
  # Operation completed successfully.
  SUCCESS = 0
  # An I/O or runtime operation failed.
  FAILURE = -1
  # A required argument is invalid.
  INVALID_ARGUMENT = -2
  # Maximum input line size in bytes, including C's terminator allowance.
  INPUT_CAPACITY = 4096

  CLM_CONSOLE_SUCCESS = SUCCESS
  CLM_CONSOLE_FAILURE = FAILURE
  CLM_CONSOLE_INVALID_ARGUMENT = INVALID_ARGUMENT

  CONSOLE_COLOR_RESET = "\e[0m"

  CONSOLE_COLOR_BLACK = "\e[30m"
  CONSOLE_COLOR_RED = "\e[31m"
  CONSOLE_COLOR_GREEN = "\e[32m"
  CONSOLE_COLOR_YELLOW = "\e[33m"
  CONSOLE_COLOR_BLUE = "\e[34m"
  CONSOLE_COLOR_MAGENTA = "\e[35m"
  CONSOLE_COLOR_CYAN = "\e[36m"
  CONSOLE_COLOR_WHITE = "\e[37m"

  CONSOLE_COLOR_BLACK_BOLD = "\e[1;30m"
  CONSOLE_COLOR_RED_BOLD = "\e[1;31m"
  CONSOLE_COLOR_GREEN_BOLD = "\e[1;32m"
  CONSOLE_COLOR_YELLOW_BOLD = "\e[1;33m"
  CONSOLE_COLOR_BLUE_BOLD = "\e[1;34m"
  CONSOLE_COLOR_MAGENTA_BOLD = "\e[1;35m"
  CONSOLE_COLOR_CYAN_BOLD = "\e[1;36m"
  CONSOLE_COLOR_WHITE_BOLD = "\e[1;37m"

  CONSOLE_COLOR_BLACK_BG = "\e[40m"
  CONSOLE_COLOR_RED_BG = "\e[41m"
  CONSOLE_COLOR_GREEN_BG = "\e[42m"
  CONSOLE_COLOR_YELLOW_BG = "\e[43m"
  CONSOLE_COLOR_BLUE_BG = "\e[44m"
  CONSOLE_COLOR_MAGENTA_BG = "\e[45m"
  CONSOLE_COLOR_CYAN_BG = "\e[46m"
  CONSOLE_COLOR_WHITE_BG = "\e[47m"

  module_function

  # Initializes the console interface; no setup is currently required.
  # @return [Integer] {SUCCESS}
  def init
    SUCCESS
  end

  # Restores console state; no platform-specific operation is currently needed.
  # @return [Integer] {SUCCESS}
  def restore
    SUCCESS
  end

  # Writes ANSI clear-screen and home-cursor controls to a stream.
  # @param stream [#write] destination stream
  # @return [Integer] a console status code
  def clear_to(stream)
    write_to(stream, "\e[2J\e[H")
  end

  # Clears the screen on standard output and flushes it.
  # @return [Integer] a console status code
  def clear
    status = clear_to($stdout)
    status == SUCCESS ? flush : status
  end

  # Flushes standard output.
  # @return [Integer] a console status code
  def flush
    $stdout.flush
    SUCCESS
  rescue IOError, SystemCallError
    FAILURE
  end

  # Emits an ANSI cursor-position sequence to a stream.
  # Coordinates are zero-based and must be nonnegative integers.
  # @param stream [#write] destination stream
  # @param position [#x, #y] position-like object
  # @return [Integer] a console status code
  def set_cursor_position_to(stream, position)
    return INVALID_ARGUMENT unless position&.respond_to?(:x) && position.respond_to?(:y)
    return INVALID_ARGUMENT unless position.x.is_a?(Integer) && position.y.is_a?(Integer)
    return INVALID_ARGUMENT if position.x.negative? || position.y.negative?

    write_to(stream, "\e[#{position.y + 1};#{position.x + 1}H")
  end

  # Moves the standard-output cursor to a zero-based position and flushes it.
  # @param position [#x, #y] position-like object with nonnegative integer coordinates
  # @return [Integer] a console status code
  def set_cursor_position(position)
    status = set_cursor_position_to($stdout, position)
    status == SUCCESS ? flush : status
  end

  # Emits an ANSI cursor visibility sequence to a stream.
  # @param stream [#write] destination stream
  # @param visible [Boolean] whether the cursor should be shown
  # @return [Integer] a console status code
  def set_cursor_visible_to(stream, visible)
    return INVALID_ARGUMENT unless visible == true || visible == false

    write_to(stream, visible ? "\e[?25h" : "\e[?25l")
  end

  # Shows the standard-output cursor and flushes it.
  # @return [Integer] a console status code
  def show_cursor
    status = set_cursor_visible_to($stdout, true)
    status == SUCCESS ? flush : status
  end

  # Hides the standard-output cursor and flushes it.
  # @return [Integer] a console status code
  def hide_cursor
    status = set_cursor_visible_to($stdout, false)
    status == SUCCESS ? flush : status
  end

  # Writes text to an IO-like stream without adding a newline.
  # @param stream [#write] destination stream
  # @param text [String, #to_str] text to write
  # @return [Integer] a console status code
  def write_to(stream, text)
    text = String.try_convert(text)
    return INVALID_ARGUMENT unless stream&.respond_to?(:write) && text

    stream.write(text)
    SUCCESS
  rescue IOError, SystemCallError, EncodingError, TypeError
    FAILURE
  end

  # Writes text to standard output without adding a newline.
  # @param text [String, #to_str] text to write
  # @return [Integer] a console status code
  def write(text)
    write_to($stdout, text)
  end

  # Formats text with Ruby's `String#%` formatting and writes it to a stream.
  # @param stream [#write] destination stream
  # @param format_string [String] format string
  # @param arguments [Array] values consumed by the format string
  # @return [Integer] a console status code
  def write_format_to(stream, format_string, *arguments)
    return INVALID_ARGUMENT unless format_string.is_a?(String)

    write_to(stream, format_string % arguments)
  rescue ArgumentError, TypeError
    FAILURE
  end

  # Formats text, appends LF, and writes it to a stream without flushing.
  # @param stream [#write] destination stream
  # @param format_string [String] format string
  # @param arguments [Array] values consumed by the format string
  # @return [Integer] a console status code
  def write_line_format_to(stream, format_string, *arguments)
    status = write_format_to(stream, format_string, *arguments)
    status == SUCCESS ? write_to(stream, "\n") : status
  end

  # Formats and writes text to standard output without a newline.
  # @param format_string [String] format string
  # @param arguments [Array] values consumed by the format string
  # @return [Integer] a console status code
  def write_format(format_string, *arguments)
    write_format_to($stdout, format_string, *arguments)
  end

  # Formats and writes a line to standard output, then flushes it.
  # @param format_string [String] format string
  # @param arguments [Array] values consumed by the format string
  # @return [Integer] a console status code
  def write_line_format(format_string, *arguments)
    status = write_line_format_to($stdout, format_string, *arguments)
    status == SUCCESS ? flush : status
  end

  # Writes text followed by LF to a stream without flushing.
  # @param stream [#write] destination stream
  # @param text [String, #to_str] text to write
  # @return [Integer] a console status code
  def write_line_to(stream, text)
    status = write_to(stream, text)
    status == SUCCESS ? write_to(stream, "\n") : status
  end

  # Writes a line to standard output and flushes it.
  # @param text [String, #to_str] text to write
  # @return [Integer] a console status code
  def write_line(text)
    status = write_line_to($stdout, text)
    status == SUCCESS ? flush : status
  end

  # Reads one byte-oriented line, stripping LF and an optional preceding CR.
  # Overlong lines are drained and fail; returned text is a binary string.
  # @param stream [#getbyte] input stream
  # @param capacity [Integer] maximum line size in bytes, including terminator allowance
  # @return [Array(Integer, String, nil)] status and line, or status and nil on failure
  def read_line_from(stream, capacity = INPUT_CAPACITY)
    return [INVALID_ARGUMENT, nil] unless stream&.respond_to?(:getbyte)
    return [INVALID_ARGUMENT, nil] unless capacity.is_a?(Integer) && capacity.between?(2, (1 << 31) - 1)

    buffer = +''.b
    limit = capacity - 1

    loop do
      byte = stream.getbyte
      return [FAILURE, nil] if byte.nil? && buffer.empty?
      break if byte.nil? || byte == 10

      if buffer.bytesize == limit
        loop do
          overflow_byte = stream.getbyte
          break if overflow_byte.nil? || overflow_byte == 10
        end
        return [FAILURE, nil]
      end

      buffer << byte
    end

    buffer.chop! if buffer.end_with?("\r".b)
    [SUCCESS, buffer]
  rescue IOError, SystemCallError, TypeError
    [FAILURE, nil]
  end

  # Reads one line from standard input.
  # @return [Array(Integer, String, nil)] status and binary line, or status and nil on failure
  def read_line
    read_line_from($stdin)
  end

  # Optionally writes a prompt, flushes standard output, then reads a line.
  # @param prompt [String, #to_str, nil] optional prompt text
  # @return [Array(Integer, String, nil)] status and binary line, or status and nil on failure
  def read(prompt = nil)
    if !prompt.nil?
      status = write_to($stdout, prompt)
      return [status, nil] unless status == SUCCESS
    end
    return [FAILURE, nil] unless flush == SUCCESS

    read_line
  end

  # Reads one byte from a stream; this does not decode a Unicode character.
  # @param stream [#getbyte] input stream
  # @return [Array(Integer, String, nil)] status and one-byte binary string, or status and nil
  def read_character_from(stream)
    return [INVALID_ARGUMENT, nil] unless stream&.respond_to?(:getbyte)

    byte = stream.getbyte
    byte.nil? ? [FAILURE, nil] : [SUCCESS, byte.chr(Encoding::BINARY)]
  rescue IOError, SystemCallError, TypeError
    [FAILURE, nil]
  end

  # Reads one byte from standard input; this is not raw key input.
  # @return [Array(Integer, String, nil)] status and one-byte binary string, or status and nil
  def read_character
    read_character_from($stdin)
  end

  # Reads one byte from standard input, equivalent to {read_character}.
  # @return [Array(Integer, String, nil)] status and one-byte binary string, or status and nil
  def read_key
    read_character
  end

  # Writes exactly one valid UTF-8 Unicode scalar value to a stream.
  # @param stream [#write] destination stream
  # @param character [String, #to_str] one UTF-8 character
  # @return [Integer] a console status code
  def write_character_to(stream, character)
    character = String.try_convert(character)
    return INVALID_ARGUMENT unless character

    utf8_character = character.b.dup.force_encoding(Encoding::UTF_8)
    return INVALID_ARGUMENT unless utf8_character.valid_encoding? && utf8_character.each_char.count == 1
    return INVALID_ARGUMENT if utf8_character.include?("\0")

    write_to(stream, character.b)
  end

  # Writes one UTF-8 character to standard output and flushes it.
  # @param character [String, #to_str] one UTF-8 character
  # @return [Integer] a console status code
  def write_character(character)
    status = write_character_to($stdout, character)
    status == SUCCESS ? flush : status
  end

  # Prompts on supplied streams and parses `y`, `yes`, `n`, or `no`.
  # Answers are case-insensitive and surrounding whitespace is ignored.
  # @param input [#getbyte] input stream
  # @param output [#write] prompt stream
  # @param prompt [String, #to_str, nil] optional prompt text
  # @return [Array(Integer, Boolean, nil)] status and answer, or status and nil if invalid
  def confirm_from(input, output, prompt = nil)
    return [INVALID_ARGUMENT, nil] unless input&.respond_to?(:getbyte) && output&.respond_to?(:write)

    unless prompt.nil?
      status = write_to(output, prompt)
      return [status, nil] unless status == SUCCESS
    end
    return [FAILURE, nil] unless write_to(output, ' [y/n]: ') == SUCCESS && flush_stream(output) == SUCCESS

    status, response = read_line_from(input)
    return [status, nil] unless status == SUCCESS

    case response.strip.downcase
    when 'y', 'yes'
      [SUCCESS, true]
    when 'n', 'no'
      [SUCCESS, false]
    else
      [INVALID_ARGUMENT, nil]
    end
  end

  # Prompts on standard streams for a yes/no answer.
  # @param prompt [String, #to_str, nil] optional prompt text
  # @return [Array(Integer, Boolean, nil)] status and answer, or status and nil if invalid
  def confirm(prompt = nil)
    confirm_from($stdin, $stdout, prompt)
  end

  # Writes ANSI color, text, and a reset sequence to a stream.
  # The log or redirected stream receives the escape sequences literally.
  # @param stream [#write] destination stream
  # @param text [String, #to_str] text to color
  # @param color [String, #to_str] ANSI color/style sequence
  # @return [Integer] a console status code
  def write_color_to(stream, text, color)
    return INVALID_ARGUMENT if text.nil? || color.nil?

    status = write_to(stream, color)
    return status unless status == SUCCESS

    status = write_to(stream, text)
    return status unless status == SUCCESS

    write_to(stream, CONSOLE_COLOR_RESET)
  end

  # Writes ANSI-colored text to standard output and flushes it.
  # @param text [String, #to_str] text to color
  # @param color [String, #to_str] ANSI color/style sequence
  # @return [Integer] a console status code
  def write_color(text, color)
    status = write_color_to($stdout, text, color)
    status == SUCCESS ? flush : status
  end

  # Waits for a finite, nonnegative duration using Ruby's sleep.
  # Unlike the C implementation, this does not busy-wait.
  # @param seconds [Numeric] duration in seconds
  # @return [Integer] a console status code
  def wait(seconds)
    return INVALID_ARGUMENT unless seconds.is_a?(Numeric)

    duration = Float(seconds)
    return INVALID_ARGUMENT unless duration.finite? && duration >= 0
    return SUCCESS if duration.zero?

    sleep(duration)
    SUCCESS
  rescue ArgumentError, RangeError, TypeError
    INVALID_ARGUMENT
  rescue StandardError
    FAILURE
  end

  # Writes text one byte at a time, flushing between bytes and sleeping between them.
  # @param stream [#write, #flush] destination stream
  # @param text [String, #to_str] text to write
  # @param delay_seconds [Numeric] finite, nonnegative delay between bytes
  # @return [Integer] a console status code
  def write_effect_to(stream, text, delay_seconds)
    text = String.try_convert(text)
    return INVALID_ARGUMENT unless stream&.respond_to?(:write) && text
    return INVALID_ARGUMENT unless delay_seconds.is_a?(Numeric)

    delay = Float(delay_seconds)
    return INVALID_ARGUMENT unless delay.finite? && delay >= 0

    bytes = text.b.bytes
    bytes.each_with_index do |byte, index|
      return FAILURE unless write_to(stream, byte.chr(Encoding::BINARY)) == SUCCESS
      return FAILURE unless flush_stream(stream) == SUCCESS
      return FAILURE unless wait(delay) == SUCCESS if index < bytes.length - 1
    end

    SUCCESS
  rescue ArgumentError, RangeError, TypeError
    INVALID_ARGUMENT
  end

  # Writes text to standard output byte by byte with a delay between bytes.
  # @param text [String, #to_str] text to write
  # @param delay_seconds [Numeric] finite, nonnegative delay between bytes
  # @return [Integer] a console status code
  def write_effect(text, delay_seconds)
    write_effect_to($stdout, text, delay_seconds)
  end

  # Flushes an IO-like stream.
  # @param stream [#flush] stream to flush
  # @return [Integer] a console status code
  def flush_stream(stream)
    return INVALID_ARGUMENT unless stream&.respond_to?(:flush)

    stream.flush
    SUCCESS
  rescue IOError, SystemCallError
    FAILURE
  end
end