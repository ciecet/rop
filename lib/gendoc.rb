class TypeNode
    def cname(href=true)
        case name
        when "bool"; "bool"
        when "i8"; "i8"
        when "i16"; "i16"
        when "i32"; "i32"
        when "i64"; "i64"
        when "f32"; "f32"
        when "f64"; "f64"
        when "String"; "String"
        when "Variant"; "Variant"
        when "void"; "void"
        when "async"; "async"
        when "Nullable"; "Nullable&lt#{subTypes[0].cname}&gt"
        when "List"; "List&lt#{subTypes[0].cname}&gt"
        when "Map"
            "Map&lt#{subTypes[0].cname}, #{subTypes[1].cname}&gt"
        else
            if href
                # here means ref to other types.
                np = name.rpartition('.')
                np.first.gsub!('.', '_') if np.first.include? '.'

                link_tag = "<a href=\"descript_#{np.first}.html##{np.last}\">#{name}</a>"
            else
                "#{name}"
            end
        end
    end
end

class CommentRefinery
    attr_accessor :type, :comment

    def initialize(type, stream)
        @type = type
        @comment = nil
        refine(stream) unless stream.empty?
    end

    def refine stream
        # remove "\n*"
        stream.gsub!(/\s+\*/, "\n")
        # remove "/**" and "/"
        stream.gsub!(/\A\/\*{2}|\/\Z/, '')
        @comment = stream.strip
    end
end

class MethodNode
    def getAnchorName
        anchor = ""
        anchor << name
        anchor << "("
        anchor << argumentTypes.map { |a| a.cname(false) }.join(",")
        anchor << ")"
    end
end

DOC_BASE_DIR = "./"
HTML_HEADER = "<!DOCTYPE html>\n<html>\n"
HTML_FOOTER = "\n</html>\n"

def generate pkgs
    # dir check.
    if File.exists? DOC_BASE_DIR
        Dir.foreach(DOC_BASE_DIR) { |fn|
            d_fn = File.join(DOC_BASE_DIR, fn)
            File.delete(d_fn) if File.extname(d_fn).eql? ".html"
        }
    else
        Dir.mkdir(DOC_BASE_DIR)
    end

    # wirte HTML
    writeIndex
    writePackagesAll(pkgs)
    writePackagesFrame(pkgs)
    writePackageFrame(pkgs)
    writePackageDescFrame(pkgs)
end

def writeIndex
    File.open(DOC_BASE_DIR+"index.html", "w") do |f|
        f.puts HTML_HEADER
        f.puts "<frameset cols=\"20%\,*\">"
        f.puts "<frameset rows=\"30%,*\">"
        f.puts "<frame src=\"idl_packages.html\" name=\"idl_packages\">"
        f.puts "<frame src=\"package_all.html\" name=\"package\">"
        f.puts "</frameset>"
        f.puts "<frame name=\"description\">"
        f.puts "</frameset>"
        f.puts HTML_FOOTER
    end
end

def writePackagesFrame pkgs
    File.open(DOC_BASE_DIR+"idl_packages.html", "w") do |f|
        f.puts HTML_HEADER
        f.puts "<body>"
        f.puts "<h3 title=\"IDL Packages\">IDL Packages</h3>"
        f.puts "<a href=\"package_all.html\" target=\"package\">All</a>"
        f.puts "<br>"
        f.puts "<ul title=\"IDL Packages\">"
        pkgs.each { |pkg|
            f.puts "<li><a href=\"package_#{pkg.name}.html\" target=\"package\">#{pkg.name}</a></li>"
        }

        f.puts "</ul>"
        f.puts "</body>"
        f.puts HTML_FOOTER
    end
end

