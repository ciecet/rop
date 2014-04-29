require 'strscan'

class RDParser
    attr_accessor :pos
    attr_reader :rules

    Token = Struct.new(:value, :source, :pos)
    class Token
        def line
            source[0..pos-1].count("\n")+1
        end
    end

    def initialize(&block)
        @lex_tokens = []
        @rules = {}
        @start = nil
        instance_eval(&block)
    end

    def parse string, filename="<unknown>"
        @tokens = []
        if string.nil?
          raise "No input stream. terminate token scanning.!!!"
        else
          scanner = StringScanner.new(string)
        end

        # input stream tokenization.
        until scanner.eos?
          raise "unable to lex analyzing" unless @lex_tokens.any? do |tok|
            #puts "pattern: #{tok.pattern}"
            pos = scanner.pos
            if scanner.scan(tok.pattern)
              #puts "token: #{scanner.matched}"
              if tok.block
                  @tokens << Token.new(tok.block.call(scanner.matched),
                        string, pos)
              end
              true
            elsif
              false
            end
          end
        end

        # start parser
        @pos = 0
        @max_pos = 0
        @expected = []
        result = @start.parse
        #puts "pos:#{@pos} tokens:#{@tokens.inspect} maxpos:#{@max_pos}"
        if @pos != @tokens.size
            t = @tokens[@max_pos]
            if t
                STDERR.puts "Parse error at:#{filename}:#{t.line}"
                STDERR.puts "expected: '#{@expected.join(', ')}'"
                STDERR.puts "found: '#{t.value}'"
            else
                STDERR.puts "Unexpected EOF from #{filename}"
            end
            exit 1
        end
        return result
    end

    def next_token
        @pos += 1
        return @tokens[@pos - 1]
    end

    def expect(tok)
        t = next_token
        t = t && t.value
        if @pos - 1 > @max_pos
            @max_pos = @pos - 1
            @expected = []
        end
        return t if tok === t
        @expected << tok if @max_pos == @pos - 1 && !@expected.include?(tok)
        return nil
    end

    private

    LexToken = Struct.new(:pattern, :block)

    def token(pattern, &block)
        @lex_tokens << LexToken.new(pattern, block)
    end

    def start(name, &block)
        rule(name, &block)
        @start = @rules[name]
    end

    def rule(name)
        @current_rule = Rule.new(name, self)
        @rules[name] = @current_rule
        yield
        @current_rule = nil
    end

    def match(*pattern, &block)
        @current_rule.add_match(pattern, block)
    end

    class Rule
        Match = Struct.new :pattern, :block

        def initialize(name, parser)
            @name = name
            @parser = parser
            @matches = []
            @lrmatches = []
        end

        def add_match(pattern, block)
            match = Match.new(pattern, block)
            if pattern[0] == @name
                pattern.shift
                @lrmatches << match
            else
                @matches << match
            end
        end

        def parse
            #puts "matches:#{@matches.inspect}"
            match_result = try_matches(@matches)
            #puts "result:#{match_result.inspect}"
            return nil unless match_result
            loop do
                result = try_matches(@lrmatches, match_result)
                return match_result unless result
                match_result = result
            end
        end

        private

        def try_matches(matches, pre_result = nil)
            match_result = nil
            start = @parser.pos
            matches.each do |match|
                r = pre_result ? [pre_result] : []
                match.pattern.each do |token|
                    #puts "for token:#{token}"
                    if @parser.rules[token]
                        r << @parser.rules[token].parse
                        unless r.last
                            r = nil
                            break
                        end
                    else
                        nt = @parser.expect(token)
                        if nt
                            r << nt
                        else
                            r = nil
                            break
                        end
                    end
                end
                if r
                    if match.block
                        match_result = match.block.call(*r)
                    else
                        match_result = r[0]
                    end
                    break
                else
                    @parser.pos = start
                end
            end
            return match_result
        end
    end
end

=begin

packages: [packages] [comments] package <name> { definitions }
definitions: definitions | structdef | interfacedef
structdef: [comments] struct <name> { fields }
fields: fields field | field
field: [comments] type <var>;
type: Map<type,type> | List<type> | Nullable<type> | struct_name | interface_name
interfacedef: [comments] interface <name> { methods }
methods: methods method
method: [comments] return_type <method> args; | [comments] sync_return_type <method> args throws Exceptions;
return_type: sync_return_type | async
args: () | (arglist)
arglist: arglist, arg | arg
arg: type <var>

comments: (?m)\/\*\*.*?\*\/ | \/\/.* | (?m)\/\*.*?\*\/

=end

PREDEFINED_TYPES = %w(void async String Map List Nullable Variant i8 i16 i32 i64 bool
        f32 f64)
