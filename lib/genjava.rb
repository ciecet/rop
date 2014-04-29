################################################################################
## JAVA Specific Only

require 'fileutils'

ROP_PREFIX = "com.alticast.rop."
IO_PREFIX = "com.alticast.io."
PACKAGE_PREFIX = OPTIONS["base-package"] || ""

JAVA_TYPES = {
    "bool" => "Boolean",
    "String" => "String",
    "Map" => "Map",
    "List" => "List",
    "Nullable" => "",
    "i8" => "Byte",
    "i16" => "Short",
    "i32" => "Integer",
    "i64" => "Long",
    "f32" => "Float",
    "f64" => "Double",
    "async" => "void",
    "void" => "void",
    "Variant" => "Object"
}

JAVA_OTYPES = {
    "bool" => "boolean",
    "i8" => "byte",
    "i16" => "short",
    "i32" => "int",
    "i64" => "long",
    "f32" => "float",
    "f64" => "double"
}

class PackageNode
    def cname
        "#{PACKAGE_PREFIX}#{name}"
    end

    def dir
        cname.gsub(".", "/")
    end
end

class TypeNode
    def to_java
        JAVA_TYPES[name] || "#{PACKAGE_PREFIX}#{name}"
    end

    def cname
        case name
        when "Map"
            "#{to_java}"
        when "Nullable"
            "#{subTypes[0].cname}"
        when "List"
            if subTypes[0].isPrimitive
                "#{subTypes[0].oname}[]"
            else
                "#{to_java}"
            end
        else
            to_java
        end
    end

    def oname
        JAVA_OTYPES[name] || cname
    end

    def isPrimitive
        JAVA_OTYPES.has_key?(name)
    end

    def oread buf, ccache = {}
        case name
        when "bool"; "#{buf}.readBool()"
        when "i8"; "#{buf}.readI8()"
        when "i16"; "#{buf}.readI16()"
        when "i32"; "#{buf}.readI32()"
        when "i64"; "#{buf}.readI64()"
        when "f32"; "#{buf}.readF32()"
        when "f64"; "#{buf}.readF64()"
        else "(#{cname})(#{ccache[codec] || codec}.read(#{buf}))"
        end
    end

    def owrite obj, buf, ccache = {}
        case name
        when "bool"; "#{buf}.writeBool(#{obj})"
        when "i8"; "#{buf}.writeI8(#{obj})"
        when "i16"; "#{buf}.writeI16(#{obj})"
        when "i32"; "#{buf}.writeI32(#{obj})"
        when "i64"; "#{buf}.writeI64(#{obj})"
        when "f32"; "#{buf}.writeF32(#{obj})"
        when "f64"; "#{buf}.writeF64(#{obj})"
        else "#{ccache[codec] || codec}.write(#{obj}, #{buf})"
        end
    end

    def oscan buf, ccache = {}
        case name
        when "bool"; "#{buf}.drop(1)"
        when "i8"; "#{buf}.drop(1)"
        when "i16"; "#{buf}.drop(2)"
        when "i32"; "#{buf}.drop(4)"
        when "i64"; "#{buf}.drop(8)"
        when "f32"; "#{buf}.drop(4)"
        when "f64"; "#{buf}.drop(8)"
        else "#{ccache[codec] || codec}.scan(#{buf})"
        end
    end

    def codec
        case name
        when "String"; "#{ROP_PREFIX}StringCodec.instance"
        when "bool"; "#{ROP_PREFIX}BoolCodec.instance"
        when "i8"; "#{ROP_PREFIX}I8Codec.instance"
        when "i16"; "#{ROP_PREFIX}I16Codec.instance"
        when "i32"; "#{ROP_PREFIX}I32Codec.instance"
        when "i64"; "#{ROP_PREFIX}I64Codec.instance"
        when "f32"; "#{ROP_PREFIX}F32Codec.instance"
        when "f64"; "#{ROP_PREFIX}F64Codec.instance"
        when "void"; "#{ROP_PREFIX}VoidCodec.instance"
        when "async"; nil
        when "Variant"; "#{ROP_PREFIX}VariantCodec.instance"
        when "Map";
            "new #{ROP_PREFIX}MapCodec(#{
                subTypes[0].codec
            }, #{
                subTypes[1].codec
            })"
        when "List"
            case subTypes[0].name
            when "bool"; "#{ROP_PREFIX}BoolArrayCodec.instance"
            when "i8"; "#{ROP_PREFIX}I8ArrayCodec.instance"
            when "i16"; "#{ROP_PREFIX}I16ArrayCodec.instance"
            when "i32"; "#{ROP_PREFIX}I32ArrayCodec.instance"
            when "i64"; "#{ROP_PREFIX}I64ArrayCodec.instance"
            when "f32"; "#{ROP_PREFIX}F32ArrayCodec.instance"
            when "f64"; "#{ROP_PREFIX}F64ArrayCodec.instance"
            else
                "new #{ROP_PREFIX}ListCodec(#{
                    subTypes[0].codec
                })"
            end
        when "Nullable"
            "new #{ROP_PREFIX}NullableCodec(#{
                subTypes[0].codec
            })"
        else
            if InterfaceNode === definition
                "new #{ROP_PREFIX}InterfaceCodec(#{cname}Stub.class)"
            else
                "#{to_java}Codec.instance"
            end
        end
    end

    def wrap v
        case name
        when "i8", "i16", "i32", "i64", "bool", "f32", "f64"
            "New.#{name}(#{v})"
        else
            v
        end
    end
end

class ConstNode
    def literal
        case @constType.name
        when "i64"
            @value.inspect+"L"
        when "f32"
            @value.inspect+"F"
        when "f64"
            @value.inspect+"D"
        else
            @value.inspect
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
        "    private static final Codec #{ccache[c]} = #{c};\n"
    }.join
