# frozen_string_literal: true
# CLM Ruby directory port. Experimental native subset.
require 'fileutils'
require 'tmpdir'
module CLMDirectory
  SUCCESS=0; FAILURE=-1; UNSUPPORTED=-2; STOPPED=1
  class << self
    def exists_as(path) = File.directory?(path) ? 1 : 0
    def create_as(path) = (Dir.mkdir(path, 0o700); SUCCESS)
    def create_recursive_as(path) = (FileUtils.mkdir_p(path, mode: 0o700); SUCCESS)
    def delete_as(path) = (Dir.rmdir(path); SUCCESS)
    def safe?(path)
      absolute = File.expand_path(path)
      absolute != File::SEPARATOR && absolute != File.expand_path('..', absolute) && !File.symlink?(absolute)
    end
    def delete_recursive_as(path)
      return FAILURE unless safe?(path)
      FileUtils.remove_entry_secure(path); SUCCESS
    end
    def clear_as(path)
      return FAILURE unless safe?(path)
      Dir.children(path).each { |name| FileUtils.remove_entry_secure(File.join(path, name)) }; SUCCESS
    end
    def move_as(from, to)
      return FAILURE if File.exist?(to) || File.symlink?(to)
      File.rename(from, to); SUCCESS
    end
    def find_entries_as(path) = [SUCCESS, Dir.children(path).map { |name| File.join(path, name) }]
    def find_files_as(path) = [SUCCESS, Dir.children(path).map { |name| File.join(path, name) }.select { |p| File.file?(p) && !File.symlink?(p) }]
    def find_directories_as(path) = [SUCCESS, Dir.children(path).map { |name| File.join(path, name) }.select { |p| File.directory?(p) && !File.symlink?(p) }]
    def is_empty_as(path) = [SUCCESS, Dir.empty?(path)]
    def get_current_to = [SUCCESS, Dir.pwd]
    def set_current_as(path) = (Dir.chdir(path); SUCCESS)
    def get_temporary_to = [SUCCESS, Dir.tmpdir]
    def create_temporary_to(path) = [SUCCESS, Dir.mktmpdir('clm-', path)]
    def get_size_as(path)
      size=0
      Dir.glob(File.join(path, '**', '*'), File::FNM_DOTMATCH).each { |p| size += File.size(p) if File.file?(p) && !File.symlink?(p) }
      [SUCCESS,size]
    end
  end
end
