#!/usr/bin/ruby
$: << File.dirname(__FILE__)
require 'parser'

=begin

packages: [packages] package <name> { definitions }
definitions: definitions | structdef | interfacedef
structdef: struct <name> { fields }
fields: fields field | field
field: type <var>;
type: Map<type,type> | List<type> | Nullable<type> | struct_name | interface_name
interfacedef: interface <name> { methods }
methods: methods method
method: return_type <method> args; | sync_return_type <method> args throws Exceptions;
return_type: sync_return_type | async
args: () | (arglist)
arglist: arglist, arg | arg
arg: type <var>

=end

PackageNode = Struct.new(:name, :defs, :structs, :exceptions, :interfaces)
StructNode = Struct.new(:name, :fieldTypes)
ExceptionNode = Struct.new(:name, :fieldTypes)
InterfaceNode = Struct.new(:name, :methods)
MethodNode = Struct.new(:name, :returnTypes, :argumentTypes)
TypeNode = Struct.new(:name, :subTypes, :variable, :package)

NAME = /[a-zA-Z_][a-zA-Z0-9_]*/

parser = RDParser.new do
    token(/\b(package|struct|exception|interface)\b/) { |r| r.intern }
    token(/[<>{}();,]/) { |s| s }
    token(NAME) { |s| s }
    token(/\s*/)

    start :packages do
        match(:package, NAME, "{", :definitions, "}") { |a,b,c,d,e|
                [ PackageNode.new(b, d) ] }
        match(:packages, :package, NAME, "{", :definitions, "}") { |p,a,b,c,d,e|
                p << PackageNode.new(b,d) }
    end

    rule :definitions do
        match(:definition) { |a| {a.name => a} }
        match(:definitions, :definition) { |a,b| a[b.name] = b; a }
    end

    rule :definition do
        match(:struct, NAME, "{", :fields, "}") { |a,b,c,d,e|
                StructNode.new(b,d)}
        match(:exception, NAME, "{", :fields, "}") {|a,b,c,d,e|
                ExceptionNode.new(b,d)}
        match(:interface, NAME, "{", :methods, "}") { |a,b,c,d,e|
                InterfaceNode.new(b,d)}
    end

    rule :fields do
        match(:field) { |a| [a] }
        match(:fields, :field) { |a,b| a+[b] }
    end

    rule :field do
        match(:type, NAME, ";") {|a,b,c| a.variable = b; a}
    end

    rule :type do
        match("Map", "<", :type, ",", :type, ">") { |a,b,c,d,e,f|
                TypeNode.new(a, [c, e])}
        match("List", "<", :type, ">") { |a,b,c,d|
                TypeNode.new(a, [c])}
        match("Nullable", "<", :type, ">") {|a,b,c,d|
                TypeNode.new(a, [c])}
        match(NAME) { |a| TypeNode.new(a) }
    end

    rule :methods do
        match(:method) { |a| [a] }
        match(:methods, :method) { |a,b| a+[b] }
    end

    rule :method do
        match(:returnType, NAME, :args, ";") { |a,b,c,d|
                MethodNode.new(b, [a], c)}
        match(:syncReturnType, NAME, :args, "throws", :exceptions, ";") {
                |a,b,c,d,e,f| MethodNode.new(b, [a]+e, c)}
    end

    rule :returnType do
        match(:syncReturnType)
        match(:asyncReturnType)
    end

    rule :syncReturnType do
        match(:type)
        match("void") { |a| TypeNode.new(a) }
    end

    rule :asyncReturnType do
        match("async") { |a| TypeNode.new(a) }
    end

    rule :args do
        match("(", ")") { [] }
        match("(", :arglist, ")") { |a,b,c| b }
    end

    rule :arglist do
        match(:arg) { |a| [a] }
        match(:arglist, ",", :arg) {|a,b,c| a+[c]}
    end

    rule :arg do
        match(:type, NAME) {|a,b| a.variable = b; a}
    end

    rule :exceptions do
        match(NAME) { |a| [TypeNode.new(a)] }
        match(:exceptions, ",", NAME) { |a,b,c| a+[TypeNode.new(c)] }
    end
end

packages = parser.parse(`cat echo.idl`)
packages.each { |p|
    p.structs = {}
    p.exceptions = {}
    p.interfaces = {}
    p.defs.each { |n,d|
        case d
        when StructNode
            p.structs[n] = d
        when InterfaceNode
            p.interfaces[n] = d
        when ExceptionNode
            p.exceptions[n] = d
        end
    }
}

# type verification

