################################################################################
## C++ Specific Only

require 'fileutils'

CPP_TYPES = {
    "String" => "std::string",
    "Map" => "std::map",
    "List" => "std::vector",
    "Nullable" => "acba::Ref",
    "i8" => "int8_t",
    "i16" => "int16_t",
    "i32" => "int32_t",
    "i64" => "int64_t",
    "async" => "void",
    "void" => "void",
    "bool" => "bool",
    "f32" => "float",
    "f64" => "double",
    "Variant" => "acba::Variant"
}

CPP_PRIMITIVES = %w{i8 i16 i32 i64 f32 f64 bool}

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
        ts.delete_if { |t| CPP_TYPES.has_key?(t.name) }
        ts.uniq {|t| t.name}
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
        ts.delete_if { |t| CPP_TYPES.has_key?(t.name) }
        ts.uniq {|t| t.name}
    end
end

class InterfaceNode
    def externalTypes
        ts = []
        ts << baseInterface if baseInterface
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
        ts.uniq {|t| t.name}
    end
end

class MethodNode
    def arguments
        args = []
        rt = returnTypes[0]
        args << "#{rt.cname} &__ret" if rt.name != "void" && rt.name != "async"
        argumentTypes.each { |t|
            args << t.cnamearg
        }
        (1...returnTypes.size).each { |ti|
            t = returnTypes[ti]
            args << "acba::Ref<#{t.cname}> &__ex#{ti}"
        }
        return args
    end
end

class TypeNode
    def to_cpp
        CPP_TYPES[name] || name.gsub(".", "::")
    end

    def cname
        return "acba::Ref<#{to_cpp}> " if InterfaceNode === definition

        case name
        when "Map"
            "#{to_cpp}<#{subTypes[0].cname}, #{subTypes[1].cname}> "
        when "List"
            "#{to_cpp}<#{subTypes[0].cname}> "
        when "Nullable"
            case subTypes[0].definition
            when ExceptionNode
                "#{to_cpp}<#{subTypes[0].cname}> "
            else
                if CPP_PRIMITIVES.include?(subTypes[0].name)
                    puts "!!! Nullable against primitive type is "+
                            "currently not supported in C++"
                end
                "#{to_cpp}<acba::RefCounted<#{subTypes[0].cname}> > "
            end
        else
            to_cpp
        end
    end

    def cnamevar
        "#{cname} #{variable}"
    end

    def cnamearg
        case name
        when "bool", "i8", "i16", "i32", "i64", "f32", "f64"
            "#{cname} #{variable}"
        else
            "const #{cname} &#{variable}"
        end
    end
end

class ConstNode
    def cnamevar
        if @constType.name == "String"
            "char #{@constType.variable}[sizeof(#{@value.inspect})]"
        else
            @constType.cnamevar
        end
    end
    def literal
        case @constType.name
        when "i64"
            @value.inspect+"l"
        else
            @value.inspect
        end
    end
end

def generate packages

    packages.each { |pkg|

        FileUtils.mkdir_p(pkg.name)

        pkg.exceptions.each { |name, exp|
            cname = "#{pkg.name}::#{name}"

            open("#{pkg.name}/#{name}_fwd.h", "w") { |f|
                f.puts <<-TEMPLATE
#ifndef #{pkg.name.upcase}_#{name.upcase}_FWD_H
#define #{pkg.name.upcase}_#{name.upcase}_FWD_H
#include "rop.h"

namespace #{pkg.name} {
    struct #{name};
}

#{exp.externalTypes.map { |t| %(\
#include "#{t.name.gsub(".", "/")}_fwd.h"
) }.join}\

namespace #{pkg.name} {
    struct #{name}: rop::RemoteException {
