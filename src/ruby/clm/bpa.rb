# frozen_string_literal: true

require_relative 'base'

module CLM
  module BPA
    FAILURE = -1
    SUCCESS = 0
    GET_SUCCESS = 1
    SET_SUCCESS = 2

    TYPE_STRUCT = 0
    TYPE_SIGNED_BYTE = 1
    TYPE_BYTE = 2
    TYPE_SHORT = 3
    TYPE_UNSIGNED_SHORT = 4
    TYPE_INT = 5
    TYPE_UNSIGNED_INT = 6
    TYPE_LONG = 7
    TYPE_UNSIGNED_LONG = 8
    TYPE_FLOAT = 9
    TYPE_DOUBLE = 10
    TYPE_STRING = 11
    TYPE_UUID = 12
    TYPE_DATETIME = 13

    CLM_BPA_FAILURE = FAILURE
    CLM_BPA_SUCCESS = SUCCESS
    CLM_BPA_GET_SUCCESS = GET_SUCCESS
    CLM_BPA_SET_SUCCESS = SET_SUCCESS

    TYPE_SIZES = {
      TYPE_SIGNED_BYTE => 1,
      TYPE_BYTE => 1,
      TYPE_SHORT => 2,
      TYPE_UNSIGNED_SHORT => 2,
      TYPE_INT => 4,
      TYPE_UNSIGNED_INT => 4,
      TYPE_LONG => 8,
      TYPE_UNSIGNED_LONG => 8,
      TYPE_FLOAT => 4,
      TYPE_DOUBLE => 8,
      TYPE_UUID => 16,
      TYPE_DATETIME => 8
    }.freeze

    INTEGER_RANGES = {
      TYPE_SIGNED_BYTE => (-(1 << 7))..((1 << 7) - 1),
      TYPE_BYTE => 0..((1 << 8) - 1),
      TYPE_SHORT => (-(1 << 15))..((1 << 15) - 1),
      TYPE_UNSIGNED_SHORT => 0..((1 << 16) - 1),
      TYPE_INT => (-(1 << 31))..((1 << 31) - 1),
      TYPE_UNSIGNED_INT => 0..((1 << 32) - 1),
      TYPE_LONG => (-(1 << 63))..((1 << 63) - 1),
      TYPE_UNSIGNED_LONG => 0..((1 << 64) - 1),
      TYPE_DATETIME => (-(1 << 63))..((1 << 63) - 1)
    }.freeze

    Block = Struct.new(:id, :type, :block_size, :refcount, :encoding, :value, keyword_init: true)
    ArrayStore = Struct.new(:id, :type, :block_size, :encoding, :items, :capacity,
                            :index, :sealed, :deck_size, :refcount, keyword_init: true)

    class ArrayBorrow
      attr_reader :store
      attr_accessor :released

      def initialize(store)
        @store = store
        @released = false
      end
    end

    class SliceView
      include Enumerable

      def initialize(store, start, length)
        @store = store
        @start = start
        @length = length
      end

      def [](index)
        return nil unless index.is_a?(Integer) && index >= 0 && index < @length

        @store.items[@start + index]
      end

      def length
        @length
      end

      alias size length

      def each
        return enum_for(__method__) unless block_given?

        @length.times { |offset| yield @store.items[@start + offset] }
      end

      def to_a
        each.to_a
      end
    end

    @blocks = {}
    @arrays = {}

    class << self
      def type_size(type)
        TYPE_SIZES.fetch(type, 0)
      end

      def block_existing?(id)
        Base.valid_id?(id) && @blocks.key?(id)
      end

      alias is_existing? block_existing?

      def array_existing?(id)
        Base.valid_id?(id) && @arrays.key?(id)
      end

      def array_info(id)
        array = @arrays[id]
        return [FAILURE, nil] unless array

        [GET_SUCCESS, { type: array.type, block_size: array.block_size,
                        encoding: array.encoding, count: array.items.length,
                        capacity: array.capacity, sealed: array.sealed,
                        index: array.index, deck_size: array.deck_size }]
      end

      def create(id, type)
        return FAILURE if type == TYPE_STRUCT

        create_sized(id, type, 0)
      end

      def create_sized(id, type, block_size)
        return FAILURE unless valid_id?(id) && !block_existing?(id)
        return FAILURE unless valid_type?(type) && block_size.is_a?(Integer) && block_size >= 0

        if type == TYPE_STRING
          return FAILURE unless block_size.zero?
          resolved_size = 0
          value = nil
        elsif type == TYPE_STRUCT
          return FAILURE if block_size.zero?
          resolved_size = block_size
          value = nil
        else
          resolved_size = type_size(type)
          return FAILURE if resolved_size.zero? || (!block_size.zero? && block_size != resolved_size)
          value = default_value(type)
        end

        @blocks[id] = Block.new(id: id, type: type, block_size: resolved_size,
                                refcount: 1, encoding: 0, value: value)
        SUCCESS
      end

      def delete(id)
        @blocks.delete(id) ? SUCCESS : FAILURE
      end

      def ref(id)
        block = @blocks[id]
        return FAILURE unless block && block.refcount < 65_535

        block.refcount += 1
        SUCCESS
      end

      def unref(id)
        block = @blocks[id]
        return FAILURE unless block

        block.refcount -= 1
        block.refcount.zero? ? delete(id) : SUCCESS
      end

      def get(id)
        block = @blocks[id]
        return [FAILURE, nil] unless block && block.type != TYPE_STRING

        [GET_SUCCESS, copy_value(block.value)]
      end

      def set(id, value)
        block = @blocks[id]
        return FAILURE unless block && block.type != TYPE_STRING && valid_value?(block, value)

        block.value = copy_value(value)
        SET_SUCCESS
      end

      def get_string(id, capacity = nil)
        block = @blocks[id]
        return [FAILURE, nil] unless block && block.type == TYPE_STRING && block.value

        value = block.value.dup
        return [FAILURE, nil] if capacity && (!capacity.is_a?(Integer) || capacity < value.bytesize + 1)

        [value.bytesize, value]
      end

      def set_string(id, value, buffer_size = nil)
        block = @blocks[id]
        return FAILURE unless block && block.type == TYPE_STRING

        copied = bounded_string(value, buffer_size)
        return FAILURE unless copied

        block.value = copied
        copied.bytesize
      end

      def replace(id, value, buffer_size = nil)
        block = @blocks[id]
        return FAILURE unless block

        block.type == TYPE_STRING ? set_string(id, value, buffer_size) : set(id, value)
      end

      def copy_block(source_id, destination_id)
        source = @blocks[source_id]
        destination = @blocks[destination_id]
        return FAILURE unless source && destination && source.type == destination.type
        return FAILURE unless source.block_size == destination.block_size
        return FAILURE if source.type == TYPE_STRING && source.value.nil?

        destination.value = copy_value(source.value)
        SUCCESS
      end

      def borrow(id)
        block = @blocks[id]
        return [FAILURE, nil] unless block

        [GET_SUCCESS, block.value]
      end

      def block_info(id)
        block = @blocks[id]
        return [FAILURE, nil] unless block

        [GET_SUCCESS, { type: block.type, block_size: block.block_size, encoding: block.encoding }]
      end

      def block_encoding(id)
        block = @blocks[id]
        block ? [GET_SUCCESS, block.encoding] : [FAILURE, nil]
      end

      def set_block_encoding(id, encoding)
        block = @blocks[id]
        return FAILURE unless block && encoding.is_a?(Integer) && encoding.between?(0, 255)

        block.encoding = encoding
        SUCCESS
      end

      def create_array(id, type, reserved = 0, encoding = 0)
        return FAILURE if type == TYPE_STRUCT

        create_array_sized(id, type, reserved, 0, encoding)
      end

      def create_array_sized(id, type, reserved, block_size, encoding = 0)
        return FAILURE unless valid_id?(id) && !array_existing?(id)
        return FAILURE unless valid_type?(type) && reserved.is_a?(Integer) && reserved >= 0
        return FAILURE unless block_size.is_a?(Integer) && block_size >= 0

        if type == TYPE_STRING
          return FAILURE unless block_size.zero?
          resolved_size = 0
        elsif type == TYPE_STRUCT
          return FAILURE if block_size.zero?
          resolved_size = block_size
        else
          resolved_size = type_size(type)
          return FAILURE if resolved_size.zero? || (!block_size.zero? && block_size != resolved_size)
        end

        @arrays[id] = ArrayStore.new(id: id, type: type, block_size: resolved_size,
                                     encoding: encoding, items: [], capacity: reserved,
                                     index: 0, sealed: false, deck_size: 0, refcount: 1)
        SUCCESS
      end

      def array_delete(id)
        @arrays.delete(id) ? SUCCESS : FAILURE
      end

      def array_ref(id)
        array = @arrays[id]
        return FAILURE unless array && array.refcount < 65_535

        array.refcount += 1
        SUCCESS
      end

      def array_unref(id)
        array = @arrays[id]
        return FAILURE unless array

        array.refcount -= 1
        array.refcount.zero? ? array_delete(id) : SUCCESS
      end

      def array_count(id)
        array = @arrays[id]
        array ? [GET_SUCCESS, array.items.length] : [FAILURE, nil]
      end

      def array_capacity(id)
        array = @arrays[id]
        array ? [GET_SUCCESS, array.capacity] : [FAILURE, nil]
      end

      def array_length(id)
        array = @arrays[id]
        return [FAILURE, nil] unless array && array.type == TYPE_STRING && array.index < array.items.length

        [GET_SUCCESS, array.items[array.index].bytesize]
      end

      def array_current_index(id)
        array = @arrays[id]
        array ? [GET_SUCCESS, array.index] : [FAILURE, nil]
      end

      def array_sealed?(id)
        array = @arrays[id]
        array ? [GET_SUCCESS, array.sealed] : [FAILURE, nil]
      end

      def array_get(id, index)
        array = @arrays[id]
        return [FAILURE, nil] unless array && array.type != TYPE_STRING && valid_index?(index, array.items.length)

        [GET_SUCCESS, copy_value(array.items[index])]
      end

      def array_set(id, index, value)
        array = @arrays[id]
        return FAILURE unless array && array.type != TYPE_STRING && valid_index?(index, array.items.length)
        return FAILURE unless valid_array_value?(array, value)

        array.items[index] = copy_value(value)
        array.deck_size = array.items.length
        SET_SUCCESS
      end

      def array_insert(id, index, value)
        array = @arrays[id]
        return FAILURE unless array && valid_insert_index?(index, array.items.length)
        return FAILURE unless valid_array_value?(array, value) && ensure_capacity(array, array.items.length + 1)

        stored_value = array.type == TYPE_STRING ? bounded_string(value) : copy_value(value)
        return FAILURE unless stored_value

        array.items.insert(index, stored_value)
        array.index = index
        array.deck_size = array.items.length
        SUCCESS
      end

      def array_append(id, value)
        array = @arrays[id]
        return FAILURE unless array

        array_insert(id, array.items.length, value)
      end

      def array_get_string(id, index)
        array = @arrays[id]
        return [FAILURE, nil] unless array && array.type == TYPE_STRING && valid_index?(index, array.items.length)

        value = array.items[index].dup
        [value.bytesize, value]
      end

      def array_set_string(id, index, value, buffer_size = nil)
        array = @arrays[id]
        return FAILURE unless array && array.type == TYPE_STRING && valid_index?(index, array.items.length)

        copied = bounded_string(value, buffer_size)
        return FAILURE unless copied

        array.items[index] = copied
        array.deck_size = array.items.length
        copied.bytesize
      end

      def array_insert_string(id, index, value, buffer_size = nil)
        array = @arrays[id]
        return FAILURE unless array && array.type == TYPE_STRING && valid_insert_index?(index, array.items.length)

        copied = bounded_string(value, buffer_size)
        return FAILURE unless copied && ensure_capacity(array, array.items.length + 1)

        array.items.insert(index, copied)
        array.index = index
        array.deck_size = array.items.length
        SUCCESS
      end

      def array_append_string_value(id, value, buffer_size = nil)
        array = @arrays[id]
        return FAILURE unless array

        array_insert_string(id, array.items.length, value, buffer_size)
      end

      def array_remove(id, index)
        array = @arrays[id]
        return FAILURE unless array && valid_index?(index, array.items.length)

        array.items.delete_at(index)
        update_cursor_after_remove(array, index)
        SUCCESS
      end

      def array_remove_first(id)
        array_remove(id, 0)
      end

      def array_remove_last(id)
        array = @arrays[id]
        return FAILURE unless array && !array.items.empty?

        array_remove(id, array.items.length - 1)
      end

      def array_clear(id)
        array = @arrays[id]
        return FAILURE unless array

        array.items.clear
        array.index = 0
        array.deck_size = 0
        SUCCESS
      end

      def array_extend(id, additional_capacity)
        array = @arrays[id]
        return FAILURE unless array && !array.sealed && additional_capacity.is_a?(Integer) && additional_capacity.positive?

        array.capacity += additional_capacity
        SUCCESS
      end

      def array_shrink(id, new_capacity)
        array = @arrays[id]
        return FAILURE unless array && !array.sealed && new_capacity.is_a?(Integer) &&
                              new_capacity >= array.items.length && new_capacity < array.capacity

        array.capacity = new_capacity
        SUCCESS
      end

      def array_seal(id)
        array = @arrays[id]
        return FAILURE unless array

        array.sealed = true
        SUCCESS
      end

      def array_unseal(id)
        array = @arrays[id]
        return FAILURE unless array

        array.sealed = false
        SUCCESS
      end

      def array_reset(id)
        array = @arrays[id]
        return FAILURE unless array

        array.index = 0
        SUCCESS
      end

      def array_move_first(id)
        array_reset(id)
      end

      def array_move_previous(id)
        array = @arrays[id]
        return FAILURE unless array && array.index.positive?

        array.index -= 1
        SUCCESS
      end

      def array_move_next(id)
        array = @arrays[id]
        return FAILURE unless array && array.index + 1 < array.items.length

        array.index += 1
        SUCCESS
      end

      def array_move_last(id)
        array = @arrays[id]
        return FAILURE unless array && !array.items.empty?

        array.index = array.items.length - 1
        SUCCESS
      end

      def array_move_to(id, index)
        array = @arrays[id]
        return FAILURE unless array && valid_index?(index, array.items.length)

        array.index = index
        SUCCESS
      end

      def array_get_current(id)
        array = @arrays[id]
        return [FAILURE, nil] unless array && valid_index?(array.index, array.items.length)

        array.type == TYPE_STRING ? array_get_string(id, array.index) : array_get(id, array.index)
      end

      def array_get_first(id)
        array_get(id, 0)
      end

      def array_get_last(id)
        array = @arrays[id]
        return [FAILURE, nil] unless array && !array.items.empty?

        array_get(id, array.items.length - 1)
      end

      def array_get_first_string(id)
        array_get_string(id, 0)
      end

      def array_get_last_string(id)
        array = @arrays[id]
        return [FAILURE, nil] unless array && !array.items.empty?

        array_get_string(id, array.items.length - 1)
      end

      def array_slice(source_id, start, length, destination_id)
        source = @arrays[source_id]
        return FAILURE unless valid_slice?(source, start, length) && !array_existing?(destination_id)
        return FAILURE unless create_array_sized(destination_id, source.type, length,
                                                 source.type == TYPE_STRUCT ? source.block_size : 0,
                                                 source.encoding) == SUCCESS

        destination = @arrays[destination_id]
        destination.items.concat(source.items.slice(start, length).map { |item| copy_value(item) })
        destination.deck_size = destination.items.length
        SUCCESS
      rescue StandardError
        array_delete(destination_id)
        FAILURE
      end

      def array_slice_to(source_id, start, length)
        source = @arrays[source_id]
        return [FAILURE, nil] unless valid_slice?(source, start, length) && source.type != TYPE_STRING

        [SUCCESS, source.items.slice(start, length).map { |item| copy_value(item) }]
      end

      def array_slice_borrow(source_id, start, length)
        source = @arrays[source_id]
        return [FAILURE, nil] unless valid_slice?(source, start, length) && source.type != TYPE_STRING

        [SUCCESS, SliceView.new(source, start, length)]
      end

      def array_get_to_block(array_id, index, block_id, expected_size = nil)
        array = @arrays[array_id]
        block = @blocks[block_id]
        return FAILURE unless compatible?(array, block, index, expected_size)

        block.value = copy_value(array.items[index])
        GET_SUCCESS
      end

      def array_set_from_block(array_id, index, block_id, expected_size = nil)
        array = @arrays[array_id]
        block = @blocks[block_id]
        return FAILURE unless compatible?(array, block, index, expected_size)

        array_set(array_id, index, block.value)
      end

      def array_insert_from_block(array_id, index, block_id, expected_size = nil)
        array = @arrays[array_id]
        block = @blocks[block_id]
        return FAILURE unless compatible?(array, block, index, expected_size, insertion: true)

        array_insert(array_id, index, block.value)
      end

      def array_append_from_block(array_id, block_id, expected_size = nil)
        array = @arrays[array_id]
        return FAILURE unless array

        array_insert_from_block(array_id, array.items.length, block_id, expected_size)
      end

      def array_get_string_to_block(array_id, index, block_id)
        array = @arrays[array_id]
        block = @blocks[block_id]
        return FAILURE unless array && block && array.type == TYPE_STRING && block.type == TYPE_STRING && valid_index?(index, array.items.length)

        block.value = array.items[index].dup
        GET_SUCCESS
      end

      def array_set_string_from_block(array_id, index, block_id)
        block = @blocks[block_id]
        return FAILURE unless block && block.type == TYPE_STRING && block.value

        result = array_set_string(array_id, index, block.value)
        result.negative? ? FAILURE : SET_SUCCESS
      end

      def array_insert_string_from_block(array_id, index, block_id)
        block = @blocks[block_id]
        return FAILURE unless block && block.type == TYPE_STRING && block.value

        array_insert_string(array_id, index, block.value)
      end

      def array_append_string_from_block(array_id, block_id)
        array = @arrays[array_id]
        return FAILURE unless array

        array_insert_string_from_block(array_id, array.items.length, block_id)
      end

      def array_sort_with(id, comparator = nil, &block)
        array = @arrays[id]
        comparator ||= block
        return FAILURE unless array && array.type != TYPE_STRING && comparator

        array.items.sort! { |left, right| comparator.call(left, right) }
        array.index = 0
        array.deck_size = array.items.length
        SUCCESS
      rescue StandardError
        FAILURE
      end

      def array_sort(_id, _predicate = nil)
        FAILURE
      end

      def array_reverse(id)
        array = @arrays[id]
        return FAILURE unless array && array.type != TYPE_STRING

        array.items.reverse!
        array.index = 0
        array.deck_size = array.items.length
        SUCCESS
      end

      def array_shift_left(id, index, positions)
        array_rotate(id, index, positions, false)
      end

      def array_shift_right(id, index, positions)
        array_rotate(id, index, positions, true)
      end

      def array_swap(id, index1, index2)
        array = @arrays[id]
        return FAILURE unless array && array.type != TYPE_STRING &&
                              valid_index?(index1, array.items.length) && valid_index?(index2, array.items.length)

        array.items[index1], array.items[index2] = array.items[index2], array.items[index1]
        array.deck_size = array.items.length
        SUCCESS
      end

      def array_copy(id, source_index, destination_index)
        array = @arrays[id]
        return FAILURE unless array && array.type != TYPE_STRING &&
                              valid_index?(source_index, array.items.length) && valid_index?(destination_index, array.items.length)

        array.items[destination_index] = copy_value(array.items[source_index])
        array.deck_size = array.items.length
        SUCCESS
      end

      def array_shuffle(id, random: Random)
        array = @arrays[id]
        return FAILURE unless array && array.type != TYPE_STRING

        array.items.shuffle!(random: random)
        array.deck_size = array.items.length
        array.index = 0
        SUCCESS
      end

      def array_draw_at(id, index)
        array = @arrays[id]
        return [FAILURE, nil] unless array && array.type != TYPE_STRING && valid_index?(index, array.deck_size)

        last = array.deck_size - 1
        array.items[index], array.items[last] = array.items[last], array.items[index] if index != last
        array.deck_size -= 1
        [GET_SUCCESS, copy_value(array.items[last])]
      end

      def array_draw(id, random: Random)
        array = @arrays[id]
        return [FAILURE, nil] unless array && array.type != TYPE_STRING && !array.items.empty?
        return [FAILURE, nil] if array.deck_size.zero? && array_shuffle(id, random: random) != SUCCESS

        array_draw_at(id, 0)
      end

      def array_draw_to_block(array_id, block_id, expected_size = nil)
        array = @arrays[array_id]
        block = @blocks[block_id]
        return FAILURE unless compatible?(array, block, 0, expected_size)

        status, value = array_draw(array_id)
        return status unless status == GET_SUCCESS

        block.value = value
        GET_SUCCESS
      end

      def array_draw_at_to_block(array_id, index, block_id, expected_size = nil)
        array = @arrays[array_id]
        block = @blocks[block_id]
        return FAILURE unless compatible?(array, block, 0, expected_size)

        status, value = array_draw_at(array_id, index)
        return status unless status == GET_SUCCESS

        block.value = value
        GET_SUCCESS
      end

      def advanced_borrow(id)
        array = @arrays[id]
        return [FAILURE, nil] unless array && array.refcount < 65_535

        array.refcount += 1
        [SUCCESS, ArrayBorrow.new(array)]
      end

      def advanced_release(borrow)
        return FAILURE unless borrow.is_a?(ArrayBorrow) && !borrow.released

        borrow.released = true
        array = borrow.store
        return FAILURE unless @arrays[array.id].equal?(array)

        array.refcount -= 1
        array.refcount.zero? ? array_delete(array.id) : SUCCESS
      end

      def advanced_data(borrow)
        return nil unless valid_borrow?(borrow) && borrow.store.type != TYPE_STRING

        borrow.store.items
      end

      def advanced_get(borrow, index)
        array = borrowed_array(borrow)
        return [FAILURE, nil] unless array && array.type != TYPE_STRING && valid_index?(index, array.items.length)

        [GET_SUCCESS, copy_value(array.items[index])]
      end

      def advanced_set(borrow, index, value)
        array = borrowed_array(borrow)
        return FAILURE unless array && array.type != TYPE_STRING && valid_index?(index, array.items.length)
        return FAILURE unless valid_array_value?(array, value)

        array.items[index] = copy_value(value)
        array.deck_size = array.items.length
        SET_SUCCESS
      end

      def advanced_insert(borrow, index, value)
        array = borrowed_array(borrow)
        return FAILURE unless array && array.type != TYPE_STRING

        array_insert(array.id, index, value)
      end

      def advanced_append(borrow, value)
        array = borrowed_array(borrow)
        return FAILURE unless array && array.type != TYPE_STRING

        array_append(array.id, value)
      end

      def advanced_remove(borrow, index)
        array = borrowed_array(borrow)
        return FAILURE unless array && array.type != TYPE_STRING

        array_remove(array.id, index)
      end

      def advanced_clear(borrow)
        array = borrowed_array(borrow)
        return FAILURE unless array && array.type != TYPE_STRING

        array_clear(array.id)
      end

      def advanced_extend(borrow, additional_capacity)
        array = borrowed_array(borrow)
        return FAILURE unless array

        array_extend(array.id, additional_capacity)
      end

      def advanced_shrink(borrow, new_capacity)
        array = borrowed_array(borrow)
        return FAILURE unless array

        array_shrink(array.id, new_capacity)
      end

      def shutdown
        @arrays.clear
        @blocks.clear
        SUCCESS
      end

      def pool_cleanup
        @blocks.empty? && @arrays.empty? ? SUCCESS : FAILURE
      end

      def managed_stats
        { blocks: @blocks.length, arrays: @arrays.length }
      end

      def value_valid?(type, value, block_size = nil)
        size = block_size || type_size(type)
        return false unless valid_type?(type) && size.is_a?(Integer) && size >= 0

        valid_value?(Block.new(type: type, block_size: size), value)
      end

      private

      def valid_id?(id)
        Base.valid_id?(id)
      end

      def valid_type?(type)
        type.is_a?(Integer) && ((TYPE_STRUCT..TYPE_DATETIME).cover?(type))
      end

      def default_value(type)
        return 0.0 if type == TYPE_FLOAT || type == TYPE_DOUBLE
        return "\0".b * 16 if type == TYPE_UUID

        0
      end

      def copy_value(value, copies = {})
        return value if value.nil? || value == true || value == false || value.is_a?(Numeric) || value.is_a?(Symbol)
        return copies[value.object_id] if copies.key?(value.object_id)

        case value
        when String
          copies[value.object_id] = value.dup
        when Array
          copy = []
          copies[value.object_id] = copy
          value.each { |item| copy << copy_value(item, copies) }
          copy
        when Hash
          copy = {}
          copies[value.object_id] = copy
          value.each { |key, item| copy[copy_value(key, copies)] = copy_value(item, copies) }
          copy
        when Struct
          copy = value.class.new
          copies[value.object_id] = copy
          value.each_pair { |name, item| copy[name] = copy_value(item, copies) }
          copy
        else
          value.dup
        end
      rescue TypeError
        value
      end

      def bounded_string(value, buffer_size = nil)
        return nil unless value.is_a?(::String)
        return nil if buffer_size && (!buffer_size.is_a?(Integer) || buffer_size <= 0)

        bytes = value.b
        bytes = bytes.byteslice(0, buffer_size) if buffer_size
        terminator = bytes.index("\0".b)
        (terminator ? bytes.byteslice(0, terminator) : bytes).dup
      end

      def valid_value?(block, value)
        return true if block.type == TYPE_STRUCT
        return value.is_a?(::String) && value.bytesize == block.block_size if block.type == TYPE_UUID
        return INTEGER_RANGES.fetch(block.type).cover?(value) if INTEGER_RANGES.key?(block.type)
        return value.is_a?(Numeric) if block.type == TYPE_FLOAT || block.type == TYPE_DOUBLE

        false
      end

      def valid_array_value?(array, value)
        return bounded_string(value) != nil if array.type == TYPE_STRING
        return true if array.type == TYPE_STRUCT

        valid_value?(Block.new(type: array.type, block_size: array.block_size), value)
      end

      def valid_index?(index, count)
        index.is_a?(Integer) && index >= 0 && index < count
      end

      def valid_insert_index?(index, count)
        index.is_a?(Integer) && index >= 0 && index <= count
      end

      def ensure_capacity(array, needed)
        return true if needed <= array.capacity
        return false if array.sealed

        capacity = array.capacity.zero? ? 4 : array.capacity
        capacity *= 2 while capacity < needed
        array.capacity = capacity
        true
      end

      def update_cursor_after_remove(array, index)
        count = array.items.length
        if count.zero?
          array.index = 0
        elsif array.index > index
          array.index -= 1
        elsif array.index >= count
          array.index = count - 1
        end
        array.deck_size = count
      end

      def valid_slice?(array, start, length)
        array && start.is_a?(Integer) && length.is_a?(Integer) && start >= 0 && length >= 0 &&
          start <= array.items.length && length <= array.items.length - start
      end

      def compatible?(array, block, index, expected_size = nil, insertion: false)
        return false unless array && block && array.type != TYPE_STRING && block.type != TYPE_STRING
        return false unless (array.type == TYPE_STRUCT || array.type == block.type) && array.block_size == block.block_size
        return false if expected_size && expected_size != array.block_size
        return insertion ? valid_insert_index?(index, array.items.length) : valid_index?(index, array.items.length)
      end

      def array_rotate(id, index, positions, right)
        array = @arrays[id]
        return FAILURE unless array && array.type != TYPE_STRING && valid_index?(index, array.items.length)
        return FAILURE unless positions.is_a?(Integer) && positions >= 0

        suffix = array.items[index..]
        shift = positions % suffix.length
        return SUCCESS if shift.zero?

        rotated = right ? suffix.rotate(-shift) : suffix.rotate(shift)
        array.items[index, suffix.length] = rotated
        array.index = 0
        array.deck_size = array.items.length
        SUCCESS
      end

      def valid_borrow?(borrow)
        borrow.is_a?(ArrayBorrow) && !borrow.released && @arrays[borrow.store.id].equal?(borrow.store)
      end

      def borrowed_array(borrow)
        valid_borrow?(borrow) ? borrow.store : nil
      end
    end
  end
end