end

def generate packages

    packages.each { |pkg|
        FileUtils.mkdir_p(pkg.dir)

        pkg.exceptions.each { |name, exp|
            open("#{pkg.dir}/#{name}.java", "w") { |f|
                f.puts <<-TEMPLATE
package #{pkg.cname};
import java.util.*;
import #{ROP_PREFIX}*;
import #{IO_PREFIX}*;
public class #{name} extends Exception {
#{exp.fieldTypes.map { |t|
"    public #{t.oname} #{t.variable};"
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
import java.util.*;
import #{ROP_PREFIX}*;
import #{IO_PREFIX}*;
public class #{name}Codec implements Codec {

#{declareCodeCache(ccache)}
    public static final #{name}Codec instance = new #{name}Codec();

    public void scan (Buffer buf) {
#{exp.fieldTypes.map { |t| "\
        #{t.oscan("buf", ccache)};"
}.join("\n")}
    }

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
import java.util.*;
import #{ROP_PREFIX}*;
public class #{name} {
#{st.fieldTypes.map { |t| %(\
    public #{t.oname} #{t.variable};
)}.join}\
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
import java.util.*;
import #{ROP_PREFIX}*;
import #{IO_PREFIX}*;
public class #{name}Codec implements Codec {

#{declareCodeCache(ccache)}
    public static final #{name}Codec instance = new #{name}Codec();

    public void scan (Buffer buf) {
#{st.fieldTypes.map { |t| "\
        #{t.oscan("buf", ccache)};"
}.join("\n")}
    }

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
public interface #{name}#{
    " extends "+intf.baseInterface.cname if intf.baseInterface
} {
#{intf.consts.map { |c| %(\
    public static final #{c.constType.oname} #{c.constType.variable} = #{c.literal};\n)
}.join}\
#{intf.methods.map {|m|
"    #{m.returnTypes[0].oname} #{m.name} (#{
    m.argumentTypes.map {|t|
        "#{t.oname} #{t.variable}"
    }.join(", ")
})#{if m.returnTypes.size > 1
    " throws #{m.returnTypes[1..-1].map { |rt|
        "#{rt.cname}"
    }.join(", ")}"
end};\n"
}.join}\
}
                TEMPLATE
            }

            fullMethods = intf.fullMethods
            ccache = makeCodecCache(fullMethods.map { |m|
                    m.returnTypes + m.argumentTypes }.flatten)

            open("#{pkg.dir}/#{name}Stub.java", "w") { |f|
                f.puts <<-TEMPLATE
package #{pkg.cname};
import java.util.*;
import #{ROP_PREFIX}*;
import #{IO_PREFIX}*;
public class #{name}Stub extends Stub implements #{name} {

#{declareCodeCache(ccache)}
                TEMPLATE
                fullMethods.each_index { |mi|
                    m = fullMethods[mi]
                    rt = m.returnTypes[0]
                    async = (rt.name == "async")
                    hasRet = (m.returnTypes.size > 1 || (!async && rt != "void"))
                    if hasRet
                        f.puts <<-TEMPLATE
    public static final Codec[] ret#{mi} = {#{
        m.returnTypes.map { |t|
            case t.name
            when "void"; "null"
            else ccache[t.codec] || t.codec
            end
        }.join(", ") }};
                        TEMPLATE
                    end
                    f.puts <<-TEMPLATE
    public #{rt.oname} #{m.name} (#{
        m.argumentTypes.map { |t| "#{t.oname} #{t.variable}" }.join(", ")
    })#{
        if m.returnTypes.size > 1
            " throws #{m.returnTypes[1..-1].map { |t| "#{t.cname}" }.join(", ")}"
        end
    } {
#{if OPTIONS["trace"] then %(\
        if (Log.I) Log.info(""+remote.id+".#{m.name}");
) end}\
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(#{async ? 1 : 0}<<6, remote, #{mi});
#{m.argumentTypes.map{ |t|
"            #{t.owrite(t.variable, "buf", ccache)};"
}.join("\n")}#{"\n\
            rc.setReturnCodecs(ret#{mi});" if hasRet}
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
import java.util.*;
import #{ROP_PREFIX}*;
import #{IO_PREFIX}*;
public class #{name}Skel extends Skeleton {

#{declareCodeCache(ccache)}
    public #{name}Skel () {}
    public #{name}Skel (#{name} o) {
        object = o;
    }

    public void scan (Buffer __buf) {
        switch (__buf.readI16()) {
        default: throw new RemoteException("Out of method index range.");
#{(0...fullMethods.size).map { |mi|
    m = fullMethods[mi]; "\
        case #{mi}:
#{m.argumentTypes.map { |t| "\
            #{t.oscan("__buf", ccache)};\n"
}.join}\
            break;\n"
}.join}\
        }
    }

    public void processRequest (LocalCall lc) {
        switch (lc.buffer.readI16()) {
        default: throw new RemoteException("Out of method index range.");
#{(0...fullMethods.size).map { |mi|
    m = fullMethods[mi]
"        case #{mi}: __call_#{m.name}(lc); return;\n"
}.join}\
        }
    }

#{fullMethods.map { |m|
    rt = m.returnTypes[0]
    async = rt.name == "async"
%(\
    private void __call_#{m.name} (LocalCall lc) {
#{if OPTIONS["trace"] then %(\
        if (Log.I) Log.info(""+lc.skeleton.id+".#{m.name}");
) end}\
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
            lc.index = -1;
            #{callstr};
            lc.codec = #{ccache[rt.codec]};
            lc.index = 0;)
    else %(\
            lc.index = -1;
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
        } finally {}
    }
)
}.join}\
}
                TEMPLATE
            }
        }
    }
end