#{exp.fieldTypes.map { |t| %(\
        #{t.cnamevar};
) }.join}\

        #{name} () {}
        #{name} (#{(0...exp.fieldTypes.size).map{ |i| 
            "const #{exp.fieldTypes[i].cname} &arg#{i}"
        }.join(", ")}): #{(0...exp.fieldTypes.size).map{|i|
            "#{exp.fieldTypes[i].variable}(arg#{i})"
        }.join(", ")} {}
    };
}

namespace rop {

template <>
struct Reader<#{cname}> {
    static inline void run (#{cname} &o, acba::Buffer &buf);
};

template <>
struct Writer<#{cname}> {
    static inline void run (const #{cname} &o, acba::Buffer &buf);
};

}

#endif
                TEMPLATE
            }

            open("#{pkg.name}/#{name}.h", "w") { |f|
                f.puts <<-TEMPLATE
#ifndef #{pkg.name.upcase}_#{name.upcase}_H
#define #{pkg.name.upcase}_#{name.upcase}_H
#include "rop.h"
#include "#{pkg.name.gsub(".", "/")}/#{name}_fwd.h"
#{exp.externalTypes.map { |t| %(\
#include "#{t.name.gsub(".", "/")}.h"
) }.join}\

namespace rop {

inline void Reader<#{cname}>::run (#{cname} &o, acba::Buffer &buf) {
#{exp.fieldTypes.map { |t| %(\
    Reader<#{t.cname}>::run(o.#{t.variable}, buf);
) }.join}\
}

inline void Writer<#{cname}>::run (const #{cname} &o, acba::Buffer &buf) {
#{exp.fieldTypes.map { |t| %(\
    Writer<#{t.cname}>::run(o.#{t.variable}, buf);
) }.join}\
}

}

#endif
                TEMPLATE
            }
        }

        pkg.structs.each { |name, st|
            cname = "#{pkg.name}::#{name}"

            open("#{pkg.name}/#{name}_fwd.h", "w") { |f|
                f.puts <<-TEMPLATE
#ifndef #{pkg.name.upcase}_#{name.upcase}_FWD_H
#define #{pkg.name.upcase}_#{name.upcase}_FWD_H
#include "rop.h"

namespace #{pkg.name} {
    struct #{name};
}

#{st.externalTypes.map { |t| %(\
#include "#{t.name.gsub(".", "/")}_fwd.h"
) }.join}\

namespace #{pkg.name} {
    struct #{name} {
#{st.fieldTypes.map { |t| %(\
        #{t.cnamevar};
) }.join}\
    };
}

namespace rop {

template <>
struct Reader<#{cname}> {
    static inline void run (#{cname} &o, acba::Buffer &buf);
};

template <>
struct Writer<#{cname}> {
    static inline void run (const #{cname} &o, acba::Buffer &buf);
};

}

#endif
                TEMPLATE
            }
            open("#{pkg.name}/#{name}.h", "w") { |f|
                f.puts <<-TEMPLATE
#ifndef #{pkg.name.upcase}_#{name.upcase}_H
#define #{pkg.name.upcase}_#{name.upcase}_H
#include "rop.h"
#include "#{pkg.name.gsub(".", "/")}/#{name}_fwd.h"
#{st.externalTypes.map { |t| %(\
#include "#{t.name.gsub(".", "/")}.h"
) }.join}\

namespace rop {

inline void Reader<#{cname}>::run (#{cname} &o, acba::Buffer &buf) {
#{st.fieldTypes.map { |t| %(\
    Reader<#{t.cname}>::run(o.#{t.variable}, buf);
) }.join}\
}

inline void Writer<#{cname}>::run (const #{cname} &o, acba::Buffer &buf) {
#{st.fieldTypes.map { |t| %(\
    Writer<#{t.cname}>::run(o.#{t.variable}, buf);
) }.join}\
}

}

