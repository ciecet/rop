class TypeNode
    def cname
        case name
        when "i8"; "I8"
        when "i16"; "I16"
        when "i32"; "I32"
        when "i64"; "I64"
        when "String"; "STRING"
        when "void"; "VOID"
        when "async"; throw "Unexpected cname request of async"
        when "Nullable"; %([NullableCodec, #{subTypes[0].cname}])
        when "List"; %([ListCodec, #{subTypes[0].cname}])
        when "Map"
            %([MapCodec, #{subTypes[0].cname}, #{subTypes[1].cname}])
        else
            %("#{package.name}.#{name}")
        end
    end
end

def generate pkgs
    pkgs.each { |pkg|
        open(pkg.name+".js", "w") { |f|
            pkg.structs.each { |name, st|
                f.puts %(Types["#{pkg.name}.#{name}"] = [StructCodec, #{
                    st.fieldTypes.map{|t|
                        %(#{t.cname}, "#{t.variable}")
                    }.join(", ")
                }])
            }
            pkg.exceptions.each { |name, exp|
                f.puts %(Types["#{pkg.name}.#{name}"] = [ExceptionCodec,"#{
                    pkg.name
                }.#{name}", #{
                    exp.fieldTypes.map{|t|
                        %(#{t.cname}, "#{t.variable}")
                    }.join(", ")
                }])
            }
            pkg.interfaces.each { |name, intf|
                f.puts %(Types["#{pkg.name}.#{name}"] = [InterfaceCodec, #{
                    intf.methods.map{|m|
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
                }])
            }
        }
    }
end