class PackageNode
    attr_accessor :name, :defs, :comments, :structs, :exceptions, :interfaces
    attr_accessor :group, :source

    def initialize a, b, c=nil
        @name = a
        @defs = b
        @comments = c
    end

    def merge pkg
        @comments.concat pkg.comments
        pkg.defs.each { |n, d|
            if @defs.has_key?(n)
                throw "Duplicate definition of #{n} in #{@name}"
            end

            @defs[n] = d
        }
    end

    def resolve
        @structs.each { |_,s|
            s.package = self
            s.fieldTypes.each { |t|
                t.resolve self
            }
        }
        @exceptions.each { |_,e|
            e.package = self
            e.fieldTypes.each { |t|
                t.resolve self
            }
        }
        @interfaces.each { |_,intf|
            intf.package = self

            intf.consts.each { |c|
                c.constType.resolve self
            }

            intf.methods.each { |m|
                m.returnTypes.each { |t|
                    t.resolve self
                }
                m.returnTypes[1..-1].each { |t|
                    next if ExceptionNode === t.definition
                    throw "Expected exception type #{t.name}"
                }
                m.argumentTypes.each { |t|
                    t.resolve self
                }
            }
            if intf.baseInterface
                intf.baseInterface.resolve self
                unless InterfaceNode === intf.baseInterface.definition
                    throw "Expected interface for base type of #{
                            intf.name}"
                end
            end
        }
    end
end
StructNode = Struct.new(:name, :fieldTypes, :package, :comments, :source)
ExceptionNode = Struct.new(:name, :fieldTypes, :package, :comments, :source)

class InterfaceNode
    attr_accessor :name, :baseInterface, :consts, :methods
    attr_accessor :package, :comments, :source
    def initialize name, base, consts, methods, comments=nil
        @name = name
        @baseInterface = base
        @consts = consts
        @methods = methods
        @comments = comments
    end

    def fullMethods
        l = []
        if baseInterface
            baseInterface.definition.fullMethods + methods
        else
            methods
        end
    end
end

class MethodNode
    attr_accessor :name, :returnTypes, :argumentTypes
    attr_accessor :interface, :comments
    def initialize name, returnTypes, argumentTypes, comments=nil
        @name = name
        @returnTypes = returnTypes
        @argumentTypes = argumentTypes
        @comments = comments
    end

    def async?
        returnTypes[0].name == "async"
    end
end

class TypeNode
    attr_accessor :name, :subTypes, :variable
    attr_accessor :definition, :comments

    def initialize name, subTypes=nil, variable=nil , comments=nil
        @name = name
        @subTypes = subTypes
        @variable = variable
        @comments = comments
    end

    def resolve p
        if subTypes
            subTypes.each { |st|
                st.resolve p
            }
        end

        return unless Array === @name
        if @name.length == 1
            @name = @name[0]
            return if PREDEFINED_TYPES.include?(@name)
            if p.defs.has_key?(@name)
                @definition = p.defs[@name]
                @name = "#{p.name}.#{@name}"
                return
            end
            throw "Unknown type #{name} referred in package #{p.name}"
        else
            pkgname = @name[0...-1].join(".")
            dp = p.group[pkgname]
            throw "Package '#{pkgname}' not found" unless dp
            @definition = dp.defs[@name.last]
            unless definition
                throw "Definition #{name.last} not found in package #{dp.name}"
            end
            @name = @name.join(".")
        end
    end
end

class ConstNode
    attr_accessor :constType, :value
    def initialize ctype, val
        @constType = ctype
        @value = val
    end
end

ConstToken = Struct.new(:value)

SYMBOL = /[a-zA-Z_][a-zA-Z0-9_]*/
DOC_COMMENT = /\/\*\*.*?\*\//m
NUMBER = /-?[0-9]+(\.[0-9]+)?/
STRING = /"(\\.|[^\\"])*"/

