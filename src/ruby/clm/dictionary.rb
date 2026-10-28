# frozen_string_literal: true

require_relative 'bpa'

module CLM
  module Dictionary
    FAILURE = BPA::FAILURE
    SUCCESS = BPA::SUCCESS
    GET_SUCCESS = BPA::GET_SUCCESS
    SET_SUCCESS = BPA::SET_SUCCESS
    METADATA_MAGIC = 0x434c_4d44_4943_544e
    MAX_COUNT = (1 << 31) - 1

    class << self
      def exists?(id)
        read_metadata(id).first == SUCCESS
      end

      def new(id, value_type, size = 0)
        return FAILURE unless valid_id?(id) && !exists?(id)
        return FAILURE unless valid_type?(value_type) && size.is_a?(Integer) && size >= 0

        value_size = BPA.type_size(value_type)
        if value_type == BPA::TYPE_STRING
          return FAILURE unless size.zero?
          value_size = 0
        elsif value_type == BPA::TYPE_STRUCT
          return FAILURE if size.zero?
          value_size = size
        elsif value_size.zero? || (!size.zero? && size != value_size)
          return FAILURE
        end

        keys_id = allocate_array_id(id, nil)
        return FAILURE unless keys_id && BPA.create_array_sized(keys_id, BPA::TYPE_STRUCT, 0, 8) == SUCCESS

        values_id = allocate_array_id(id, keys_id)
        unless values_id && BPA.create_array_sized(values_id, value_type, 0, value_size) == SUCCESS
          BPA.array_delete(keys_id)
          return FAILURE
        end

        metadata = {
          magic: METADATA_MAGIC,
          keys_id: keys_id,
          values_id: values_id,
          value_type: value_type,
          value_size: value_size,
          count: 0,
          iterator: 0
        }
        unless BPA.create_sized(id, BPA::TYPE_STRUCT, 1) == SUCCESS &&
               BPA.set(id, metadata) == BPA::SET_SUCCESS
          BPA.delete(id)
          BPA.array_delete(values_id)
          BPA.array_delete(keys_id)
          return FAILURE
        end

        SUCCESS
      end

      def delete(id)
        status, metadata = read_metadata(id)
        return FAILURE unless status == SUCCESS

        keys_status = BPA.array_delete(metadata[:keys_id])
        values_status = BPA.array_delete(metadata[:values_id])
        block_status = BPA.delete(id)
        keys_status == SUCCESS && values_status == SUCCESS && block_status == SUCCESS ? SUCCESS : FAILURE
      end

      def set_from(id, key, value, value_size = nil)
        status, metadata = read_metadata(id)
        return FAILURE unless status == SUCCESS && valid_id?(key)
        value_size ||= metadata[:value_size] unless metadata[:value_type] == BPA::TYPE_STRING
        return FAILURE unless valid_value?(metadata, value, value_size)

        found, index = find_key(metadata, key)
        if found
          result = set_array_value(metadata, index, value, value_size)
          return result < 0 ? FAILURE : SET_SUCCESS
        end

        count_status, count = BPA.array_count(metadata[:keys_id])
        return FAILURE unless count_status == GET_SUCCESS && count < MAX_COUNT

        key_result = BPA.array_append(metadata[:keys_id], key)
        return FAILURE unless key_result == SUCCESS

        value_result = append_array_value(metadata, value, value_size)
        if value_result != SUCCESS
          BPA.array_remove_last(metadata[:keys_id])
          return FAILURE
        end

        metadata[:count] = count + 1
        metadata[:iterator] = 0
        write_metadata(id, metadata) == SUCCESS ? SET_SUCCESS : FAILURE
      end

      def set(id, key, source_block_id)
        status, metadata = read_metadata(id)
        source_status, source = BPA.block_info(source_block_id)
        return FAILURE unless status == SUCCESS && source_status == GET_SUCCESS
        return FAILURE unless source[:type] == metadata[:value_type]
        return FAILURE if source[:type] != BPA::TYPE_STRING && source[:block_size] != metadata[:value_size]

        if source[:type] == BPA::TYPE_STRING
          result, value = BPA.get_string(source_block_id)
          return FAILURE if result.negative?
          set_from(id, key, value, value.bytesize + 1)
        else
          result, value = BPA.get(source_block_id)
          return FAILURE unless result == GET_SUCCESS
          set_from(id, key, value, source[:block_size])
        end
      end

      def get_to(id, key)
        status, metadata = read_metadata(id)
        return [FAILURE, nil] unless status == SUCCESS && valid_id?(key)

        found, index = find_key(metadata, key)
        return [FAILURE, nil] unless found

        if metadata[:value_type] == BPA::TYPE_STRING
          value_status, value = BPA.array_get_string(metadata[:values_id], index)
          value_status.negative? ? [FAILURE, nil] : [GET_SUCCESS, value]
        else
          BPA.array_get(metadata[:values_id], index)
        end
      end

      def get(id, key, destination_block_id)
        status, metadata = read_metadata(id)
        destination_status, destination = BPA.block_info(destination_block_id)
        return FAILURE unless status == SUCCESS && destination_status == GET_SUCCESS
        return FAILURE unless destination[:type] == metadata[:value_type]
        return FAILURE if metadata[:value_type] != BPA::TYPE_STRING && destination[:block_size] != metadata[:value_size]

        value_status, value = get_to(id, key)
        return FAILURE unless value_status == GET_SUCCESS

        if metadata[:value_type] == BPA::TYPE_STRING
          result = BPA.set_string(destination_block_id, value)
          result.negative? ? FAILURE : GET_SUCCESS
        else
          BPA.set(destination_block_id, value) == SET_SUCCESS ? GET_SUCCESS : FAILURE
        end
      end

      def contains?(id, key)
        status, metadata = read_metadata(id)
        return false unless status == SUCCESS && valid_id?(key)

        find_key(metadata, key).first
      end

      def count(id)
        status, metadata = read_metadata(id)
        status == SUCCESS ? metadata[:count] : FAILURE
      end

      def remove(id, key)
        status, metadata = read_metadata(id)
        return FAILURE unless status == SUCCESS && valid_id?(key)

        found, index = find_key(metadata, key)
        return FAILURE unless found

        return FAILURE unless BPA.array_remove(metadata[:values_id], index) == SUCCESS &&
                              BPA.array_remove(metadata[:keys_id], index) == SUCCESS

        metadata[:count] -= 1
        metadata[:iterator] = 0
        write_metadata(id, metadata)
      end

      def iterate_reset(id)
        status, metadata = read_metadata(id)
        return FAILURE unless status == SUCCESS

        metadata[:iterator] = 0
        write_metadata(id, metadata)
      end

      def iterate_next(id)
        status, metadata = read_metadata(id)
        return [FAILURE, nil] unless status == SUCCESS && metadata[:iterator] < metadata[:count]

        key_status, key = BPA.array_get(metadata[:keys_id], metadata[:iterator])
        return [FAILURE, nil] unless key_status == GET_SUCCESS

        metadata[:iterator] += 1
        return [FAILURE, nil] unless write_metadata(id, metadata) == SUCCESS

        [GET_SUCCESS, key]
      end

      def clear(id)
        status, metadata = read_metadata(id)
        return FAILURE unless status == SUCCESS
        return FAILURE unless BPA.array_clear(metadata[:values_id]) == SUCCESS &&
                              BPA.array_clear(metadata[:keys_id]) == SUCCESS

        metadata[:count] = 0
        metadata[:iterator] = 0
        write_metadata(id, metadata)
      end

      def add_from(id, key, value, value_size = nil)
        return FAILURE if contains?(id, key)

        set_from(id, key, value, value_size)
      end

      def update_from(id, key, value, value_size = nil)
        return FAILURE unless contains?(id, key)

        set_from(id, key, value, value_size)
      end

      def keys(id)
        status, metadata = read_metadata(id)
        return [FAILURE, nil] unless status == SUCCESS

        values = []
        metadata[:count].times do |index|
          item_status, key = BPA.array_get(metadata[:keys_id], index)
          return [FAILURE, nil] unless item_status == GET_SUCCESS
          values << key
        end
        [GET_SUCCESS, values]
      end

      private

      def valid_id?(id)
        Base.valid_id?(id)
      end

      def valid_type?(type)
        type.is_a?(Integer) && (BPA::TYPE_STRUCT..BPA::TYPE_DATETIME).cover?(type)
      end

      def read_metadata(id)
        status, metadata = BPA.get(id)
        return [FAILURE, nil] unless status == GET_SUCCESS && metadata.is_a?(Hash) && metadata[:magic] == METADATA_MAGIC
        return [FAILURE, nil] unless BPA.array_existing?(metadata[:keys_id]) && BPA.array_existing?(metadata[:values_id])

        [SUCCESS, metadata]
      end

      def write_metadata(id, metadata)
        BPA.set(id, metadata) == SET_SUCCESS ? SUCCESS : FAILURE
      end

      def allocate_array_id(dictionary_id, other_id)
        candidate = Base::ID_MAX
        loop do
          return nil if candidate.zero?
          unless candidate == dictionary_id || candidate == other_id ||
                 BPA.array_existing?(candidate) || BPA.block_existing?(candidate)
            return candidate
          end
          candidate -= 1
        end
      end

      def find_key(metadata, key)
        metadata[:count].times do |index|
          status, stored_key = BPA.array_get(metadata[:keys_id], index)
          return [false, nil] unless status == GET_SUCCESS
          return [true, index] if stored_key == key
        end
        [false, nil]
      end

      def valid_value?(metadata, value, value_size)
        if metadata[:value_type] == BPA::TYPE_STRING
          return false unless value.is_a?(::String)
          return false unless value_size.nil? || (value_size.is_a?(Integer) && value_size.positive?)
          true
        elsif metadata[:value_type] == BPA::TYPE_STRUCT
          value_size.nil? || value_size == metadata[:value_size]
        else
          !value_size.nil? && value_size == metadata[:value_size] &&
            BPA.value_valid?(metadata[:value_type], value, metadata[:value_size])
        end
      end

      def append_array_value(metadata, value, value_size)
        if metadata[:value_type] == BPA::TYPE_STRING
          BPA.array_append_string_value(metadata[:values_id], value, value_size) == SUCCESS ? SUCCESS : FAILURE
        else
          BPA.array_append(metadata[:values_id], value)
        end
      end

      def set_array_value(metadata, index, value, value_size)
        if metadata[:value_type] == BPA::TYPE_STRING
          BPA.array_set_string(metadata[:values_id], index, value, value_size)
        else
          BPA.array_set(metadata[:values_id], index, value)
        end
      end
    end
  end
end