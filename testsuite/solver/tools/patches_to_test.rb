#
# patches_to_test --arch=<arch> [--output=<output.xml>] /patch/to/update/repo
#
require 'getoptlong'

class Options
  attr_reader :arch, :base, :path, :output

  def initialize
    opts = GetoptLong.new(
      [ "--arch", "-a",     GetoptLong::REQUIRED_ARGUMENT ],
      [ "--base", "-b",     GetoptLong::REQUIRED_ARGUMENT ],
      [ "--debug", "-d",    GetoptLong::NO_ARGUMENT ],
      [ "--output", "-o",   GetoptLong::REQUIRED_ARGUMENT ]
    )
    @base = Array.new
    opts.each do |opt, arg|
      case opt
        when "--arch": @arch = arg
        when "--base": @base << arg
        when "--debug": @debug = true
        when "--output": @output = arg
      end
    end
    STDERR.puts "Required argument --arch missing" unless @arch
    STDERR.puts "Must give at least one --base" unless @base.size > 0
    @path = ARGV.shift
    STDERR.puts "Required argument <path-to-repo> missing" unless @path
  end
end

require 'rexml/document'

class Repomd
  attr_reader :primary, :patches, :filelists, :other
  def initialize( options )
    doc = REXML::Document.new( File.open( options.path + "/repodata/repomd.xml" ) )
    r = doc.elements["repomd"]
    r.each_element do |e|
      type = e.attributes['type']
      next unless type
      location = e.elements['location']
      STDERR.puts "No location for repomd type #{type}" unless location
      href = location.attributes['href']
      case type
        when "patches":   @patches = href
        when "other":	  @other = href
        when "primary":   @primary = href
        when "filelists": @filelists = href
        else
          STDERR.puts "Unknown repomd type: #{type}"
      end
    end
  end
end


class Patches
  attr_reader :patches
  def initialize( path, options )
    doc = REXML::Document.new( File.open( options.path + "/" + path ) )
    r = doc.elements["patches"]
    @patches = Array.new
    r.each_element do |e|
      location = e.elements['location']
      STDERR.puts "No location for patch id #{e.attributes['id']}" unless location
      href = location.attributes['href']
      @patches << href
    end
  end
end


require 'pathname'

options = Options.new
exit 1 unless options.path
repomd = Repomd.new( options )
patches = Patches.new( repomd.patches, options )

puts "<?xml version=\"1.0\"?>"
puts "<test>"
puts "<setup arch=\"#{options.arch}\">"
i = 0
options.base.each do |b|
  puts "<source name=\"install#{i}\" url=\"file:#{b}\"/>"
  i = i + 1
end
puts "<source name=\"update\" url=\"file:#{options.path}\"/>"
puts "</setup>"

patches.patches.each do |pn|
  p = Pathname.new( pn )
  names = p.basename(".xml").to_s.split("-")
  version = names.pop
  name = names[1..-1].join("-")
  puts "  <trial><install channel=\"update\" kind=\"patch\" name=\"#{name}\" version=\"#{version}\" /></trial>"
end

puts "</test>"
