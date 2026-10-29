# frozen_string_literal: true
# CLM Ruby file port. Experimental native subset; file IDs retired until shutdown.
module CLMFile
  SUCCESS = 0; FAILURE = -1; UNSUPPORTED = -2
  READ = 1; WRITE = 2; APPEND = 4; CREATE = 8; TRUNCATE = 16; EXCLUSIVE = 32
  @registry = {}; @retired = {}
  class << self
    def open_as(id, path, mode)
      return FAILURE if @registry.key?(id) || @retired.key?(id) || path.nil? || path.empty? || (mode & 7).zero? || (mode & ~63) != 0
      return FAILURE if (mode & EXCLUSIVE != 0 && mode & CREATE == 0) || (mode & TRUNCATE != 0 && (mode & WRITE == 0 || mode & APPEND != 0))
      flags = (mode & READ != 0 && mode & (WRITE | APPEND) != 0) ? File::RDWR : ((mode & (WRITE | APPEND) != 0) ? File::WRONLY : File::RDONLY)
      flags |= File::CREAT if mode & CREATE != 0
      flags |= File::EXCL if mode & EXCLUSIVE != 0
      flags |= File::TRUNC if mode & TRUNCATE != 0
      flags |= File::APPEND if mode & APPEND != 0
      flags |= File::NOFOLLOW if defined?(File::NOFOLLOW)
      f = File.open(path, flags, 0o600)
      unless f.stat.file? then f.close; return FAILURE end
      @registry[id] = [f, mode]; SUCCESS
    rescue SystemCallError, ArgumentError
      FAILURE
    end
    alias open open_as
    def close(id)
      pair = @registry.delete(id)
      return FAILURE unless pair
      @retired[id] = true; pair[0].close; SUCCESS
    rescue SystemCallError
      FAILURE
    end
    def shutdown
      status = SUCCESS
      @registry.keys.each { |id| status = FAILURE unless close(id) == SUCCESS }
      @retired.clear; status
    end
    def is_open(id) = @registry.key?(id) ? 1 : 0
    def read_to(id, size)
      pair = @registry[id]
      return [FAILURE, ''.b] if !pair || pair[1] & READ == 0 || size.negative?
      [SUCCESS, pair[0].read(size) || ''.b]
    rescue IOError, SystemCallError
      [FAILURE, ''.b]
    end
    def write_from(id, data)
      pair = @registry[id]
      return [FAILURE, 0] unless pair && pair[1] & (WRITE | APPEND) != 0
      [SUCCESS, pair[0].write(data)]
    rescue IOError, SystemCallError
      [FAILURE, 0]
    end
    def seek(id, offset, whence = IO::SEEK_SET)
      return FAILURE unless @registry[id]
      @registry[id][0].seek(offset, whence); SUCCESS
    rescue IOError, SystemCallError
      FAILURE
    end
    def tell(id)
      return [FAILURE, 0] unless @registry[id]
      [SUCCESS, @registry[id][0].pos]
    rescue IOError, SystemCallError
      [FAILURE, 0]
    end
    def exists_as(path) = File.file?(path) ? 1 : 0
    def read_all_to(path) = [SUCCESS, File.binread(path)]
    def write_all_from(path, data) = (File.binwrite(path, data); SUCCESS)
    def append_from(path, data) = (File.open(path, 'ab') { |f| f.write(data) }; SUCCESS)
    def delete_as(path) = (File.delete(path); SUCCESS)
  end
end