#endif
                TEMPLATE
            }
        }

        pkg.interfaces.each { |name, intf|
            fullMethods = intf.fullMethods
            cname = "#{pkg.name}::#{name}"
            open("#{pkg.name}/#{name}_fwd.h", "w") { |f|
                f.puts <<-TEMPLATE
#ifndef #{pkg.name.upcase}_#{name.upcase}_FWD_H
#define #{pkg.name.upcase}_#{name.upcase}_FWD_H
#include "rop.h"

namespace #{pkg.name} {
    struct #{name};
}

#{intf.externalTypes.map { |t| %(\
#include "#{t.name.gsub(".", "/")}_fwd.h"
) }.join}\

namespace #{pkg.name} {

struct #{name}: #{
    if intf.baseInterface
        intf.baseInterface.to_cpp
    else
        "rop::Interface"
    end
} {
#{intf.consts.map { |c|
    if c.constType.name == "String" then %(\
    static const #{c.cnamevar};
) else %(\
    static const #{c.cnamevar} = #{c.literal};
) end
}.join}\
#{intf.methods.map { |m| %(\
    virtual int32_t #{m.name} (#{m.arguments.join(", ")}) = 0;
) }.join}\
};

}

namespace rop {

template <>
struct Reader<acba::Ref<#{cname}> > {
    static inline void run (acba::Ref<#{cname}> &o, acba::Buffer &buf);
};

template <>
struct Writer<acba::Ref<#{cname}> > {
    static inline void run (const acba::Ref<#{cname}> &o, acba::Buffer &buf);
};

template <>
struct Stub<#{cname}>: #{cname} {
#{(0...fullMethods.size).map { |mi|
    m = fullMethods[mi]
    %(\
    inline virtual int32_t #{m.name} (#{m.arguments.join(", ")});
)}.join}\
};

template <>
struct Skeleton<#{cname}>: rop::SkeletonBase {

    Skeleton (#{cname} *o): rop::SkeletonBase(o) {}
#{(0...fullMethods.size).map { |mi|
    m = fullMethods[mi]
    next if m.argumentTypes.size == 0
    %(\
    typedef acba::Tuple<#{
        m.argumentTypes.map { |t| t.cname }.join(", ")
    }> args_t_#{mi};
)}.join}\

    inline void readRequest (LocalCall &lc, acba::Buffer &buf);

#{(0...fullMethods.size).map { |mi|
    m = fullMethods[mi]
    args = []
    %(
    inline static void __call_#{m.name} (#{cname} *obj, LocalCall &lc);
)}.join}\
};

}
#endif
                TEMPLATE
            }
            open("#{pkg.name}/#{name}.h", "w") { |f|
                f.puts <<-TEMPLATE
#ifndef #{pkg.name.upcase}_#{name.upcase}_H
#define #{pkg.name.upcase}_#{name.upcase}_H
#include "rop.h"
#include "#{pkg.name.gsub(".", "/")}/#{name}_fwd.h"
#{intf.externalTypes.map { |t| %(\
#include "#{t.name.gsub(".", "/")}.h"
) }.join}\

namespace rop {

inline void Reader<acba::Ref<#{cname}> >::run (acba::Ref<#{cname}> &o, acba::Buffer &buf) {
    readInterface<#{cname}>(o, buf);
}

inline void Writer<acba::Ref<#{cname}> >::run (const acba::Ref<#{cname}> &o, acba::Buffer &buf) {
    writeInterface<#{cname}>(o, buf);
}