IDLParser = RDParser.new do
    token(/\s+/)
    token(DOC_COMMENT) { |c| c }
    token(/(\/\/.*)|((?m)\/\*.*?\*\/)/)
    token(/\b(package|struct|exception|interface)\b/) { |r| r.intern }
    token(SYMBOL) { |s| s }
    token(NUMBER) { |n| ConstToken.new(eval(n)) }
    token(STRING) { |s| ConstToken.new(eval(s)) }
    token(/./) { |s| s }

    start :packages do
        match(:comments, :package, SYMBOL, "{", :definitions, "}") { |a,b,c,d,e,f|
                [ PackageNode.new(c,e,a) ] }
        match(:packages, :comments, :package, SYMBOL, "{", :definitions, "}") { |p,a,b,c,d,e,f|
                p << PackageNode.new(c,e,a) }
    end

    rule :definitions do
        match(:definition) { |a| {a.name => a} }
        match(:definitions, :definition) { |a,b| a[b.name] = b; a }
    end

    rule :definition do
        match(:comments, :struct, SYMBOL, "{", :fields, "}") { |a,b,c,d,e,f|
                StructNode.new(c,e,nil,a)}
        match(:comments, :exception, SYMBOL, "{", :fields, "}") {|a,b,c,d,e,f|
                ExceptionNode.new(c,e,nil,a)}
        match(:comments, :interface, SYMBOL, "{", :members, "}"
                ) { |comments,_,name,_,members,_|
            InterfaceNode.new(name, nil,
                members.find_all { |m| ConstNode === m },
                members.find_all { |m| MethodNode === m },
                comments)
	}
        match(:comments, :interface, SYMBOL, "extends", :type, "{", :members,
                "}") { |comments,_,name,_,base,_,members,_|
            InterfaceNode.new(name, base,
                members.find_all { |m| ConstNode === m },
                members.find_all { |m| MethodNode === m },
                comments)
        }
    end

    rule :fields do
        match(:field) { |a| [a] }
        match(:fields, :field) { |a,b| a+[b] }
    end

    rule :field do
        match(:comments, :type, SYMBOL, ";") {|a,b,c,d|
	    b.variable = c
	    b.comments = a
	    b
	}
    end

    rule :type do
        match("Map", "<", :type, ",", :type, ">") { |a,b,c,d,e,f|
                TypeNode.new(a, [c, e])}
        match("List", "<", :type, ">") { |a,b,c,d|
                TypeNode.new(a, [c])}
        match("Nullable", "<", :type, ">") {|a,b,c,d|
                TypeNode.new(a, [c])}
        match(:typename) { |a| TypeNode.new(a) }
    end

    rule :typename do
        match(SYMBOL) { |a| [a]}
        match(:typename, ".", SYMBOL) { |a,b,c| a << c }
    end

    rule :members do
        match(:member) { |a| [a] }
        match(:members, :member) { |a,b| a+[b] }
        match() { [] }
    end

    rule :member do
        match(:comments, :type, SYMBOL, "=", :constValue, ";") {
                |comments,t,var,_,val,_|
            tname = t.name.join
            case tname
            when "i8", "i16", "i32", "i64"
                unless Fixnum === val || Bignum === val
                    throw "Expected integer value for constant: #{var}"
                end
                bits = tname[1..-1].to_i - 1
                range = (-2**bits..(2**bits-1))
                unless range === val
                    throw "Constant value #{val} out of range #{range}"
                end
            when "f32", "f64"
                unless Float === val
                    throw "Expected float value for constant: #{var}"
                end
            when "String"
                unless String === val
                    throw "Expected String value for constant: #{var}"
                end
            else
                throw "Unsupported type for constant value: #{tname}"
            end
            t.comments = comments
            t.variable = var
            ConstNode.new(t, val)
        }

        match(:comments, :returnType, SYMBOL, "(", :args, ")", ";") {
                |comments,rtype,name,_,args,_,_|
            MethodNode.new(name, [rtype], args, comments)
        }
        match(:comments, :syncReturnType, SYMBOL, "(", :args, ")", "throws",
                :exceptions, ";") { |comments,rtype,name,_,args,_,_,exps,_|
            MethodNode.new(name, [rtype] + exps, args, comments)
        }
    end

    rule :constValue do
        match(ConstToken) { |c| c.value }
    end

    rule :returnType do
        match("async") { |a| TypeNode.new(a) }
        match(:syncReturnType)
    end

    rule :syncReturnType do
        match("void") { |a| TypeNode.new(a) }
        match(:type) { |a|
            if a.name == "void" || a.name == "async"
                throw "#{a.name} can be placed as return type only."
            end
            a
        }
    end

    rule :args do
        match(:arg) { |a| [a] }
        match(:args, ",", :arg) { |a,b,c| a+[c] }
        match() { [] }
    end

    rule :arg do
        match(:type, SYMBOL) {|a,b| a.variable = b; a}
    end

    rule :exceptions do
        match(:type) { |a| [a] }
        match(:exceptions, ",", :type) { |a,b,c| a << c }
    end

    rule :comments do
        match(DOC_COMMENT) { |c| c }
        match() { "" }
    end
end

def mergeAndResolve pkgs
    group = {}

    # merge packages
    pkgs.each { |p|
        if group.has_key?(p.name)
            group[p.name].merge p
        else
            group[p.name] = p
        end
    }

    # resolve
    pkgs = group.values
    pkgs.each { |p|
        p.group = group

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
    pkgs.each { |p|
        p.resolve
    }

    # check method conflict
    pkgs.each { |p|
        p.interfaces.each { |iname, intf|
            methods = intf.fullMethods
            methods.map{|m|m.name}.sort.uniq.each{|m|
                uniqlist = methods.find_all {|m2| m2.name == m}
                unless uniqlist.size == 1
                    throw "#{iname} has duplicate method:#{m}"
                end
            }
        }
    }

    return pkgs
end
