#!/usr/bin/ruby
$: << File.dirname(__FILE__)
require 'idlparser'

lang = "cpp"
idls = []
OPTIONS = {}
OPTIONS["dstdir"] = "."
OPTIONS["module"] = "roptypes"

while ARGV.length > 0
    arg = ARGV.shift
    case arg
    when '--lang', '-l'
        lang = ARGV.shift
    when '--base-package'
        OPTIONS["base-package"] = ARGV.shift + "."
    when '--dest', '-d'
        OPTIONS["dstdir"] = ARGV.shift
    when '--module', '-m'
        OPTIONS["module"] = ARGV.shift
    when '--extern-file'
        OPTIONS["extern-file"] = ARGV.shift
    when '--print-trace'
        OPTIONS["trace"] = true
    else
        idls << arg
    end
end

if idls.empty?
    exit 0
end

unless lang && ["js", "cpp", "java", "doc"].include?(lang)
    puts "Invalid output language specified."
    exit 1
end

packages = []
idls.each { |idl|
    unless File.exists?(idl)
        puts "File not found:#{idl}"
        exit 1
    end
    pkgs = IDLParser.parse(File.read(idl), idl)
    pkgs.each { |p|
        p.source = idl
    }
    packages.concat pkgs
}
packages = mergeAndResolve(packages)

require "gen#{lang}"
unless File.directory?(OPTIONS["dstdir"])
    puts "Invalid destiantion path"
    exit 1
end
Dir.chdir(OPTIONS["dstdir"]) or raise "failed to chdir to #{OPTIONS["dstdir"]}"
generate packages
