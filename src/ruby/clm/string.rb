# frozen_string_literal: true

require_relative 'bpa'

module CLM
  module String
    FAILURE = BPA::FAILURE
    SUCCESS = BPA::SUCCESS
    GET_SUCCESS = BPA::GET_SUCCESS
    SET_SUCCESS = BPA::SET_SUCCESS

    ENCODING_UTF8 = 1
    ENCODING_LATIN1 = 2
    ENCODING_UTF16LE = 3

    @default_encoding = ENCODING_UTF8

    class << self
      def set_default_encoding(encoding)
        return FAILURE unless supported_encoding?(encoding)

        @default_encoding = encoding
        SUCCESS
      end

      def get_default_encoding
        @default_encoding
      end

      def new(id, text)
        utf8 = to_utf8(text)
        return false unless utf8 && BPA.create(id, BPA::TYPE_STRING) == SUCCESS

        unless BPA.set_string(id, utf8, utf8.bytesize + 1) >= 0 &&
               BPA.set_block_encoding(id, @default_encoding) == SUCCESS
          BPA.delete(id)
          return false
        end
        true
      end

      def delete(id)
        status, info = BPA.block_info(id)
        status == GET_SUCCESS && info[:type] == BPA::TYPE_STRING && BPA.delete(id) == SUCCESS
      end

      def set(id, text)
        utf8 = to_utf8(text)
        return FAILURE unless utf8 && string_block?(id)

        count = BPA.set_string(id, utf8, utf8.bytesize + 1)
        count.negative? ? FAILURE : utf8.each_char.count
      end

      def append(id, suffix)
        current = read_text(id)
        addition = to_utf8(suffix)
        return FAILURE unless current && addition

        result = current + addition
        return FAILURE if set(id, result).negative?

        addition.each_char.count
      end

      def append_line(id, suffix)
        current = read_text(id)
        addition = to_utf8(suffix)
        return FAILURE unless current && addition

        return FAILURE if set(id, current + addition + "\n").negative?

        addition.each_char.count + 1
      end

      def insert(id, index, inserted)
        current = read_text(id)
        addition = to_utf8(inserted)
        return FAILURE unless current && addition && valid_character_range?(index, 0, current.each_char.count)

        characters = current.each_char.to_a
        characters.insert(index, addition)
        return FAILURE if set(id, characters.join).negative?

        addition.each_char.count
      end

      def remove(id, index, length)
        current = read_text(id)
        return FAILURE unless current && valid_character_range?(index, length, current.each_char.count)
        return 0 if length.zero?

        characters = current.each_char.to_a
        characters.slice!(index, length)
        return FAILURE if set(id, characters.join).negative?

        length
      end

      def contains?(id, search)
        current = read_text(id)
        needle = to_utf8(search)
        !!(current && needle && current.include?(needle))
      end

      def length(id)
        current = read_text(id)
        current ? current.each_char.count : FAILURE
      end

      def replace(id, search, replacement)
        current = read_text(id)
        needle = to_utf8(search)
        substitute = to_utf8(replacement)
        return FAILURE unless current && needle && substitute && !needle.empty?

        count = 0
        result = current.gsub(needle) do
          count += 1
          substitute
        end
        return FAILURE if set(id, result).negative?

        count
      end

      def get_bytes(id, capacity = nil)
        text = read_text(id)
        return [FAILURE, nil] unless text

        status, encoding = BPA.block_encoding(id)
        return [FAILURE, nil] unless status == GET_SUCCESS && supported_encoding?(encoding)

        bytes = text.encode(external_encoding(encoding)).b
        return [FAILURE, nil] if capacity && (!capacity.is_a?(Integer) || capacity < bytes.bytesize)

        [bytes.bytesize, bytes]
      rescue EncodingError, ArgumentError
        [FAILURE, nil]
      end

      def from_bytes(id, encoding, bytes)
        return FAILURE unless supported_encoding?(encoding) && bytes.is_a?(::String)

        text = bytes.dup.force_encoding(external_encoding(encoding)).encode(Encoding::UTF_8)
        return FAILURE if text.include?("\0") || !text.valid_encoding?

        status, info = BPA.block_info(id)
        if status != GET_SUCCESS
          return FAILURE unless new(id, text)
        elsif info[:type] != BPA::TYPE_STRING
          return FAILURE
        else
          return FAILURE if set(id, text).negative?
        end
        BPA.set_block_encoding(id, encoding)
      rescue EncodingError, ArgumentError
        FAILURE
      end

      def change_encoding(id, encoding)
        return FAILURE unless supported_encoding?(encoding)

        text = read_text(id)
        return FAILURE unless text
        return FAILURE if conversion_fails?(text, encoding)

        BPA.set_block_encoding(id, encoding)
      end

      def slice(source_id, start, length, destination_id)
        text = read_text(source_id)
        return FAILURE unless text && valid_character_range?(start, length, text.each_char.count)
        return FAILURE if BPA.block_existing?(destination_id) || BPA.array_existing?(destination_id)

        slice_text = text.each_char.drop(start).take(length).join
        return FAILURE unless new(destination_id, slice_text)

        _, preference = BPA.block_encoding(source_id)
        if change_encoding(destination_id, preference) != SUCCESS
          delete(destination_id)
          return FAILURE
        end
        SUCCESS
      end

      def slice_borrow(source_id, start, length)
        text = read_text(source_id)
        return [FAILURE, nil] unless text && valid_character_range?(start, length, text.each_char.count)

        [SUCCESS, text.each_char.drop(start).take(length).join]
      end

      def array_new(id, reserved = 0)
        BPA.create_array(id, BPA::TYPE_STRING, reserved, ENCODING_UTF8) == SUCCESS
      end

      def array_delete(id)
        string_array?(id) && BPA.array_delete(id) == SUCCESS
      end

      def array_count(id)
        return FAILURE unless string_array?(id)

        status, count = BPA.array_count(id)
        status == GET_SUCCESS ? count : FAILURE
      end

      def array_length_at(id, index)
        text = array_text_at(id, index)
        text ? text.each_char.count : FAILURE
      end

      def array_contains_at(id, index, search)
        text = array_text_at(id, index)
        needle = to_utf8(search)
        !!(text && needle && text.include?(needle))
      end

      def array_append_at(id, index, suffix)
        current = array_text_at(id, index)
        addition = to_utf8(suffix)
        return FAILURE unless current && addition

        return FAILURE unless BPA.array_set_string(id, index, current + addition) >= 0

        addition.each_char.count
      end

      def array_append_line_at(id, index, suffix)
        current = array_text_at(id, index)
        addition = to_utf8(suffix)
        return FAILURE unless current && addition

        return FAILURE unless BPA.array_set_string(id, index, current + addition + "\n") >= 0

        addition.each_char.count + 1
      end

      def array_insert_at(id, index, character_index, inserted)
        current = array_text_at(id, index)
        addition = to_utf8(inserted)
        return FAILURE unless current && addition && valid_character_range?(character_index, 0, current.each_char.count)

        characters = current.each_char.to_a
        characters.insert(character_index, addition)
        return FAILURE unless BPA.array_set_string(id, index, characters.join) >= 0

        addition.each_char.count
      end

      def array_remove_at(id, index, character_index, length)
        current = array_text_at(id, index)
        return FAILURE unless current && valid_character_range?(character_index, length, current.each_char.count)
        return 0 if length.zero?

        characters = current.each_char.to_a
        characters.slice!(character_index, length)
        return FAILURE unless BPA.array_set_string(id, index, characters.join) >= 0

        length
      end

      def array_replace_at(id, index, search, replacement)
        current = array_text_at(id, index)
        needle = to_utf8(search)
        substitute = to_utf8(replacement)
        return FAILURE unless current && needle && substitute && !needle.empty?

        count = 0
        updated = current.gsub(needle) do
          count += 1
          substitute
        end
        return FAILURE unless BPA.array_set_string(id, index, updated) >= 0

        count
      end

      def array_get(id, index, destination_string_id)
        return FAILURE unless string_block?(destination_string_id)

        status, text = BPA.array_get_string(id, index)
        return FAILURE if status.negative?
        return FAILURE if set(destination_string_id, text).negative?

        _, info = BPA.array_info(id)
        BPA.set_block_encoding(destination_string_id, info[:encoding]) == SUCCESS ? GET_SUCCESS : FAILURE
      end

      def array_set(id, index, source_string_id)
        text = read_text(source_string_id)
        return FAILURE unless string_array?(id) && text

        BPA.array_set_string(id, index, text) >= 0 ? SET_SUCCESS : FAILURE
      end

      def array_insert(id, index, source_string_id)
        text = read_text(source_string_id)
        return FAILURE unless string_array?(id) && text

        BPA.array_insert_string(id, index, text)
      end

      def array_append(id, source_string_id)
        text = read_text(source_string_id)
        return FAILURE unless string_array?(id) && text

        BPA.array_append_string_value(id, text)
      end

      def array_remove(id, index)
        return FAILURE unless string_array?(id)

        BPA.array_remove(id, index)
      end

      def array_clear(id)
        return FAILURE unless string_array?(id)

        BPA.array_clear(id)
      end

      def array_find(id, source_string_id)
        source = read_text(source_string_id)
        return FAILURE unless string_array?(id) && source

        count = array_count(id)
        count.times do |index|
          text = array_text_at(id, index)
          return index if text == source
        end
        FAILURE
      end

      def array_sort(id)
        borrow_status, borrow = BPA.advanced_borrow(id)
        return FAILURE unless borrow_status == SUCCESS && borrow.store.type == BPA::TYPE_STRING

        borrow.store.items.sort_by!(&:b)
        borrow.store.index = 0
        borrow.store.deck_size = borrow.store.items.length
        BPA.advanced_release(borrow)
        SUCCESS
      end

      def array_shuffle(id, random: Random)
        borrow_status, borrow = BPA.advanced_borrow(id)
        return FAILURE unless borrow_status == SUCCESS && borrow.store.type == BPA::TYPE_STRING

        borrow.store.items.shuffle!(random: random)
        borrow.store.index = 0
        borrow.store.deck_size = borrow.store.items.length
        BPA.advanced_release(borrow)
        SUCCESS
      end

      def array_move_first(id)
        string_array?(id) ? BPA.array_move_first(id) : FAILURE
      end

      def array_move_previous(id)
        string_array?(id) ? BPA.array_move_previous(id) : FAILURE
      end

      def array_move_next(id)
        string_array?(id) ? BPA.array_move_next(id) : FAILURE
      end

      def array_move_last(id)
        string_array?(id) ? BPA.array_move_last(id) : FAILURE
      end

      def array_move_to(id, index)
        string_array?(id) ? BPA.array_move_to(id, index) : FAILURE
      end

      def array_get_current(id, destination_string_id)
        return FAILURE unless string_array?(id)

        status, info = BPA.array_info(id)
        return FAILURE unless status == GET_SUCCESS

        array_get(id, info[:index], destination_string_id)
      end

      def array_draw_at(id, deck_index, destination_string_id)
        with_string_array_borrow(id) do |array|
          return FAILURE unless deck_index.is_a?(Integer) && deck_index >= 0 && deck_index < array.deck_size
          return FAILURE unless string_block?(destination_string_id)

          text = array.items[deck_index]
          return FAILURE if set(destination_string_id, text).negative?
          BPA.set_block_encoding(destination_string_id, array.encoding)

          last = array.deck_size - 1
          array.items[deck_index], array.items[last] = array.items[last], array.items[deck_index] if deck_index != last
          array.deck_size -= 1
          GET_SUCCESS
        end
      end

      def array_draw(id, destination_string_id, random: Random)
        with_string_array_borrow(id) do |array|
          if array.deck_size.zero?
            return FAILURE if array.items.empty?

            array.items.shuffle!(random: random)
            array.index = 0
            array.deck_size = array.items.length
          end
          array_draw_at(id, 0, destination_string_id)
        end
      end

      private

      def supported_encoding?(encoding)
        [ENCODING_UTF8, ENCODING_LATIN1, ENCODING_UTF16LE].include?(encoding)
      end

      def external_encoding(encoding)
        case encoding
        when ENCODING_UTF8 then Encoding::UTF_8
        when ENCODING_LATIN1 then Encoding::ISO_8859_1
        when ENCODING_UTF16LE then Encoding::UTF_16LE
        end
      end

      def to_utf8(text)
        return nil unless text.is_a?(::String)

        result = text.encode(Encoding::UTF_8)
        return nil if result.include?("\0") || !result.valid_encoding?

        result
      rescue EncodingError, ArgumentError
        nil
      end

      def read_text(id)
        length, bytes = BPA.get_string(id)
        return nil if length.negative?

        text = bytes.dup.force_encoding(Encoding::UTF_8)
        text.valid_encoding? && !text.include?("\0") ? text : nil
      end

      def string_block?(id)
        status, info = BPA.block_info(id)
        status == GET_SUCCESS && info[:type] == BPA::TYPE_STRING
      end

      def string_array?(id)
        status, info = BPA.array_info(id)
        status == GET_SUCCESS && info[:type] == BPA::TYPE_STRING && info[:encoding] == ENCODING_UTF8
      end

      def array_text_at(id, index)
        return nil unless string_array?(id)

        length, bytes = BPA.array_get_string(id, index)
        return nil if length.negative?

        text = bytes.dup.force_encoding(Encoding::UTF_8)
        text.valid_encoding? ? text : nil
      end

      def valid_character_range?(index, length, count)
        index.is_a?(Integer) && length.is_a?(Integer) && index >= 0 && length >= 0 &&
          index <= count && length <= count - index
      end

      def conversion_fails?(text, encoding)
        text.encode(external_encoding(encoding))
        false
      rescue EncodingError
        true
      end

      def with_string_array_borrow(id)
        status, borrow = BPA.advanced_borrow(id)
        return FAILURE unless status == SUCCESS && borrow.store.type == BPA::TYPE_STRING

        yield borrow.store
      ensure
        BPA.advanced_release(borrow) if borrow
      end
    end
  end
end