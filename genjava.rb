################################################################################
## C++ Specific Only

require 'fileutils'

ROP_PREFIX = "com.alticast.rop."
PACKAGE_PREFIX = OPTIONS["base-package"] || ""

JAVA_TYPES = {
    "String" => "String",
    "Map" => "Map",
    "List" => "List",
    "Nullable" => "",
    "i8" => "Byte",
    "i16" => "Short",
    "i32" => "Integer",
    "i64" => "Long",
    "async" => "void",
    "void" => "void"
}

JAVA_OTYPES = {
    "i8" => "byte",
    "i16" => "short",
    "i32" => "int",
    "i64" => "long",
}

class PackageNode
    def cname
        "#{PACKAGE_PREFIX}#{name}"
    end

    def dir
        cname.gsub(".", "/")
    end
end

class ExceptionNode
    def externalTypes
        ts = []
        fieldTypes.each { |t|
            ts << t
            next unless t.subTypes
            t.subTypes.each { |t2|
                ts << t2
            }
        }
        ts.delete_if { |t| JAVA_TYPES.has_key?(t.name) }
        ts.uniq
    end
end

class StructNode
    def externalTypes
        ts = []
        fieldTypes.each { |t|
            ts << t
            next unless t.subTypes
            t.subTypes.each { |t2|
                ts << t2
            }
        }
        ts.delete_if { |t| JAVA_TYPES.has_key?(t.name) }
        ts.uniq
    end
end

class InterfaceNode
    def externalTypes
        ts = []
        methods.each { |m|
            m.returnTypes.each { |t|
                ts << t
                next unless t.subTypes
                t.subTypes.each { |t2|
                    ts << t2
                }
            }
            m.argumentTypes.each { |t|
                ts << t
                next unless t.subTypes
                t.subTypes.each { |t2|
                    ts << t2
                }
            }
        }
        ts.delete_if { |t| JAVA_TYPES.has_key?(t.name) }
        ts.uniq
    end
end

class TypeNode
    def to_java
        JAVA_TYPES[name] || "#{PACKAGE_PREFIX}#{package.name}.#{name}"
    end

    def cname
        case name
        when "Map", "List"
            "#{to_java}"
        when "Nullable"
            "#{subTypes[0].cname}"
        else
            to_java
        end
    end

    def oname
        JAVA_OTYPES[name] || cname
    end

    def oread buf, ccache = {}
        case name
        when "i8"; "#{buf}.readI8()"
        when "i16"; "#{buf}.readI16()"
        when "i32"; "#{buf}.readI32()"
        when "i64"; "#{buf}.readI64()"
        else "(#{cname})(#{ccache[codec] || codec}.read(#{buf}))"
        end
    end

    def owrite obj, buf, ccache = {}
        case name
        when "i8"; "#{buf}.writeI8(#{obj})"
        when "i16"; "#{buf}.writeI16(#{obj})"
        when "i32"; "#{buf}.writeI32(#{obj})"
        when "i64"; "#{buf}.writeI64(#{obj})"
        else "#{ccache[codec] || codec}.write(#{obj}, #{buf})"
        end
    end

    def codec
        case name
        when "String"; "#{ROP_PREFIX}StringCodec.instance"
        when "i8"; "#{ROP_PREFIX}I8Codec.instance"
        when "i16"; "#{ROP_PREFIX}I16Codec.instance"
        when "i32"; "#{ROP_PREFIX}I32Codec.instance"
        when "i64"; "#{ROP_PREFIX}I64Codec.instance"
        when "void"; "#{ROP_PREFIX}VoidCodec.instance"
        when "async"; nil
        when "Map";
            "new #{ROP_PREFIX}MapCodec(#{
                subTypes[0].codec
            }, #{
                subTypes[1].codec
            })"
        when "List"
            "new #{ROP_PREFIX}ListCodec(#{
                subTypes[0].codec
            })"
        when "Nullable"
            "new #{ROP_PREFIX}NullableCodec(#{
                subTypes[0].codec
            })"
        else
            if package.interfaces.has_key?(name)
                "new #{ROP_PREFIX}InterfaceCodec(#{cname}Stub.class)"
            else
                "#{to_java}Codec.instance"
            end
        end
    end

    def wrap v
        case name
        when "i8", "i16", "i32", "i64"
            "New.#{name}(#{v})"
        else
            v
        end
    end
end