#{(0...fullMethods.size).map { |mi|
    m = fullMethods[mi]
    %(\
inline int32_t Stub<#{cname}>::#{m.name} (#{m.arguments.join(", ")}) {
#{if OPTIONS["trace"] then %(\
    printf("Stub<#{cname}> %d.#{m.name}\\n", remote->id);
) end}\
    acba::ScopedLock __l(remote->registry->monitor);

    RemoteCall __rc(#{m.async? ? 1 : 0}<<6, *remote, #{mi});
#{m.argumentTypes.map { |t| %(\
    Writer<#{t.cname}>::run(#{t.variable}, __rc);
)}.join}\
#{if m.returnTypes.size > 1 || (!m.async? && m.returnTypes[0].name != "void")
    o = []
    agents = []
    if m.returnTypes[0].name == "void"
        agents << "0"
    else
        o << "\
    ReturnReadAgent<#{m.returnTypes[0].cname}> __ret0(__ret);\n"
        agents << "&__ret0"
    end

    (1...m.returnTypes.size).each { |ti|
        t = m.returnTypes[ti]
        o << "\
    ExceptionReadAgent<#{t.cname}> __ret#{ti}(__ex#{ti});\n"
        agents << "&__ret#{ti}"
    }

    o << "\
    ReadAgent *__agents[] = {#{agents.join(", ")}};
    __rc.setReadAgents(__agents, #{m.returnTypes.size});\n"

    o.join
end}\

    return remote->registry->#{m.async? ?
        "asyncCall(__rc, false)" : "syncCall(__rc)" };
}

)}.join}\
inline void Skeleton<#{cname}>::readRequest (LocalCall &lc, acba::Buffer &buf) {
    switch (buf.readI16()) {
    default: return;
#{(0...fullMethods.size).map { |mi|
    m = fullMethods[mi]
    %(\
    case #{mi}:
#{if m.argumentTypes.size > 0 then %(\
        Reader<args_t_#{mi}>::run(*lc.allocate<args_t_#{mi}>(), buf);
) end}\
        lc.function = (LocalCall::Function)__call_#{m.name};
        break;
)}.join}\
    }
}
#{(0...fullMethods.size).map { |mi|
    m = fullMethods[mi]
    args = []
    %(
inline void Skeleton<#{cname}>::__call_#{m.name} (#{cname} *obj, LocalCall &lc) {
#{if OPTIONS["trace"] then %(\
    printf("Skeleton<#{cname}> %d.#{m.name}\\n", lc.skeleton->id);
) end}\
#{if m.argumentTypes.size > 0 then %(\
    args_t_#{mi} *args = lc.getArguments<args_t_#{mi}>();
) end}\
#{
    o = []
    rt = m.returnTypes[0]
    unless rt.name == "void" || rt.name == "async"
        args << "ret->value"
        o << %(\
    Return<#{rt.cname}> *ret = new Return<#{rt.cname}>();
    lc.returnValue = ret;\n)
    end
    (0...m.argumentTypes.size).each { |ti|
       args << "*args->at#{ti}()" 
    }
    (1...m.returnTypes.size).map { |ti|
        t = m.returnTypes[ti]
        args << "ex#{ti}"
        o << %(\
    acba::Ref<#{t.cname}> ex#{ti};\n)
    }
    o.join
}\
    {
        acba::ScopedUnlock ul(lc.registry->monitor);
#{
    if m.returnTypes[0].name == "async" then %(\
        obj->#{m.name}(#{args.join(", ")});)
    else %(\
        lc.returnIndex = obj->#{m.name}(#{args.join(", ")});)
    end
}
    }
#{if m.returnTypes.size > 1 then %(\
    switch (lc.returnIndex) {
    default: return;
#{(1...m.returnTypes.size).map { |ti| %(\
    case #{ti}:
        Writer<#{m.returnTypes[ti].cname}>::run(*ex#{ti}, lc);
        break;
)}.join}
    }
) end}\
}
)}.join}\

}
#endif
                TEMPLATE
            }
        }
    }

    constDefines = []
    packages.each { |pkg|
        pkg.interfaces.each { |name, intf|
            next if intf.consts.empty?
            constDefines << %(
#include "#{pkg.name}/#{name}.h"
namespace #{pkg.name} {
#{intf.consts.map { |c|
    ct = c.constType
    if ct.name == "String"
        %(const char #{name}::#{ct.variable}[] = #{c.literal};\n)
    end
}.join}\
}
)
        }
    }

    unless constDefines.empty?
        open("ropconsts.cpp", "w") { |f|
            f.puts constDefines.join
        }
    end
end
