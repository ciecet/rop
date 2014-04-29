class TypeNode
    def cname
        case name
        when "bool"; "BOOLEAN"
        when "i8"; "I8"
        when "i16"; "I16"
        when "i32"; "I32"
        when "i64"; "I64"
        when "f32"; "F32"
        when "f64"; "F64"
        when "String"; "STRING"
        when "void"; "VOID"
        when "Variant"; "VARIANT"
        when "async"; raise "Unexpected cname request of async"
        when "Nullable"; %([NullableCodec, #{subTypes[0].cname}])
        when "List"; %([ListCodec, #{subTypes[0].cname}])
        when "Map"
            %([MapCodec, #{subTypes[0].cname}, #{subTypes[1].cname}])
        else
            %("#{name}")
        end
    end
end

def generate pkgs
    externs = []
    mod = OPTIONS['module']

    open("#{File.basename(mod)}.js", "w") { |f|
        f.puts <<-TEMPLATE
acba.define("js:#{mod}", ["js:rop"], function(r) {
    var types = r.types
    var I8 = types.i8
    var I16 = types.i16
    var I32 = types.i32
    var I64 = types.i64
    var STRING = types.String
    var VOID = types.void
    var BOOLEAN = types.bool
    var F32 = types.f32
    var F64 = types.f64
    var VARIANT = types.Variant
    var NullableCodec = r.codecs.Nullable
    var ListCodec = r.codecs.List
    var MapCodec = r.codecs.Map
    var StructCodec = r.codecs.Struct
    var ExceptionCodec = r.codecs.Exception
    var InterfaceCodec = r.codecs.Interface
        TEMPLATE

        pkgs.each { |pkg|
            pkg.structs.each { |name, st|
                f.puts %(    types["#{pkg.name}.#{name}"] = [StructCodec, #{
                    st.fieldTypes.map{|t|
                        externs << t.variable
                        %(#{t.cname}, "#{t.variable}")
                    }.join(", ")
                }])
            }
            pkg.exceptions.each { |name, exp|
                f.puts %(    types["#{pkg.name}.#{name}"] = [ExceptionCodec,"#{
                    pkg.name
                }.#{name}", #{
                    exp.fieldTypes.map{|t|
                        externs << t.variable
                        %(#{t.cname}, "#{t.variable}")
                    }.join(", ")
                }])
            }
            pkg.interfaces.each { |name, intf|
                f.puts %(    types["#{pkg.name}.#{name}"] = [InterfaceCodec, #{
                    if intf.consts.empty?
                        "undefined"
                    else
                        %([#{
                            intf.consts.map { |c|
                                externs << c.constType.variable
                                %("#{c.constType.variable}", #{c.value.inspect})
                            }.join(", ")
                        }])
                    end
                }, [#{
                    intf.fullMethods.map{|m|
                        externs << m.name
                        %("#{m.name}", #{
                            if m.argumentTypes.empty?
                                "undefined"
                            else
                                %([#{
                                    m.argumentTypes.map{|a|a.cname}.join(", ")
                                }])
                            end
                        }, #{
                            if m.returnTypes[0].name == "async"
                                "undefined"
                            else
                                %([#{
                                    m.returnTypes.map{|r|r.cname}.join(", ")
                                }])
                            end
                        })
                    }.join(", ")
                }]])
            }
        }

        f.puts %[\
    return types
})]
    }

    externFile = OPTIONS["extern-file"]
    if externFile
        open(externFile, "w") { |f|
            f.puts externs.join("\n")
        }
    end
end
