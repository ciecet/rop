#!/usr/bin/ruby

require 'webrick'

class XFileHandler < WEBrick::HTTPServlet::FileHandler
    def do_GET request, response
        super
        response["Access-Control-Allow-Origin"] = "*"
        response["Access-Control-Allow-Methods"] = "POST, GET"
        #p response
    end
end

server = WEBrick::HTTPServer.new(:Port => 8080, :DoNotReverseLookup => true)
trap :INT do
    server.stop
end
server.mount "/", XFileHandler, ".", :FancyIndexing=>true
server.start
