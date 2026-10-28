# frozen_string_literal: true

require_relative 'bpa'

module CLM
  module Storage
    FAILURE = BPA::FAILURE
    SUCCESS = BPA::SUCCESS
    GET_SUCCESS = BPA::GET_SUCCESS
    SET_SUCCESS = BPA::SET_SUCCESS
    MAX_ELEMENTS = (1 << 31) - 1
    METADATA_MAGIC = 0x434c_4d53_544f_5247

    StorageInfo = Struct.new(:columns, :rows, :layers, :frame_columns,
                             :frame_rows, :element_size, :type, :atlas,
                             :data_id, keyword_init: true)

    class << self
      def new(id, type, struct_size, columns, rows, layers)
        create(id, type, struct_size, columns, rows, layers, 0, 0, false)
      end

      def new_2d(id, type, struct_size, columns, rows)
        new(id, type, struct_size, columns, rows, 1)
      end

      def atlas_new(id, type, struct_size, columns, rows, frame_columns, frame_rows)
        create(id, type, struct_size, columns, rows, 1, frame_columns, frame_rows, true)
      end

      def atlas_new_3d(id, type, struct_size, columns, rows, layers, frame_columns, frame_rows)
        create(id, type, struct_size, columns, rows, layers, frame_columns, frame_rows, true)
      end

      def get_info(id)
        metadata = read_metadata(id)
        metadata ? [GET_SUCCESS, info_from(metadata)] : [FAILURE, nil]
      end

      def delete(id)
        metadata = read_metadata(id)
        return FAILURE unless metadata
        return FAILURE unless BPA.array_delete(metadata[:data_id]) == SUCCESS

        BPA.delete(id)
      end

      def get_to(id, layer, row, column)
        metadata = read_metadata(id)
        return [FAILURE, nil] unless metadata

        index = element_index(metadata, layer, row, column)
        return [FAILURE, nil] unless index

        BPA.array_get(metadata[:data_id], index)
      end

      def set_from(id, layer, row, column, value, value_size = nil)
        metadata = read_metadata(id)
        return FAILURE unless metadata

        index = element_index(metadata, layer, row, column)
        return FAILURE unless index && valid_value?(metadata, value, value_size)

        BPA.array_set(metadata[:data_id], index, value)
      end

      def get(id, layer, row, column, destination_block_id)
        status, value = get_to(id, layer, row, column)
        return status unless status == GET_SUCCESS

        metadata = read_metadata(id)
        block_status, block = BPA.block_info(destination_block_id)
        return FAILURE unless metadata && block_status == GET_SUCCESS &&
                              block[:type] == metadata[:type] &&
                              block[:block_size] == metadata[:element_size]

        BPA.set(destination_block_id, value) == SET_SUCCESS ? GET_SUCCESS : FAILURE
      end

      def set(id, layer, row, column, source_block_id)
        metadata = read_metadata(id)
        block_status, block = BPA.block_info(source_block_id)
        return FAILURE unless metadata && block_status == GET_SUCCESS &&
                              block[:type] == metadata[:type] &&
                              block[:block_size] == metadata[:element_size]

        status, value = BPA.get(source_block_id)
        return FAILURE unless status == GET_SUCCESS

        set_from(id, layer, row, column, value, metadata[:element_size]) == SET_SUCCESS ? SET_SUCCESS : FAILURE
      end

      def fill_from(id, value, value_size = nil)
        metadata = read_metadata(id)
        return FAILURE unless metadata && valid_value?(metadata, value, value_size)

        status, borrow = BPA.advanced_borrow(metadata[:data_id])
        return FAILURE unless status == SUCCESS

        begin
          array = borrow.store
          array.items.length.times do |index|
            return FAILURE unless BPA.advanced_set(borrow, index, value) == SET_SUCCESS
          end
        ensure
          BPA.advanced_release(borrow)
        end
        SUCCESS
      end

      def slice(source_id, layer, row, column, layers, rows, columns, destination_id)
        source = read_metadata(source_id)
        return FAILURE unless source && positive_dimensions?(layers, rows, columns)
        return FAILURE unless valid_region?(source, layer, row, column, layers, rows, columns)

        create(destination_id, source[:type], source[:element_size], columns, rows, layers, 0, 0, false).tap do |status|
          return FAILURE unless status == SUCCESS
        end

        layers.times do |z|
          rows.times do |y|
            columns.times do |x|
              status, value = get_to(source_id, layer + z, row + y, column + x)
              return rollback(destination_id) unless status == GET_SUCCESS
              return rollback(destination_id) unless set_from(destination_id, z, y, x, value, source[:element_size]) == SET_SUCCESS
            end
          end
        end
        SUCCESS
      end

      def atlas_get_frame_to(id, layer, frame_row, frame_column)
        metadata = read_metadata(id)
        rect = atlas_rect(metadata, layer, frame_row, frame_column)
        return [FAILURE, nil] unless rect

        top, left = rect
        values = []
        metadata[:frame_rows].times do |y|
          metadata[:frame_columns].times do |x|
            status, value = get_to(id, layer, top + y, left + x)
            return [FAILURE, nil] unless status == GET_SUCCESS
            values << value
          end
        end
        [GET_SUCCESS, values]
      end

      def atlas_set_frame_from(id, layer, frame_row, frame_column, values, value_size = nil)
        metadata = read_metadata(id)
        rect = atlas_rect(metadata, layer, frame_row, frame_column)
        return FAILURE unless rect && values.is_a?(Array)

        expected = metadata[:frame_columns] * metadata[:frame_rows]
        return FAILURE unless values.length == expected
        return FAILURE unless values.all? { |value| valid_value?(metadata, value, value_size) }

        top, left = rect
        index = 0
        metadata[:frame_rows].times do |y|
          metadata[:frame_columns].times do |x|
            status = set_from(id, layer, top + y, left + x, values[index], metadata[:element_size])
            return FAILURE unless status == SET_SUCCESS
            index += 1
          end
        end
        SET_SUCCESS
      end

      def atlas_slice_frame(id, layer, frame_row, frame_column, destination_id)
        metadata = read_metadata(id)
        rect = atlas_rect(metadata, layer, frame_row, frame_column)
        return FAILURE unless rect

        top, left = rect
        slice(id, layer, top, left, 1, metadata[:frame_rows], metadata[:frame_columns], destination_id)
      end

      private

      def create(id, type, struct_size, columns, rows, layers,
                 frame_columns, frame_rows, atlas)
        return FAILURE unless Base.valid_id?(id) && !BPA.block_existing?(id) && !BPA.array_existing?(id)
        return FAILURE unless positive_dimensions?(columns, rows, layers)
        return FAILURE if type == BPA::TYPE_STRING

        if atlas
          return FAILURE unless positive_dimensions?(frame_columns, frame_rows, 1)
          return FAILURE unless (columns % frame_columns).zero? && (rows % frame_rows).zero?
        elsif !frame_columns.zero? || !frame_rows.zero?
          return FAILURE
        end

        element_size = if type == BPA::TYPE_STRUCT
                         return FAILURE unless struct_size.is_a?(Integer) && struct_size.positive?
                         struct_size
                       else
                         size = BPA.type_size(type)
                         return FAILURE if size.zero? || (!struct_size.zero? && struct_size != size)
                         size
                       end

        total = columns * rows * layers
        return FAILURE if total > MAX_ELEMENTS || total * element_size > (1 << 63) - 1

        data_id = allocate_data_id(id)
        return FAILURE unless data_id
        return FAILURE unless BPA.create_array_sized(data_id, type, total, element_size) == SUCCESS

        initial = default_value(type, element_size)
        total.times do
          unless BPA.array_append(data_id, initial) == SUCCESS
            BPA.array_delete(data_id)
            return FAILURE
          end
        end

        metadata = {
          magic: METADATA_MAGIC,
          data_id: data_id,
          columns: columns,
          rows: rows,
          layers: layers,
          frame_columns: frame_columns,
          frame_rows: frame_rows,
          element_size: element_size,
          type: type,
          atlas: atlas
        }
        unless BPA.create_sized(id, BPA::TYPE_STRUCT, 1) == SUCCESS && BPA.set(id, metadata) == SET_SUCCESS
          BPA.array_delete(data_id)
          BPA.delete(id)
          return FAILURE
        end
        SUCCESS
      end

      def read_metadata(id)
        status, metadata = BPA.get(id)
        return nil unless status == GET_SUCCESS && metadata.is_a?(Hash) && metadata[:magic] == METADATA_MAGIC

        array_status, array = BPA.array_info(metadata[:data_id])
        return nil unless array_status == GET_SUCCESS && array[:type] == metadata[:type] &&
                          array[:block_size] == metadata[:element_size]

        metadata
      end

      def info_from(metadata)
        StorageInfo.new(columns: metadata[:columns], rows: metadata[:rows],
                        layers: metadata[:layers], frame_columns: metadata[:frame_columns],
                        frame_rows: metadata[:frame_rows], element_size: metadata[:element_size],
                        type: metadata[:type], atlas: metadata[:atlas], data_id: metadata[:data_id])
      end

      def positive_dimensions?(*dimensions)
        dimensions.all? { |dimension| dimension.is_a?(Integer) && dimension.positive? }
      end

      def valid_value?(metadata, value, value_size)
        return false if value_size && value_size != metadata[:element_size]

        BPA.value_valid?(metadata[:type], value, metadata[:element_size])
      end

      def default_value(type, element_size)
        return 0.0 if type == BPA::TYPE_FLOAT || type == BPA::TYPE_DOUBLE
        return "\0".b * 16 if type == BPA::TYPE_UUID
        return "\0".b * element_size if type == BPA::TYPE_STRUCT

        0
      end

      def element_index(metadata, layer, row, column)
        dimensions = [layer, row, column]
        return nil unless dimensions.all? { |value| value.is_a?(Integer) && value >= 0 }
        return nil if layer >= metadata[:layers] || row >= metadata[:rows] || column >= metadata[:columns]

        (layer * metadata[:rows] + row) * metadata[:columns] + column
      end

      def valid_region?(metadata, layer, row, column, layers, rows, columns)
        values = [layer, row, column, layers, rows, columns]
        return false unless values.all? { |value| value.is_a?(Integer) && value >= 0 }
        return false if layer >= metadata[:layers] || row >= metadata[:rows] || column >= metadata[:columns]

        layers <= metadata[:layers] - layer && rows <= metadata[:rows] - row && columns <= metadata[:columns] - column
      end

      def atlas_rect(metadata, layer, frame_row, frame_column)
        return nil unless metadata && metadata[:atlas]
        return nil unless [layer, frame_row, frame_column].all? { |value| value.is_a?(Integer) && value >= 0 }
        return nil if layer >= metadata[:layers] || frame_row >= metadata[:rows] / metadata[:frame_rows] ||
                      frame_column >= metadata[:columns] / metadata[:frame_columns]

        [frame_row * metadata[:frame_rows], frame_column * metadata[:frame_columns]]
      end

      def allocate_data_id(forbidden)
        candidate = Base::ID_MAX
        while candidate.positive?
          return candidate unless candidate == forbidden || BPA.block_existing?(candidate) || BPA.array_existing?(candidate)
          candidate -= 1
        end
        nil
      end

      def rollback(id)
        delete(id)
        FAILURE
      end
    end
  end
end