class TypeNode
    def check p
        self.package = p
        if subTypes
            subTypes.each { |st|  
                st.check p
            }
        end

        return if %w(void async String Map List Nullable i8 i16 i32 i16).include?(name)
        return if package.defs.has_key?(name)
        throw "Unknown type #{name}"
    end
end

packages.each { |p|

    p.structs.each { |_,s|
        s.fieldTypes.each { |t|
            t.check p
        }
    }
    p.exceptions.each { |_,e|
        e.fieldTypes.each { |t|
            t.check p
        }
    }
    p.interfaces.each { |_,intf|
        intf.methods.each { |m|
            m.returnTypes.each { |t|
                t.check p
            }
            m.returnTypes[1..-1].each { |t|
                throw "Non exception type #{t}" unless p.exceptions.has_key?(t.name)
            }
            m.argumentTypes.each { |t|
                t.check p
            }
        }
    }
}

################################################################################
## From now on, C++ Specific Only
#
# StructNode = Struct.new(:name, :fieldTypes)
# ExceptionNode = Struct.new(:name, :fieldTypes)
# InterfaceNode = Struct.new(:name, :methods)
# MethodNode = Struct.new(:name, :returnTypes, :argumentTypes)
# TypeNode = Struct.new(:name, :subTypes, :variable)

CPP_TYPES = {
    "String" => "std::string",
    "Map" => "std::map",
    "List" => "std::vector",
    "Nullable" => "base::Ref",
    "i8" => "int8_t",
    "i16" => "int16_t",
    "i32" => "int32_t",
    "async" => "void",
    "void" => "void"
}

class StructNode
    def externalTypes
        ts = []
        fieldTypes.each { |t|
            ts << t unless CPP_TYPES.has_key?(t.name)
            next unless t.subTypes
            t.subTypes.each { |t2|
                ts << t2 unless CPP_TYPES.has_key?(t2.name)
            }
        }
        ts.uniq
    end
end

class InterfaceNode
    def externalTypes
        ts = []
        methods.each { |m|
            m.returnTypes.each { |t|
                ts << t unless CPP_TYPES.has_key?(t.name)
                next unless t.subTypes
                t.subTypes.each { |t2|
                    ts << t2 unless CPP_TYPES.has_key?(t2.name)
                }
            }
            m.argumentTypes.each { |t|
                ts << t unless CPP_TYPES.has_key?(t.name)
                next unless t.subTypes
                t.subTypes.each { |t2|
                    ts << t2 unless CPP_TYPES.has_key?(t2.name)
                }
            }
        }
        ts.uniq
    end
end

class TypeNode
    def to_cpp
        CPP_TYPES[name] || "#{package.name}::#{name}"
    end

    def cname
        isIntf = package.interfaces.has_key?(name)
        return "base::Ref<#{to_cpp}> " if isIntf

        case name
        when "Map"
            "#{to_cpp}<#{subTypes[0].cname}, #{subTypes[1].cname}> "
        when "List"
            "#{to_cpp}<#{subTypes[0].cname}> "
        when "Nullable"
            if isIntf
                "#{to_cpp}<#{subTypes[0].cname}> "
            else
                "base::ContainerRef<#{subTypes[0].cname}> "
            end
        else
            to_cpp
        end
    end

    def cnamevar
        "#{cname} #{variable}"
    end
end

