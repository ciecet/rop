#!/usr/bin/ruby
$: << File.dirname(__FILE__)
require 'idlparser'

lang = "cpp"
inputFile = nil
OPTIONS = {}
OPTIONS["base-package"] = "com.alticast."

while ARGV.length > 0
    arg = ARGV.shift
    case arg
    when '--lang', '-l'
        lang = ARGV.shift
    when '--base-package', '-bp'
        OPTIONS["base-package"] = ARGV.shift + "."
    else
        inputFile = arg
    end
end

unless lang && File.exists?("gen#{lang}.rb")
    puts "Invalid output language specified."
    exit 1
end

unless inputFile && File.exists?(inputFile)
    puts "Need filename"
    exit 1
end

packages = IDLParser.parse(File.read(inputFile))
require "gen#{lang}"
generate packages