def writePackagesAll pkgs
    File.open(DOC_BASE_DIR+"package_all.html", "w") do |f|
        f.puts HTML_HEADER
        f.puts "<body>"
        f.puts "<h3 title=\"All Packages\">All</h3>"

        # wrtie struct info.
        f.puts "<h4>Struct</h4>"
        f.puts "<ul>"
        pkgs.each { |pkg|
            pkg.structs.each { |name, st|
                f.puts "<li><a href=\"descript_#{pkg.name}.html##{name}\" target=\"description\">#{name}</a></li>"
            }
        }
        f.puts "</ul>"

        # write interface info.
        f.puts "<h4>Interface</h4>"
        f.puts "<ul>"
        pkgs.each { |pkg|
            pkg.interfaces.each { |name, intf|
                f.puts "<li><a href=\"descript_#{pkg.name}.html##{name}\" target=\"description\">#{name}</a></li>"
            }
        }
        f.puts "</ul>"

        # write exception info.
        f.puts "<h4>Exception</h4>"
        f.puts "<ul>"
        pkgs.each { |pkg|
            pkg.exceptions.each { |name, exp|
                f.puts "<li><a href=\"descript_#{pkg.name}.html##{name}\" target=\"description\">#{name}</a></li>"
            }
        }
        f.puts "</ul>"

        f.puts "</body>"
        f.puts HTML_FOOTER
    end
end

def writePackageFrame pkgs
    pkgs.each { |pkg|
        File.open(DOC_BASE_DIR+"package_#{pkg.name}.html", "w") do |f|
            f.puts HTML_HEADER
            f.puts "<body>"
            f.puts "<h3 title=\"Package #{pkg.name}\">#{pkg.name}</h3>"

            f.puts "<h4>Struct</h4>"
            f.puts "<ul>"
            pkg.structs.each { |name, st|
                f.puts "<li><a href=\"descript_#{pkg.name}.html##{name}\" target=\"description\">#{name}</a></li>"
            }
            f.puts "</ul>"

            f.puts "<h4>Interface</h4>"
            f.puts "<ul>"
            pkg.interfaces.each { |name, intf|
                f.puts "<li><a href=\"descript_#{pkg.name}.html##{name}\" target=\"description\">#{name}</a></li>"
            }
            f.puts "</ul>"

            f.puts "<h4>Exception</h4>"
            f.puts "<ul>"
            pkg.exceptions.each { |name, exp|
                f.puts "<li><a href=\"descript_#{pkg.name}.html##{name}\" target=\"description\">#{name}</a></li>"
            }
            f.puts "</ul>"

            f.puts "</body>"
            f.puts HTML_FOOTER
        end
    }
end