def makeCodecCache types
    cc = {}
    id = 0
    types.each { |t|
        c = t.codec
        next if c == nil
        next if cc.has_key?(c)
        cc[c] = "codec#{id}"
        id += 1
    }
    return cc
end

def declareCodeCache ccache
    ccache.keys.map { |c|
        "   private static final Codec #{ccache[c]} = #{c};\n"
    }.join
end

def generate packages

    packages.each { |pkg|
        FileUtils.mkdir_p(pkg.dir);

        pkg.exceptions.each { |name, exp|
            open("#{pkg.dir}/#{name}.java", "w") { |f|
                f.puts <<-TEMPLATE
package #{pkg.cname};
import #{ROP_PREFIX}*;
import java.util.*;
#{exp.externalTypes.map { |t|
    "import #{t.cname};"
}.join("\n")}
public class #{name} extends Exception {
#{exp.fieldTypes.map { |t|
"    #{t.oname} #{t.variable};"
}.join("\n")}
    public #{name} () {}
    public #{name} (#{(0...exp.fieldTypes.size).map { |i| 
        "#{exp.fieldTypes[i].oname} arg#{i}"
    }.join(", ")}) {
#{(0...exp.fieldTypes.size).map { |i|
"        this.#{exp.fieldTypes[i].variable} = arg#{i};"
}.join("\n")}
    }
}
                TEMPLATE
            }

            open("#{pkg.dir}/#{name}Codec.java", "w") { |f|
                ccache = makeCodecCache(exp.fieldTypes)
                f.puts <<-TEMPLATE
package #{pkg.cname};
import #{ROP_PREFIX}*;
import java.util.*;
#{exp.externalTypes.map { |t|
    "import #{t.cname};"
}.join("\n")}
public class #{name}Codec implements Codec {

#{declareCodeCache(ccache)}
    public static final #{name}Codec instance = new #{name}Codec();

    public Object read (Buffer buf) {
        #{name} o = new #{name}();
#{exp.fieldTypes.map { |t| "\
        o.#{t.variable} = #{t.oread("buf", ccache)};"
}.join("\n")}
        return o;
    }

    public void write (Object obj, Buffer buf) {
        #{name} o = (#{name})obj;
#{exp.fieldTypes.map { |t|
"        #{t.owrite("o.#{t.variable}", "buf", ccache)};"
}.join("\n")}
    }
}
                TEMPLATE
            }
        }

        pkg.structs.each { |name, st|
            open("#{pkg.dir}/#{name}.java", "w") { |f|
                f.puts <<-TEMPLATE
package #{pkg.cname};
import #{ROP_PREFIX}*;
import java.util.*;
#{st.externalTypes.map { |t|
    "import #{t.cname};"
}.join("\n")}
public class #{name} {
#{st.fieldTypes.map { |t|
"    #{t.oname} #{t.variable};"
}.join("\n")}
    public #{name} () {}
    public #{name} (#{(0...st.fieldTypes.size).map { |i| 
        "#{st.fieldTypes[i].oname} arg#{i}"
    }.join(", ")}) {
#{(0...st.fieldTypes.size).map { |i|
"        this.#{st.fieldTypes[i].variable} = arg#{i};"
}.join("\n")}
    }
}
                TEMPLATE
            }

            open("#{pkg.dir}/#{name}Codec.java", "w") { |f|
                ccache = makeCodecCache(st.fieldTypes)
                f.puts <<-TEMPLATE
package #{pkg.cname};
import #{ROP_PREFIX}*;
import java.util.*;
#{st.externalTypes.map { |t|
    "import #{t.cname};"
}.join("\n")}
public class #{name}Codec implements Codec {

#{declareCodeCache(ccache)}
    public static final #{name}Codec instance = new #{name}Codec();

    public Object read (Buffer buf) {
        #{name} o = new #{name}();
#{st.fieldTypes.map { |t|
"        o.#{t.variable} = #{t.oread("buf", ccache)};"
}.join("\n")}
        return o;
    }

    public void write (Object obj, Buffer buf) {
        #{name} o = (#{name})obj;
#{st.fieldTypes.map { |t|
"        #{t.owrite("o.#{t.variable}", "buf", ccache)};"
}.join("\n")}
    }
}
                TEMPLATE
            }
        }

        pkg.interfaces.each { |name, intf|
            open("#{pkg.dir}/#{name}.java", "w") { |f|
                f.puts <<-TEMPLATE
package #{pkg.cname};
import #{ROP_PREFIX}*;
import java.util.*;
#{intf.externalTypes.map { |t|
    "import #{t.cname};"
}.join("\n")}
public interface #{name} {
#{intf.methods.map {|m|
"    #{m.returnTypes[0].oname} #{m.name} (#{
    m.argumentTypes.map {|t|
        "#{t.oname} #{t.variable}"
    }.join(", ")
})#{if m.returnTypes.size > 1
    " throws #{m.returnTypes[1..-1].map { |rt|
        "#{rt.cname}"
    }.join(", ")}"
