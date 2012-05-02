#!/usr/bin/ruby
$: << File.dirname(__FILE__)
require 'parser'

=begin

file: package <name> { definitions }
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

StructNode = Struct.new(:name, :fieldTypes)
ExceptionNode = Struct.new(:name)
InterfaceNode = Struct.new(:name, :methods)
MethodNode = Struct.new(:name, :returnTypes, :argTypes)
TypeNode = Struct.new(:name, :subTypes, :variable)

NAME = /[a-zA-Z_][a-zA-Z0-9_]*/

parser = RDParser.new do
    token(/\b(package|struct|exception|interface)\b/) { |r| r.intern }
    token(/[<>{}();,]/) { |s| s }
    token(NAME) { |s| s }
    token(/\s*/)

    start :file do
        match(:package, NAME, "{", :definitions, "}") { |a,b,c,d,e| { b => d } }
    end

    rule :definitions do
        match(:definition) { |a| [a] }
        match(:definitions, :definition) { |a,b| a+[b] }
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
        match(:syncReturnType, NAME, :args, "throws", :Exceptions, ";") {
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

    rule :Exceptions do
        match(:exception) { |a| [a] }
        match(:Exceptions, ",", :exception) { |a,b,c| a+[c] }
    end

    rule :exception do
        match(NAME) { |a| ExceptionNode.new(a) }
    end
end

packages = parser.parse(`cat echo.idl`)

Structs = {}
Interfaces = {}
Exceptions = {}

packages.each { |k,v|
    v.each { |d|
        case d
        when StructNode
            Structs[d.name] = d
        when InterfaceNode
            Interfaces[d.name] = d
        when ExceptionNode
            Exceptions[d.name] = d
        end
    }
}

def checkType t
    return if %w(String Map List Ref i8 i16 i32 i16).include?(t.name)
    if t.subTypes
        t.subTypes.each { |st|  
            checkType st
        }
    end
    return if Structs.has_key?(t.name)
    throw "Unknown type #{t.name}"
end

Structs.each { |_,s|
    s.fieldTypes.each { |t|
       checkType t
    }
}

Interfaces.each { |_,intf|
    intf.methods.each { |m|
        m.returnTypes.each { |t|
            checkType t
        }
        m.argTypes.each { |t|
            checkType t
        }
    }
}