def writePackageDescFrame pkgs
    pkgs.each { |pkg|
        File.open(DOC_BASE_DIR+"descript_#{pkg.name}.html", "w") do |f|
            f.puts HTML_HEADER
            f.puts "<body>"

            f.puts "<h2>Package #{pkg.name}</h2>"
            rc = CommentRefinery.new(pkg, pkg.comments)
            f.puts "<pre>#{rc.comment}</pre>" unless rc.comment.nil?
            f.puts "<hr><br>"

            pkg.structs.each { |name, st|
                f.puts "<h3><a name=\"#{name}\">struct #{name}</a></h3>"
                f.puts "<pre style='color:green;'>from: #{st.package.source}</pre>"
                rc = CommentRefinery.new(st, st.comments)
                f.puts "<pre>#{rc.comment}</pre>" unless rc.comment.nil?
                f.puts "<br>"
                f.puts "<table border=\"1\" width=\"100%\" cellpadding=\"3\", cellspacing=\"0\">"
                f.puts "<tr>"
                f.puts "<th colspan=2 align=\"left\">Fields in #{name}</th>"
                f.puts "</tr>"

                st.fieldTypes.map { |t|
                    f.puts "<tr><td align=\"right\" valign=\"top\" width=\"10%\">"
                    f.puts "#{t.cname}"
                    f.puts "</td><td>#{t.variable}<br>"
                    rc = CommentRefinery.new(t, t.comments)
                    f.puts "<pre>#{rc.comment}</pre>" unless rc.comment.nil?
                    f.puts "</td></tr>"
                }

                f.puts "</table>"
                f.puts "<hr><br>"
            }

            pkg.interfaces.each { |name, intf|
                f.print "<h3><a name=\"#{name}\">interface #{name}</a>"
                f.print " extends #{intf.baseInterface.cname}" unless intf.baseInterface.nil?
                f.print "</h3>\n"
                f.puts "<pre style='color:green;'>from: #{intf.package.source}</pre>"
                rc = CommentRefinery.new(intf, intf.comments)
                f.puts "<pre>#{rc.comment}</pre>" unless rc.comment.nil?
                f.puts "<br>"
                f.puts "<table border=\"1\" width=\"100%\" cellpadding=\"3\", cellspacing=\"0\">"
                f.puts "<tr>"
                f.puts "<th colspan=2 align=\"left\">Methods in #{name}</th>"
                f.puts "</tr>"

                intf.methods.map { |m|
                    f.puts "<tr><td align=\"right\" valign=\"top\" width=\"10%\">"
                    f.puts "#{m.returnTypes[0].cname}"
                    f.puts "</td><td>"
                    f.print "<a name=\"#{intf.name}_#{m.getAnchorName}\">"
                    f.print "#{m.name}"
                    f.print "</a> ("
                    f.print m.argumentTypes.map{ |a| a.cname + " " + a.variable }.join(", ")
                    f.print ")"

                    # throws
                    if m.returnTypes.size > 1
                        f.print " <b>throws</b> "
                        f.print m.returnTypes[1..-1].map { |e| e.cname}.join(", ")
                    end

                    f.print "<br>"
                    rc = CommentRefinery.new(m, m.comments)
                    f.puts "<pre>#{rc.comment}</pre>" unless rc.comment.nil?
                    f.puts "</td></tr>"
                }

                f.puts "</table>"

                # display inherited interface methods.
                unless intf.baseInterface.nil?
                    f.puts "<br>"
                    f.puts "<table border=\"1\" width=\"100%\" cellpadding=\"3\">"
                    f.puts "<tr>"
                    f.puts "<th align=\"left\">Methods inherited from #{intf.baseInterface.cname}</th>"
                    f.puts "</tr>"
                    f.puts "<tr><td>"

                    bname = intf.baseInterface.name.rpartition('.')
                    bname.first.gsub!('.', '_') if bname.first.include? '.'
                    f.puts intf.baseInterface.definition.fullMethods.map { |m|
                        "<a href=\"descript_#{bname.first}.html##{bname.last}_#{m.getAnchorName}\">#{m.name}</a>"
                    }.join(", ")
                    f.puts "</td></tr>"

                    f.puts "</table>"
                end

                f.puts "<hr><br>"
            }

            pkg.exceptions.each { |name, exp|
                f.puts "<h3><a name=\"#{name}\">exception #{name}</a></h3>"
                f.puts "<pre style='color:green;'>from: #{exp.package.source}</pre>"
                rc = CommentRefinery.new(exp, exp.comments)
                f.puts "<pre>#{rc.comment}</pre>" unless rc.comment.nil?
                f.puts "<br>"
                f.puts "<table border=\"1\" width=\"100%\" cellpadding=\"3\", cellspacing=\"0\">"
                f.puts "<tr>"
                f.puts "<th colspan=2 align=\"left\">Fields in #{name}</th>"
                f.puts "</tr>"

                exp.fieldTypes.map { |t|
                    f.puts "<tr><td align=\"right\" valign=\"top\" width=\"10%\">"
                    f.puts "#{t.cname}"
                    f.puts "</td><td>#{t.variable}<br>"
                    rc = CommentRefinery.new(t, t.comments)
                    f.puts "<pre>#{rc.comment}</pre>" unless rc.comment.nil?
                    f.puts "</td></tr>"
                }

                f.puts "</table>"
                f.puts "<hr><br>"
            }

            f.puts "</body>"
            f.puts HTML_FOOTER
        end
    }
end