end};"
}.join("\n")}
}
                TEMPLATE
            }

            ccache = makeCodecCache(intf.methods.map { |m|
                    m.returnTypes + m.argumentTypes }.flatten)

            open("#{pkg.dir}/#{name}Stub.java", "w") { |f|
                f.puts <<-TEMPLATE
package #{pkg.cname};
import #{ROP_PREFIX}*;
import java.util.*;
#{intf.externalTypes.map { |t|
    "import #{t.cname};"
}.join("\n")}
public class #{name}Stub extends Stub implements #{name} {

#{declareCodeCache(ccache)}
                TEMPLATE
                intf.methods.each_index { |mi|
                    m = intf.methods[mi]
                    rt = m.returnTypes[0]
                    async = (rt.name == "async")
                    f.puts <<-TEMPLATE
    public #{rt.oname} #{m.name} (#{
        m.argumentTypes.map { |t| "#{t.oname} #{t.variable}" }.join(", ")
    })#{
        if m.returnTypes.size > 1
            " throws #{m.returnTypes[1..-1].map { |t| "#{t.cname}" }.join(", ")}"
        end
    } {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(#{async ? 1 : 0}<<6, remote, #{mi});
#{m.argumentTypes.map{ |t|
"            #{t.owrite(t.variable, "buf", ccache)};"
}.join("\n")}
            remote.registry.#{async ? "asyncCall(rc, false)" : "syncCall(rc)"};
#{%(\
            switch (buf.readI8() & 63) {
            case 0: return#{" #{rt.oread("buf", ccache)}" if rt.name != "void"};
#{(1...m.returnTypes.size).map { |i|
    t = m.returnTypes[i]
"            case #{i}: throw #{t.oread("buf", ccache)};\n"
}.join}\
            default: throw new RemoteException("Remote Call Failed.");
            }
) unless async}\
        } finally {
            New.release(rc);
        }
    }
                    TEMPLATE
                }
                f.puts "}"
            }

            open("#{pkg.dir}/#{name}Skel.java", "w") { |f|
                f.puts <<-TEMPLATE
package #{pkg.cname};
import #{ROP_PREFIX}*;
import java.util.*;
#{intf.externalTypes.map { |t|
    "import #{t.cname};"
}.join("\n")}
public class #{name}Skel extends Skeleton {

#{declareCodeCache(ccache)}
    public #{name}Skel (#{name} o) {
        object = o;
    }

    public void processRequest (LocalCall lc) {
        switch (lc.buffer.readI16()) {
#{(0...intf.methods.size).map { |mi|
    m = intf.methods[mi]
"        case #{mi}: __call_#{m.name}(lc); return;\n"
}.join}\
        default: throw new RemoteException("Out of method index range.");
        }
    }

#{intf.methods.map { |m|
    rt = m.returnTypes[0]
    async = rt.name == "async"
%(\
    private void __call_#{m.name} (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
#{m.argumentTypes.map { |t|
"            #{t.oname} #{t.variable} = #{t.oread("__buf", ccache)};\n"
}.join}\
#{
    callstr = "((#{name})object).#{m.name}(#{
            m.argumentTypes.map{ |t| t.variable }.join(", ")})"
    case rt.name
    when "async"; %(\
            #{callstr};)
    when "void"; %(\
            #{callstr};
            lc.codec = #{ccache[rt.codec]};
            lc.index = 0;)
    else %(\
            lc.value = #{rt.wrap(callstr)};
            lc.codec = #{ccache[rt.codec]};
            lc.index = 0;)
    end
}
#{(1...m.returnTypes.size).map { |ti|
    t = m.returnTypes[ti]; %(\
        } catch (#{t.cname} e) {
            lc.value = e;
            lc.codec = #{ccache[t.codec]};
            lc.index = #{ti};
)}.join}\
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
)
}.join}\
}
                TEMPLATE
            }
        }
    }
end
