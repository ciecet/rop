################################################################################
## C++ Specific Only

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
            ts << t
            next unless t.subTypes
            t.subTypes.each { |t2|
                ts << t2
            }
        }
        ts.delete_if { |t| CPP_TYPES.has_key?(t.name) }
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
        ts.delete_if { |t| CPP_TYPES.has_key?(t.name) }
        ts.uniq
    end
end

class TypeNode
    def to_cpp
        CPP_TYPES[name] || "#{package.name}::#{name}"
    end

    def cname
        return "base::Ref<#{to_cpp}> " if package.interfaces.has_key?(name)

        case name
        when "Map"
            "#{to_cpp}<#{subTypes[0].cname}, #{subTypes[1].cname}> "
        when "List"
            "#{to_cpp}<#{subTypes[0].cname}> "
        when "Nullable"
            if package.interfaces.has_key?(subTypes[0].name)
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

    def cnamearg
        "#{cname} #{'&' if package.structs.has_key?(name)}#{variable}"
    end
end

def generate packages

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
                            m.argumentTypes.map{|t|t.cnamearg}.join(", ")
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
                            m.argumentTypes.map{|t|t.cnamearg}.join(", ")
                            }) {"
                    f.puts "        Port *__p = remote->registry->getPort();"
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
                    f.puts "        __p->writer.push(&req);"
                    if async
                        f.puts "        __p->send(0);"
                        f.puts "    }"
                        next
                    end

                    f.puts
                    f.puts "        ReturnReader<#{
                            m.returnTypes.map{|t|t.cname}.join(", ")
                            }> ret;"
                    f.puts "        __p->sendAndWait(&ret);"
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
end
