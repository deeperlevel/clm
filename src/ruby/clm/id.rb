# frozen_string_literal: true

require_relative 'dictionary'

module CLM
  module ID
    FAILURE = BPA::FAILURE
    SUCCESS = BPA::SUCCESS
    GET_SUCCESS = BPA::GET_SUCCESS
    SET_SUCCESS = BPA::SET_SUCCESS
    ID_STORAGE_ID = 0x0F00_0001
    INDEX_STORAGE_ID = 0x0F00_0002
    NAME_BUFFER_ID = 0x0F00_0003
    NAMES_STORAGE_ID = 0x0F00_0004
    VALUES_STORAGE_ID = 0x0F00_0005
    NOT_FOUND = Base::ID_MAX
    HASH_MASK = (1 << 32) - 1

    @generator_value = 0
    @generator_exhausted = false
    @next_entry_id = 0
    @active_index_id = ID_STORAGE_ID
    @case_sensitive = true

    class << self
      def register(name, id)
        return FAILURE unless valid_name?(name) && Base.valid_id?(id) && ensure_storage

        found, slot_key, = find_name(@active_index_id, name, @case_sensitive)
        return FAILURE if found.nil? || found

        entry_id = allocate_entry_id
        return FAILURE unless entry_id

        unless Dictionary.set_from(NAMES_STORAGE_ID, entry_id, name, name.bytesize + 1) == BPA::SET_SUCCESS
          return FAILURE
        end
        unless Dictionary.set_from(VALUES_STORAGE_ID, entry_id, id, 8) == BPA::SET_SUCCESS
          Dictionary.remove(NAMES_STORAGE_ID, entry_id)
          return FAILURE
        end
        unless Dictionary.set_from(@active_index_id, slot_key, entry_id, 8) == BPA::SET_SUCCESS
          Dictionary.remove(VALUES_STORAGE_ID, entry_id)
          Dictionary.remove(NAMES_STORAGE_ID, entry_id)
          return FAILURE
        end

        SET_SUCCESS
      end

      alias map register

      def get(name)
        status, id = try_get(name)
        status == GET_SUCCESS ? id : NOT_FOUND
      end

      def try_get(name)
        return [FAILURE, nil] unless valid_name?(name) && ensure_storage

        found, _slot_key, entry_id = find_name(@active_index_id, name, @case_sensitive)
        return [FAILURE, nil] unless found

        Dictionary.get_to(VALUES_STORAGE_ID, entry_id)
      end

      def set(name, id)
        return FAILURE unless valid_name?(name) && Base.valid_id?(id) && ensure_storage

        found, _slot_key, entry_id = find_name(@active_index_id, name, @case_sensitive)
        return FAILURE if found.nil?
        return register(name, id) unless found

        Dictionary.set_from(VALUES_STORAGE_ID, entry_id, id, 8)
      end

      def remove(name)
        return FAILURE unless valid_name?(name) && ensure_storage

        found, slot_key, entry_id = find_name(@active_index_id, name, @case_sensitive)
        return FAILURE unless found
        return FAILURE unless Dictionary.set_from(@active_index_id, slot_key, NOT_FOUND, 8) == SET_SUCCESS
        return FAILURE unless Dictionary.remove(VALUES_STORAGE_ID, entry_id) == SUCCESS

        Dictionary.remove(NAMES_STORAGE_ID, entry_id)
      end

      def unmap(id)
        return FAILURE unless Base.valid_id?(id) && ensure_storage

        removed = false
        loop do
          status, keys = Dictionary.keys(NAMES_STORAGE_ID)
          return FAILURE unless status == GET_SUCCESS

          matching_name = nil
          keys.each do |entry_id|
            value_status, stored_id = Dictionary.get_to(VALUES_STORAGE_ID, entry_id)
            return FAILURE unless value_status == GET_SUCCESS
            if stored_id == id
              name_status, name = Dictionary.get_to(NAMES_STORAGE_ID, entry_id)
              return FAILURE unless name_status == GET_SUCCESS
              matching_name = name
              break
            end
          end
          break unless matching_name
          return FAILURE unless remove(matching_name) == SUCCESS

          removed = true
        end
        removed ? SUCCESS : FAILURE
      end

      alias unmap_name remove

      def case_sensitive?
        @case_sensitive
      end

      def set_case_sensitive(enabled)
        return FAILURE unless enabled == true || enabled == false
        return FAILURE unless ensure_storage
        return SUCCESS if enabled == @case_sensitive

        next_index = @active_index_id == ID_STORAGE_ID ? INDEX_STORAGE_ID : ID_STORAGE_ID
        return FAILURE unless Dictionary.clear(next_index) == SUCCESS

        status, entry_ids = Dictionary.keys(NAMES_STORAGE_ID)
        return FAILURE unless status == GET_SUCCESS

        entry_ids.each do |entry_id|
          name_status, name = Dictionary.get_to(NAMES_STORAGE_ID, entry_id)
          return rollback_index(next_index) unless name_status == GET_SUCCESS

          found, slot_key, = find_name(next_index, name, enabled)
          return rollback_index(next_index) if found.nil? || found
          unless Dictionary.set_from(next_index, slot_key, entry_id, 8) == SET_SUCCESS
            return rollback_index(next_index)
          end
        end

        @active_index_id = next_index
        @case_sensitive = enabled
        SUCCESS
      end

      def clear
        return FAILURE unless ensure_storage

        [ID_STORAGE_ID, INDEX_STORAGE_ID, NAMES_STORAGE_ID, VALUES_STORAGE_ID].each do |dictionary_id|
          return FAILURE unless Dictionary.clear(dictionary_id) == SUCCESS
        end
        @active_index_id = ID_STORAGE_ID
        @next_entry_id = 0
        SUCCESS
      end

      def shutdown
        return FAILURE unless storage_present?

        [VALUES_STORAGE_ID, NAMES_STORAGE_ID, INDEX_STORAGE_ID, ID_STORAGE_ID].each do |dictionary_id|
          return FAILURE unless Dictionary.delete(dictionary_id) == SUCCESS
        end
        return FAILURE unless BPA.delete(NAME_BUFFER_ID) == SUCCESS

        @active_index_id = ID_STORAGE_ID
        @next_entry_id = 0
        SUCCESS
      end

      def gen_reset
        @generator_value = 0
        @generator_exhausted = false
        nil
      end

      def gen_next
        return NOT_FOUND if @generator_exhausted

        result = @generator_value
        if result == NOT_FOUND - 1
          @generator_exhausted = true
        else
          @generator_value += 1
        end
        result
      end

      private

      def valid_name?(name)
        return false unless name.is_a?(::String) && !name.empty? && !name.include?("\0")
        return false unless name.encoding.ascii_compatible?

        name.dup.force_encoding(Encoding::UTF_8).valid_encoding?
      end

      def storage_present?
        [ID_STORAGE_ID, INDEX_STORAGE_ID, NAMES_STORAGE_ID, VALUES_STORAGE_ID].all? do |id|
          Dictionary.exists?(id)
        end && BPA.block_existing?(NAME_BUFFER_ID)
      end

      def ensure_storage
        return true if storage_present?
        return false if [ID_STORAGE_ID, INDEX_STORAGE_ID, NAMES_STORAGE_ID, VALUES_STORAGE_ID, NAME_BUFFER_ID].any? do |id|
          BPA.block_existing?(id) || BPA.array_existing?(id)
        end

        created = []
        specifications = [
          [ID_STORAGE_ID, BPA::TYPE_STRUCT, 8],
          [INDEX_STORAGE_ID, BPA::TYPE_STRUCT, 8],
          [NAMES_STORAGE_ID, BPA::TYPE_STRING, 0],
          [VALUES_STORAGE_ID, BPA::TYPE_STRUCT, 8]
        ]
        specifications.each do |id, type, size|
          return rollback_storage(created) unless Dictionary.new(id, type, size) == SUCCESS
          created << [:dictionary, id]
        end
        unless BPA.create(NAME_BUFFER_ID, BPA::TYPE_STRING) == SUCCESS
          return rollback_storage(created)
        end
        created << [:block, NAME_BUFFER_ID]
        @active_index_id = ID_STORAGE_ID
        @next_entry_id = 0
        true
      end

      def rollback_storage(created)
        created.reverse_each do |kind, id|
          kind == :dictionary ? Dictionary.delete(id) : BPA.delete(id)
        end
        false
      end

      def rollback_index(index_id)
        Dictionary.clear(index_id)
        FAILURE
      end

      def allocate_entry_id
        count = Dictionary.count(NAMES_STORAGE_ID)
        return nil if count.negative?

        (count + 1).times do
          candidate = @next_entry_id
          @next_entry_id = candidate == NOT_FOUND - 1 ? 0 : candidate + 1
          return candidate if candidate != NOT_FOUND && !Dictionary.contains?(NAMES_STORAGE_ID, candidate)
        end
        nil
      end

      def find_name(index_id, name, case_sensitive)
        count = Dictionary.count(index_id)
        return [nil, nil, nil] if count.negative?

        hash = hash_name(name, case_sensitive)
        first_tombstone = nil
        (count + 1).times do |probe|
          slot_key = hash + probe
          status, entry_id = Dictionary.get_to(index_id, slot_key)
          if status != GET_SUCCESS
            return [false, first_tombstone || slot_key, nil]
          end
          if entry_id == NOT_FOUND
            first_tombstone ||= slot_key
            next
          end

          name_status, stored_name = Dictionary.get_to(NAMES_STORAGE_ID, entry_id)
          return [nil, nil, nil] unless name_status == GET_SUCCESS
          return [true, slot_key, entry_id] if names_equal?(stored_name, name, case_sensitive)
        end
        first_tombstone ? [false, first_tombstone, nil] : [nil, nil, nil]
      end

      def hash_name(name, case_sensitive)
        bytes = case_sensitive ? name.b.bytes : name.tr('A-Z', 'a-z').b.bytes
        bytes.reduce(2_166_136_261) { |hash, byte| ((hash ^ byte) * 16_777_619) & HASH_MASK }
      end

      def names_equal?(left, right, case_sensitive)
        case_sensitive ? left.b == right.b : left.tr('A-Z', 'a-z').b == right.tr('A-Z', 'a-z').b
      end
    end
  end
end