packages.each { |pkg|

    pkg.exceptions.each { |name, exp|
        cname = "#{pkg.name}::#{name}"

        open(name+".h", "w") { |f|
            f.puts "\#ifndef #{name.upcase}_H"
            f.puts "\#define #{name.upcase}_H"
            f.puts "\#include \"Remote.h\""
            f.puts
            f.puts "namespace #{pkg.name} {"
            f.puts "struct #{name}: rop::RemoteException {"
            exp.fieldTypes.each { |t|
                f.puts "    #{t.cnamevar};"
            }
            f.puts "    #{name} () {}"
            f.puts "    #{name} (#{
                    (0...exp.fieldTypes.size).map{|i| 
                        "const #{exp.fieldTypes[i].cname} &arg#{i}"}.join(", ")}): #{
                    (0...exp.fieldTypes.size).map{|i|
                        "#{exp.fieldTypes[i].variable}(arg#{i})"}.join(", ")} {}"
            f.puts "    ~#{name} () throw() {}"
            f.puts "};"
            f.puts "}"
            f.puts
            f.puts "namespace rop {"
            f.puts "template<>"
            f.puts "struct Reader <#{cname}>: base::Frame {"
            f.puts "    #{cname} &object;"
            f.puts "    Reader (#{cname} &o): object(o) {}"
            f.puts "    STATE run (base::Stack *stack) {"
            f.puts "        switch (step) {"
            exp.fieldTypes.each_index { |i|
                t = exp.fieldTypes[i]
                f.puts "        case #{i}: stack->push(new(stack->allocate(sizeof(Reader<#{t.cname}>))) Reader<#{t.cname}>(object.#{t.variable})); step++; return CONTINUE;"
            }
            f.puts "        default: return COMPLETE;"
            f.puts "        }"
            f.puts "    }"
            f.puts "};"
            f.puts "template<>"
            f.puts "struct Writer <#{cname}>: base::Frame {"
            f.puts "    #{cname} &object;"
            f.puts "    Writer (#{cname} &o): object(o) {}"
            f.puts "    STATE run (base::Stack *stack) {"
            f.puts "        switch (step) {"
            exp.fieldTypes.each_index { |i|
                t = exp.fieldTypes[i]
                f.puts "        case #{i}: stack->push(new(stack->allocate(sizeof(Writer<#{t.cname}>))) Writer<#{t.cname}>(object.#{t.variable})); step++; return CONTINUE;"
            }
            f.puts "        default: return COMPLETE;"
            f.puts "        }"
            f.puts "    }"
            f.puts "};"
            f.puts "}"
            f.puts "\#endif"
        }
    }

    pkg.structs.each { |name, st|
        cname = "#{pkg.name}::#{name}"

        open(name+".h", "w") { |f|
            f.puts "\#ifndef #{name.upcase}_H"
            f.puts "\#define #{name.upcase}_H"
            f.puts "\#include \"Remote.h\""
            st.externalTypes.each { |t|
                f.puts "\#include \"#{t.name}.h\""
            }
            f.puts
            f.puts "namespace #{pkg.name} {"
            f.puts "struct #{name} {"
            st.fieldTypes.each { |t|
                f.puts "    #{t.cnamevar};"
            }
            f.puts "};"
            f.puts "}"
            f.puts
            f.puts "namespace rop {"
            f.puts "template<>"
            f.puts "struct Reader <#{cname}>: base::Frame {"
            f.puts "    #{cname} &object;"
            f.puts "    Reader (#{cname} &o): object(o) {}"
            f.puts "    STATE run (base::Stack *stack) {"
            f.puts "        switch (step) {"
            st.fieldTypes.each_index { |i|
                t = st.fieldTypes[i]
                f.puts "        case #{i}: stack->push(new(stack->allocate(sizeof(Reader<#{t.cname}>))) Reader<#{t.cname}>(object.#{t.variable})); step++; return CONTINUE;"
            }
            f.puts "        default: return COMPLETE;"
            f.puts "        }"
            f.puts "    }"
            f.puts "};"
            f.puts "template<>"
            f.puts "struct Writer <#{cname}>: base::Frame {"
            f.puts "    #{cname} &object;"
            f.puts "    Writer (#{cname} &o): object(o) {}"
            f.puts "    STATE run (base::Stack *stack) {"
            f.puts "        switch (step) {"
            st.fieldTypes.each_index { |i|
                t = st.fieldTypes[i]
                f.puts "        case #{i}: stack->push(new(stack->allocate(sizeof(Writer<#{t.cname}>))) Writer<#{t.cname}>(object.#{t.variable})); step++; return CONTINUE;"
            }
            f.puts "        default: return COMPLETE;"
            f.puts "        }"
            f.puts "    }"
            f.puts "};"
            f.puts "}"
            f.puts "\#endif"
        }
    }
    pkg.interfaces.each { |name, intf|
        cname = "#{pkg.name}::#{name}"
        open("#{name}.h", "w") { |f|
            f.puts "\#ifndef #{name.upcase}_H"
            f.puts "\#define #{name.upcase}_H"
            f.puts "\#include \"Remote.h\""
            intf.externalTypes.each { |t|
                f.puts "\#include \"#{t.name}.h\""
            }
            f.puts "namespace #{pkg.name} {"
            f.puts "struct #{name}: rop::Interface {"
            intf.methods.each {|m|
                f.puts "    virtual #{m.returnTypes[0].cname} #{m.name} (#{
                        m.argumentTypes.map{|t|t.cnamevar}.join(", ")
                        }) = 0;"
            }
            f.puts "};"
            f.puts "}"
            f.puts "namespace rop {"
            f.puts "template<>"
            f.puts "struct Reader<base::Ref<#{cname}> >: InterfaceReader<#{cname}> {"
            f.puts "    Reader (base::Ref<#{cname}> &o): InterfaceReader<#{cname}>(o) {}"
            f.puts "};"
            f.puts "template<>"
            f.puts "struct Writer<base::Ref<#{cname}> >: InterfaceWriter<#{cname}> {"
            f.puts "    Writer (base::Ref<#{cname}> &o): InterfaceWriter<#{cname}>(o) {}"
            f.puts "};"
            f.puts "template<>"
            f.puts "struct Stub<#{cname}>: #{cname} {"
            intf.methods.each_index { |mi|
                m = intf.methods[mi]
                rt = m.returnTypes[0]
                async = rt.name == "async"


                f.puts "    #{rt.cname} #{m.name} (#{
                        m.argumentTypes.map{|t|t.cnamevar}.join(", ")
                        }) {"
                f.puts "        Transport *trans = remote->registry->transport;"
                f.puts "        Port *p = trans->getPort();"
                f.puts ""
                m.argumentTypes.each { |t|
                    f.puts "        Writer<#{t.cname}> __arg_#{t.variable}(#{t.variable});"
                }
                f.puts "        RequestWriter<#{m.argumentTypes.size}> req(#{
                        async ? 1 : 0}<<6, remote->id, #{mi});"
                m.argumentTypes.each_index { |i|
                    t = m.argumentTypes[i]
                    f.puts "        req.args[#{i}] = &__arg_#{t.variable};"
                }
                f.puts "        p->writer.push(&req);"
                if async
                    f.puts "        p->flush();"
                    f.puts "    }"
                    next
                end

                f.puts
                f.puts "        ReturnReader<#{
                        m.returnTypes.map{|t|t.cname}.join(", ")
                        }> ret;"
                f.puts "        p->addReturn(&ret);"
                f.puts "        p->flushAndWait();"
                f.puts "        switch(ret.index) {"
                if rt.name == "void"
                    f.puts "        case 0: return;"
                else
                    f.puts "        case 0: return ret.get<#{rt.cname}>(0);"
                end
                (1...m.returnTypes.size).each { |i|
                    t = m.returnTypes[i];
                    f.puts "        case #{i}: throw ret.get<#{t.cname}>(#{i});"
                }
                f.puts "        default: throw rop::RemoteException();"
                f.puts "        }"
                f.puts "    }"
            }
            f.puts "};"
            f.puts "template<>"
            f.puts "struct Skeleton<#{cname}>: SkeletonBase {"
            f.puts "    Skeleton (rop::Interface *o): SkeletonBase(o) {}"
            intf.methods.each { |m|
                rt = m.returnTypes[0]
                async = rt.name == "async"
                f.puts "    struct __req_#{m.name}: Request {"
                f.puts "        #{cname} *object;"
                f.puts "        ArgumentsReader<#{
                        m.argumentTypes.map{|t| t.cname}.join(", ")
                        }> args;" unless m.argumentTypes.empty?
                f.puts "        ReturnWriter<#{
                        m.returnTypes.map{|t|t.cname}.join(", ")
                        }> ret;" unless async
                f.puts "        __req_#{m.name} (#{cname} *o): object(o) {"
                f.puts "            argumentsReader = #{
                        m.argumentTypes.empty? ? 0 : "&args"};"
                f.puts "            returnWriter = #{async ? 0 : "&ret"};"
                f.puts "        }"
                f.puts "        void call () {"
                args = (0...m.argumentTypes.size).map {|i|
                        t = m.argumentTypes[i]
                        "args.get<#{t.cname}>(#{i})"
                }.join(", ")
                if async
                    f.puts "            object->#{m.name}(#{args});"
                else
                    f.puts "            try {"
                    if rt.name == "void"
                        f.puts "                object->#{m.name}(#{args});"
                        f.puts "                ret.index = 0;"
                    else
                        f.puts "                ret.get<#{rt.cname}>(0) = object->#{m.name}(#{args});"
                        f.puts "                ret.index = 0;"
                    end
                    (1...m.returnTypes.size).each { |i|
                        t = m.returnTypes[i]
                        f.puts "            } catch (#{t.cname} &e) {"
                        f.puts "                ret.get<#{t.cname}>(#{i}) = e;"
                        f.puts"                 ret.index = #{i};"
                    }
                    f.puts "            } catch (...) {"
                    f.puts "                ret.index = -1;"
                    f.puts "            }"
                end
                f.puts "        }"
                f.puts "    };"
            }
            f.puts "    Request *createRequest (int mid) {"
            f.puts "        #{cname} *o = static_cast<#{cname}*>(object.get());"
            f.puts "        switch (mid) {"
            intf.methods.each_index { |i|
                m = intf.methods[i];
                f.puts "        case #{i}: return new __req_#{m.name}(o);"
            }
            f.puts "        default: return 0;"
            f.puts "        }"
            f.puts "    }"
            f.puts "};"

            f.puts "}"
            f.puts "\#endif"
        }
    }
}
