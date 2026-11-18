import Lean
import DSP.IfLess
import DSP.FMA
import DSP.Transcendental

/-! A deliberately small backend for pure processor functions.
It reads elaborated Lean definitions, resolves their arithmetic instances,
and emits native C++ records. Generic export interprets the arithmetic parameter
as MultiChannelNumber. It is tested, not a verified Lean-to-C++ compiler.
-/

namespace DSP.Codegen

open Lean Meta Elab Command

inductive Shape where
  | scalar (type : String)
  | record (fields : Array (String × Shape))
  | vector (size : Nat) (element : Shape)
  deriving Inhabited

inductive Value where
  | scalar (type text : String)
  | record (fields : Array Value)
  | vector (elements : Array Value)
  deriving Inhabited

structure Emission where
  lines : Array String := #[]
  next : Nat := 0
  numberType : Option Expr := none
  arithmetic : Array FVarId := #[]
  usesMath : Bool := false

abbrev GenM := StateT Emission MetaM
abbrev Locals := List (FVarId × Value)

/-- Shared data records are exported before functions that use them. The
association is by Lean type, so kernels reuse one C++ type and header. -/
structure RecordExport where
  cppName : String
  header : String

initialize recordExports : SimplePersistentEnvExtension (Name × RecordExport) (NameMap RecordExport) ←
  registerSimplePersistentEnvExtension {
    addEntryFn := fun state (name, target) => state.insert name target
    addImportedFn := fun imports => imports.foldl (init := ∅) fun state entries =>
      entries.foldl (init := state) fun state (name, target) => state.insert name target }

def emit (line : String) : GenM Unit := modify fun s => { s with lines := s.lines.push line }

def fresh : GenM String := do
  let s ← get
  set { s with next := s.next + 1 }
  return s!"v{s.next}"

def scalar (v : Value) : GenM (String × String) :=
  match v with
  | .scalar type text => pure (type, text)
  | _ => throwError "C++ codegen: expected a scalar expression"

def temporary (type text : String) : GenM Value := do
  let name ← fresh
  emit s!"const {type} {name} = {text};"
  return .scalar type name

def scalarType (type : Expr) (numberType : Option Expr) : MetaM String := do
  let type ← whnf type
  if numberType == some type then return "Vector"
  match type with
  | .const ``Float _ => return "double"
  | .const ``Float32 _ => return "float"
  | .const ``Bool _ => return "bool"
  | _ => throwError "C++ codegen: unsupported field type {type}; expected Float, Float32 or Bool"

def validIdentifier (s : String) : Bool :=
  !s.isEmpty && !s.contains "__" &&
    s.toList.head!.isAlpha && s.toList.all (fun c => c.toNat < 128 && (c.isAlphanum || c == '_'))

def finSize (type : Expr) : MetaM Nat := do
  let type ← whnf type
  unless type.isAppOfArity ``Fin 1 do
    throwError "C++ codegen: only fixed Fin-indexed vectors are supported; got {type}"
  let .lit (.natVal n) ← whnf type.getAppArgs[0]!
    | throwError "C++ codegen: vector dimension must be a literal"
  unless n ≤ 256 do throwError "C++ codegen: vector dimension exceeds the 256-element export limit"
  return n

partial def fieldShape (type : Expr) (numberType : Option Expr)
    (numberName : String := "Vector") : MetaM Shape := do
  let type ← whnf type
  if numberType == some type then return .scalar numberName
  if let .forallE _ domain body _ := type then
    unless !body.hasLooseBVars do throwError "C++ codegen: dependent vector elements are unsupported"
    return .vector (← finSize domain) (← fieldShape body numberType numberName)
  if type.isConstOf ``Float || type.isConstOf ``Float32 || type.isConstOf ``Bool then
    return .scalar (← scalarType type numberType)
  let .const name _ := type.getAppFn
    | throwError "C++ codegen: unsupported field type {type}"
  let some info := getStructureInfo? (← getEnv) name
    | throwError "C++ codegen: unsupported field type {type}"
  if !info.parentInfo.isEmpty then
    throwError "C++ codegen: inherited records are not supported: {type}"
  withLocalDeclD `record type fun value => do
    let mut fields := #[]
    for i in [:info.fieldNames.size] do
      let raw := info.fieldNames[i]!.toString
      let member := if raw.startsWith "m_" then raw else "m_" ++ raw
      unless validIdentifier member do
        throwError "C++ codegen: invalid C++ field name {member}"
      if fields.any (fun f : String × Shape => f.1 == member) then
        throwError "C++ codegen: duplicate C++ field name {member}"
      let fieldType ← inferType (.proj name i value)
      fields := fields.push (member, ← fieldShape fieldType numberType numberName)
    return .record fields

partial def readValue (shape : Shape) (base : String) : Value :=
  match shape with
  | .scalar type => .scalar type base
  | .record fields => .record (fields.map fun (name, shape) => readValue shape s!"{base}.{name}")
  | .vector n shape => .vector ((List.range n).toArray.map fun i => readValue shape s!"{base}[{i}]")

def primitive (name : Name) : Bool :=
  #[``Trig.sinFloat, ``Trig.cosFloat, ``Trig.tanFloat,
    ``Trig.sinFloat32, ``Trig.cosFloat32, ``Trig.tanFloat32,
    ``ExpDecay.float, ``ExpDecay.float32, ``FMA.float, ``FMA.float32, ``Float.add, ``Float.sub, ``Float.mul, ``Float.div, ``Float.neg,
    ``Float32.add, ``Float32.sub, ``Float32.mul, ``Float32.div, ``Float32.neg,
    ``Float.ofNat, ``Float32.ofNat, ``Float.ofScientific, ``Float32.ofScientific,
    ``Float.lt, ``Float.le, ``Float32.lt, ``Float32.le,
    ``Float.beq, ``Float32.beq, ``ite, ``Bool.true, ``Bool.false, ``Eq,
    ``Bool.decEq, ``Float.decLt, ``Float.decLe, ``Float32.decLt, ``Float32.decLe].contains name

/-- Unfold one pure definition at a time, so unknown external implementations
cannot silently be replaced with their logical bodies. Value bindings stay
shared, so each is emitted once; only function-typed bindings are inlined. -/
partial def expose (expression : Expr) : MetaM Expr := withIncRecDepth do
  let e := expression.consumeMData.headBeta.consumeMData
  if e.isHeadBetaTarget then return ← expose e
  match e with
  | .letE _ type value body _ =>
    if (← whnf type).isForall then expose (body.instantiate1 value) else return e
  | .proj name index value =>
    let value ← expose value
    if let .letE binder type bound body nondep := value then
      return .letE binder type bound (.proj name index body) nondep
    if let .const ctor _ := value.getAppFn then
      if let .ctorInfo info ← getConstInfo ctor then
        let args := value.getAppArgs
        if info.induct == name && info.numParams + index < args.size then
          return ← expose args[info.numParams + index]!
    return .proj name index value
  | _ =>
    match e.getAppFn with
    | .const name levels =>
      if name == ``Nat.zero then return .lit (.natVal 0)
      if name == ``Nat.succ && e.getAppArgs.size == 1 then
        if let .lit (.natVal n) ← expose e.getAppArgs[0]! then return .lit (.natVal (n + 1))
      if let some (_, op) := (#[(``Nat.add, Nat.add), (``Nat.sub, Nat.sub),
          (``Nat.mul, Nat.mul), (``Nat.mod, Nat.mod), (``Nat.div, Nat.div)]).find?
          (fun entry => entry.1 == name) then
        let args := e.getAppArgs
        if args.size < 2 then return e
        if args.size == 2 then
          let a ← expose args[0]!
          let b ← expose args[1]!
          if let (.lit (.natVal a), .lit (.natVal b)) := (a, b) then
            return .lit (.natVal (op a b))
        throwError "C++ codegen: natural arithmetic must be statically known: {e}"
      -- Topology selection is compile-time natural arithmetic. Reduce only
      -- these known predicates; runtime arithmetic still uses the checked
      -- primitive paths below, including their external-operation boundary.
      if name == ``ite && e.getAppArgs.size == 5 then
        let args := e.getAppArgs
        let proposition := args[1]!
        let predicate := proposition.getAppFn.constName?
        let operands := proposition.getAppArgs
        if predicate == some ``LT.lt && operands.size == 4 &&
            operands[0]!.isConstOf ``Nat && operands[1]!.isConstOf ``instLTNat &&
            args[2]!.getAppFn.isConstOf ``Nat.decLt then
          let left ← expose operands[2]!
          let right ← expose operands[3]!
          if let (.lit (.natVal left), .lit (.natVal right)) := (left, right) then
            let yes := left < right
            return ← expose args[if yes then 3 else 4]!
      if primitive name then return e
      let env ← getEnv
      if (getExternAttrData? env name).isSome || (Compiler.getImplementedBy? env name).isSome then
        throwError "C++ codegen: unsupported external implementation {name}"
      let args := e.getAppArgs
      if let some proj := env.getProjectionFnInfo? name then
        if args.size > proj.numParams then
          let ctor ← getConstInfoCtor proj.ctorName
          let field := Expr.proj ctor.induct proj.i args[proj.numParams]!
          return ← expose (mkAppN field (args.extract (proj.numParams + 1) args.size))
      match ← getConstInfo name with
      | .recInfo info =>
        unless args.size > info.getMajorIdx do return e
        if name == ``Eq.rec then
          if let some reduced ← reduceRecMatcher? e then return ← expose reduced
          throwError "C++ codegen: proof transport did not reduce"
        let major ← expose args[info.getMajorIdx]!
        unless major.isLit || (← isConstructorApp major) do
          throwError "C++ codegen: recursion requires a statically known constructor: {name}, {major}"
        let specialized := mkAppN (mkConst name levels) (args.set! info.getMajorIdx major)
        if let some reduced ← reduceRecMatcher? specialized then return ← expose reduced
        throwError "C++ codegen: could not specialize recursor {name}"
      | .defnInfo info =>
        unless info.safety == .safe do
          throwError "C++ codegen: unsafe/partial definition {name} is not supported"
        let body := info.value.instantiateLevelParams info.levelParams levels
        return ← expose (mkAppN body args)
      | _ => return e
    | .proj .. | .letE .. =>
      let fn ← expose e.getAppFn
      let next := (mkAppN fn e.getAppArgs).headBeta
      if next == e then return e else expose next
    | _ => return e

/-- Exact hexadecimal literals preserve the bits selected by Lean's literal
conversion, including subnormals, without a second decimal conversion. -/
def hexLiteral (bits : Nat) (fractionBits exponentBits bias : Nat) (suffix : String) : MetaM String := do
  if bits == 0 then return "0.0" ++ suffix
  if bits == bias * 2 ^ fractionBits then return "1.0" ++ suffix
  let exponent := (bits >>> fractionBits) % (2 ^ exponentBits)
  let fraction := bits % (2 ^ fractionBits)
  if exponent == 2 ^ exponentBits - 1 then
    throwError "C++ codegen: non-finite literals are not supported"
  let mantissa := if exponent == 0 then fraction else 2 ^ fractionBits + fraction
  let power : Int := (if exponent == 0 then 1 else Int.ofNat exponent) - bias - fractionBits
  let sign := if bits >>> (fractionBits + exponentBits) == 0 then "" else "-"
  return s!"{sign}0x{String.ofList (Nat.toDigits 16 mantissa)}p{power}{suffix}"

def natLiteral : Expr → Option Nat
  | .lit (.natVal n) => some n
  | _ => none

def floatLiteral (name : Name) (args : Array Expr) : GenM Value := do
  let arg ← expose args[0]!
  let some mantissa := natLiteral arg
    | throwError "C++ codegen: only literal numeric conversions are supported: {arg}"
  let scientific := #[``Float.ofScientific, ``Float32.ofScientific, ``OfScientific.ofScientific].contains name
  let mut negativeExponent := false
  let mut exponent := 0
  if scientific then
    match ← expose args[1]! with
    | .const ``Bool.true _ => negativeExponent := true
    | .const ``Bool.false _ => pure ()
    | _ => throwError "C++ codegen: expected a literal exponent sign"
    let some n := natLiteral (← expose args[2]!)
      | throwError "C++ codegen: expected a literal exponent"
    exponent := n
  if name == ``OfNat.ofNat || name == ``OfScientific.ofScientific then
    let single := if scientific then Float32.ofScientific mantissa negativeExponent exponent
      else Float32.ofNat mantissa
    let double := if scientific then Float.ofScientific mantissa negativeExponent exponent
      else Float.ofNat mantissa
    let singleText ← hexLiteral single.toBits.toNat 23 8 127 "f"
    let doubleText ← hexLiteral double.toBits.toNat 52 11 1023 ""
    return .scalar "Vector"
      s!"Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? {singleText} : {doubleText}))"
  else if name == ``Float.ofNat || name == ``Float.ofScientific then
    let value := if scientific then Float.ofScientific mantissa negativeExponent exponent
      else Float.ofNat mantissa
    return .scalar "double" (← hexLiteral value.toBits.toNat 52 11 1023 "")
  else
    let value := if scientific then Float32.ofScientific mantissa negativeExponent exponent
      else Float32.ofNat mantissa
    return .scalar "float" (← hexLiteral value.toBits.toNat 23 8 127 "f")

def withLocal {α : Type} (name : Name) (type : Expr) (k : Expr → GenM α) : GenM α :=
  fun state => withLocalDeclD name type fun value => k value state

def selectField (value : Value) (index : Nat) : GenM Value := do
  let .record fields := value | throwError "C++ codegen: field access on a scalar"
  let some field := fields[index]? | throwError "C++ codegen: invalid field index {index}"
  return field

partial def joinValues (yes no : Value) : GenM (Value × Array String × Array String) := do
  match yes, no with
  | .scalar ty a, .scalar ty' b =>
    unless ty == ty' do throwError "C++ codegen: branch type mismatch"
    let name ← fresh
    emit s!"{ty} {name};"
    return (.scalar ty name, #[s!"{name} = {a};"], #[s!"{name} = {b};"])
  | .record a, .record b =>
    unless a.size == b.size do throwError "C++ codegen: branch record mismatch"
    let mut fields := #[]
    let mut yesLines := #[]
    let mut noLines := #[]
    for i in [:a.size] do
      let (field, ys, ns) ← joinValues a[i]! b[i]!
      fields := fields.push field
      yesLines := yesLines ++ ys
      noLines := noLines ++ ns
    return (.record fields, yesLines, noLines)
  | .vector a, .vector b =>
    let (result, ys, ns) ← joinValues (.record a) (.record b)
    let .record fields := result | throwError "C++ codegen: invalid vector join"
    return (.vector fields, ys, ns)
  | _, _ => throwError "C++ codegen: branch shape mismatch"

def mathIncludes (used : Bool) : Array String :=
  if used then #["#include \"Math.hpp\"", "#include \"MultiChannelNumber.hpp\"", ""] else #[]

/-- Scalar exports use the one-lane overload too, preserving target precision
and the same lookup/interpolation path as the multichannel export. -/
def emitTranscendental (operation type value : String) : GenM Value := do
  modify fun s => { s with usesMath := true }
  if operation == "ExpNegTwoPi" then
    let literal := if type == "Vector" then
        "Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xC90FDBp-21f : 0x1921FB54442D18p-50))"
      else if type == "float" then "0xC90FDBp-21f" else "0x1921FB54442D18p-50"
    let (_, argument) ← scalar (← temporary type s!"(-({literal} * {value}))")
    return ← temporary type (if type == "Vector" then s!"{argument}.Exp()" else s!"MultiChannelNumber<{type}, 1>::Splat({argument}).Exp()[0]")
  return ← temporary type (if type == "Vector" then s!"Math4096::{operation}({value})"
    else s!"Math4096::{operation}(MultiChannelNumber<{type}, 1>::Splat({value}))[0]")

partial def evaluate (expression : Expr) (locals : Locals) : GenM Value := do
  let e ← expose expression
  if let .proj name index (.fvar instanceId) := e.getAppFn then
    if (← get).arithmetic.contains instanceId then
      let args := e.getAppArgs
      if name == ``OfNat then
        let type ← inferType (.fvar instanceId)
        return ← floatLiteral ``OfNat.ofNat #[type.getAppArgs[1]!]
      if name == ``OfScientific then
        return ← floatLiteral ``OfScientific.ofScientific args
      let values ← args.mapM fun arg => do
        let (type, value) ← scalar (← evaluate arg locals)
        unless type == "Vector" do throwError "C++ codegen: generic arithmetic expects Vector operands"
        return value
      if name == ``DSP.Trig && values.size == 1 && index < 3 then
        return ← emitTranscendental (#["Sin2pi", "Cos2pi", "TanPi"][index]!) "Vector" values[0]!
      if name == ``DSP.ExpDecay && values.size == 1 && index == 0 then
        return ← emitTranscendental "ExpNegTwoPi" "Vector" values[0]!
      if name == ``DSP.IfLess && values.size == 4 then
        return ← temporary "Vector"
          s!"Vector::IfLess({values[0]!}, {values[1]!}, {values[2]!}, {values[3]!})"
      if name == ``DSP.FMA && values.size == 3 then
        return ← temporary "Vector"
          s!"Vector::FusedMultiplyAdd({values[0]!}, {values[1]!}, {values[2]!})"
      if name == ``Neg && values.size == 1 then
        return ← temporary "Vector" s!"(-{values[0]!})"
      if let some (_, op) := #[(``Add, "+"), (``Sub, "-"), (``Mul, "*"), (``Div, "/")].find?
          (fun item => item.1 == name) then
        unless values.size == 2 do throwError "C++ codegen: invalid generic arithmetic application"
        return ← temporary "Vector" s!"({values[0]!} {op} {values[1]!})"
      throwError "C++ codegen: unsupported generic arithmetic {e}"
  match e with
  | .fvar id =>
    let some (_, value) := locals.find? (fun item => item.1 == id)
      | throwError "C++ codegen: unbound variable {e}"
    return value
  | .lam _ domain _ _ =>
    unless (← whnf domain).isAppOfArity ``Fin 1 do
      throwError "C++ codegen: unsupported runtime function {e}"
    let n ← finSize domain
    let mut values := #[]
    for i in [:n] do
      let index ← mkAppOptM ``OfNat.ofNat #[some domain, some (mkNatLit i), none]
      values := values.push (← evaluate (mkApp e index) locals)
    return .vector values
  | .letE name type value body _ =>
    if (← whnf type).isForall then
      return ← evaluate (body.instantiate1 value) locals
    let value ← evaluate value locals
    withLocal name type fun bound => evaluate (body.instantiate1 bound) ((bound.fvarId!, value) :: locals)
  | .proj _ index value => selectField (← evaluate value locals) index
  | _ =>
    if !e.getAppFn.isConst && e.isApp then
      let mut value ← evaluate e.getAppFn locals
      for arg in e.getAppArgs do
        let .vector elements := value | throwError "C++ codegen: expected a fixed vector"
        let index ← expose (← mkAppM ``Fin.val #[arg])
        let .lit (.natVal i) := index | throwError "C++ codegen: vector index must be statically known: {repr index}"
        let some element := elements[i]? | throwError "C++ codegen: vector index out of bounds"
        value := element
      return value
    let .const name _ := e.getAppFn
      | throwError "C++ codegen: unsupported expression {e}"
    let args := e.getAppArgs
    if name == ``ite && args.size == 5 then
      let decision ← expose args[2]!
      let .const decisionName _ := decision.getAppFn
        | throwError "C++ codegen: unsupported conditional decision {decision}"
      let decisionArgs := decision.getAppArgs
      unless decisionArgs.size == 2 do
        throwError "C++ codegen: unsupported conditional decision {decision}"
      let conditionExpr ← if decisionName == ``Bool.decEq then
          pure (mkApp3 (mkConst ``Eq [1]) (mkConst ``Bool) decisionArgs[0]! decisionArgs[1]!)
        else if let some (_, predicate) := #[(``Float.decLt, ``Float.lt),
            (``Float.decLe, ``Float.le), (``Float32.decLt, ``Float32.lt),
            (``Float32.decLe, ``Float32.le)].find? (fun entry => entry.1 == decisionName) then
          pure (mkAppN (mkConst predicate) decisionArgs)
        else throwError "C++ codegen: unsupported conditional decision {decision}"
      let (_, condition) ← scalar (← evaluate conditionExpr locals)
      let before := (← get).lines
      modify fun s => { s with lines := #[] }
      let yes ← evaluate args[3]! locals
      let yesLines := (← get).lines
      modify fun s => { s with lines := #[] }
      let no ← evaluate args[4]! locals
      let noLines := (← get).lines
      modify fun s => { s with lines := before }
      let (result, yesAssign, noAssign) ← joinValues yes no
      emit s!"if ({condition})"
      emit "{"
      for line in yesLines ++ yesAssign do emit ("    " ++ line)
      emit "}"
      emit "else"
      emit "{"
      for line in noLines ++ noAssign do emit ("    " ++ line)
      emit "}"
      emit ""
      return result
    if name == ``Eq && args.size == 3 && args[0]!.isConstOf ``Bool then
      let (_, a) ← scalar (← evaluate args[1]! locals)
      let (_, b) ← scalar (← evaluate args[2]! locals)
      return ← temporary "bool" s!"({a} == {b})"
    if name == ``Bool.true then return .scalar "bool" "true"
    if name == ``Bool.false then return .scalar "bool" "false"
    if #[``Float.ofNat, ``Float32.ofNat, ``Float.ofScientific, ``Float32.ofScientific].contains name then
      return ← floatLiteral name args
    if let some op := (#[(``Float.add, "+"), (``Float.sub, "-"), (``Float.mul, "*"),
        (``Float.div, "/"), (``Float32.add, "+"), (``Float32.sub, "-"),
        (``Float32.mul, "*"), (``Float32.div, "/"), (``Float.lt, "<"), (``Float.le, "<="),
        (``Float32.lt, "<"), (``Float32.le, "<="), (``Float.beq, "=="), (``Float32.beq, "==")]).find?
        (fun entry => entry.1 == name) then
      unless args.size == 2 do throwError "C++ codegen: invalid primitive application {e}"
      let (_, a) ← scalar (← evaluate args[0]! locals)
      let (_, b) ← scalar (← evaluate args[1]! locals)
      let type ← inferType e
      let cppType ← if ← isProp e then pure "bool" else scalarType type (← get).numberType
      return ← temporary cppType s!"({a} {op.2} {b})"
    if let some (_, operation) := #[(``Trig.sinFloat, "Sin2pi"), (``Trig.sinFloat32, "Sin2pi"),
        (``Trig.cosFloat, "Cos2pi"), (``Trig.cosFloat32, "Cos2pi"),
        (``Trig.tanFloat, "TanPi"), (``Trig.tanFloat32, "TanPi"),
        (``ExpDecay.float, "ExpNegTwoPi"), (``ExpDecay.float32, "ExpNegTwoPi")].find?
          (fun item => item.1 == name) then
      unless args.size == 1 do throwError "C++ codegen: invalid transcendental application"
      let (type, value) ← scalar (← evaluate args[0]! locals)
      return ← emitTranscendental operation type value
    if name == ``FMA.float || name == ``FMA.float32 then
      unless args.size == 3 do throwError "C++ codegen: invalid FMA application {e}"
      let (type, a) ← scalar (← evaluate args[0]! locals)
      let (_, b) ← scalar (← evaluate args[1]! locals)
      let (_, c) ← scalar (← evaluate args[2]! locals)
      return ← temporary type s!"std::fma({a}, {b}, {c})"
    if name == ``Float.neg || name == ``Float32.neg then
      let (type, value) ← scalar (← evaluate args[0]! locals)
      return ← temporary type s!"(-{value})"
    if let .ctorInfo info ← getConstInfo name then
      unless isStructure (← getEnv) info.induct && args.size == info.numParams + info.numFields do
        throwError "C++ codegen: unsupported constructor {name}"
      return .record (← (args.extract info.numParams args.size).mapM (evaluate · locals))
    throwError "C++ codegen: unsupported expression {e}"

partial def cppElementType (shape : Shape) : MetaM String := do
  match shape with
  | .scalar type => return type
  | .vector n element => return s!"std::array<{← cppElementType element}, {n}>"
  | .record _ => throwError "C++ codegen: vectors of records are unsupported"

partial def renderField (name : String) (shape : Shape) (indent : String) : MetaM (Array String) := do
  match shape with
  | .record fields =>
    let mut lines := #[indent ++ "struct", indent ++ "{"]
    for (name, shape) in fields do
      lines := lines ++ (← renderField name shape (indent ++ "    "))
    return lines ++ #[indent ++ "} " ++ name ++ ";", ""]
  | _ => return #[s!"{indent}{← cppElementType shape} {name};"]

def renderRecord (name : String) (shape : Shape) (indent : String := "    ") : MetaM (Array String) := do
  let .record fields := shape | throwError "C++ codegen: expected a named State/Input/Output record"
  let mut lines := #[s!"{indent}struct {name}", indent ++ "{"]
  for (name, shape) in fields do
    lines := lines ++ (← renderField name shape (indent ++ "    "))
  return lines ++ #[indent ++ "};", ""]

/-- A reusable data template, independent of the processor's channel wrapper.
`Number` may be a scalar or MultiChannelNumber; no arithmetic is generated. -/
def generateRecord (record : Name) (cppName : String) : MetaM String := do
  unless validIdentifier cppName && cppName.toList.head!.isUpper && cppName != "Number" do
    throwError "C++ codegen: record name must be an uppercase C++ identifier other than Number"
  let info ← getConstInfo record
  unless info.levelParams.isEmpty && isStructure (← getEnv) record do
    throwError "C++ codegen: record export expects a structure with one number type parameter"
  forallTelescopeReducing info.type fun parameters _ => do
    unless parameters.size == 1 do
      throwError "C++ codegen: record export expects one number type parameter"
    unless (← inferType parameters[0]!) == mkSort (.succ .zero) do
      throwError "C++ codegen: record export expects one number type parameter"
    let fields ← fieldShape (mkAppN (mkConst record) parameters) (some parameters[0]!) "Number"
    let header := #["#pragma once", "", s!"// Generated from {record}. Do not edit.", "//", "",
      "#include <array>", "", "template<typename Number>"]
    return String.intercalate "\n" (header ++ (← renderRecord cppName fields "")).toList

partial def commitValue (shape : Shape) (value : Value) (target : String) : GenM Unit := do
  match shape, value with
  | .scalar type, .scalar type' text =>
    unless type == type' do throwError "C++ codegen: result field type mismatch"
    emit s!"{target} = {text};"
  | .record fields, .record values =>
    unless fields.size == values.size do throwError "C++ codegen: result shape mismatch"
    for i in [:fields.size] do
      commitValue fields[i]!.2 values[i]! s!"{target}.{fields[i]!.1}"
  | .vector n element, .vector values =>
    unless n == values.size do throwError "C++ codegen: result vector size mismatch"
    for i in [:n] do commitValue element values[i]! s!"{target}[{i}]"
  | _, _ => throwError "C++ codegen: result shape mismatch"

/-- Compute all changed leaves before writing any of them. A projected old
field is omitted entirely, so coefficient updates do not store audio history. -/
partial def changedAssignments (shape : Shape) (value : Value) (target : String) : GenM (Array String) := do
  match shape, value with
  | .scalar type, .scalar type' text =>
    unless type == type' do throwError "C++ codegen: updated field type mismatch"
    if text == target then return #[]
    let (_, saved) ← scalar (← temporary type text)
    return #[s!"{target} = {saved};"]
  | .record fields, .record values =>
    unless fields.size == values.size do throwError "C++ codegen: updated record shape mismatch"
    let mut lines := #[]
    for i in [:fields.size] do
      lines := lines ++ (← changedAssignments fields[i]!.2 values[i]! s!"{target}.{fields[i]!.1}")
    return lines
  | .vector n element, .vector values =>
    unless n == values.size do throwError "C++ codegen: updated vector shape mismatch"
    let mut lines := #[]
    for i in [:n] do
      lines := lines ++ (← changedAssignments element values[i]! s!"{target}[{i}]")
    return lines
  | _, _ => throwError "C++ codegen: updated result shape mismatch"

/-- Snapshot every returned leaf before assigning either persistent record. -/
partial def snapshot (value : Value) : GenM Value := do
  match value with
  | .scalar type text => temporary type text
  | .record fields => return .record (← fields.mapM snapshot)
  | .vector fields => return .vector (← fields.mapM snapshot)

def renderHeader (function : Name) (cppName : String) (generic : Bool)
    (includes : Array String := #[]) : Array String :=
  let templateHeader := if generic then
      #["#include \"MultiChannelNumber.hpp\"", "#include <type_traits>", "",
        "template<typename Number, size_t Size>", s!"struct {cppName}", "{",
        "    static_assert(std::is_same_v<Number, float> || std::is_same_v<Number, double>);",
        "    using Vector = MultiChannelNumber<Number, Size>;", ""]
    else #[s!"struct {cppName}", "{"]
  #["#pragma once", "", s!"// Generated from {function}. Do not edit.",
    "// This backend is validated by tests; it is not a verified compiler.", "//",
    "// Compile with strict floating-point arithmetic and implicit contraction disabled.", "//", "",
    "#include <limits>", "#include <array>", "#include <cmath>", ""] ++ includes.filter (fun line => !generic || line != "#include \"MultiChannelNumber.hpp\"") ++ templateHeader ++ #[
    "    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);",
    "    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);", ""]

/-- Export a read-only snapshot and numeric or shared-record input to a numeric result. This uses
the same evaluator as processors, but returns the value without committing state. -/
def renderFunction (function : Name) (method : String) (parameters runtime : Array Expr)
    (resultType : Expr) (numberType : Option Expr) (arithmetic : Array FVarId) (inputName : String)
    (declareRecords : Bool) :
    MetaM (Array String × Array String) := do
  let snapshotFields ← fieldShape (← inferType runtime[0]!) numberType
  let .record _ := snapshotFields
    | throwError "C++ codegen: function export expects a Snapshot record as its first argument"
  let inputType ← whnf (← inferType runtime[1]!)
  let exports := recordExports.getState (← getEnv)
  let inputRecord := inputType.getAppFn.constName?.bind fun name =>
    exports.find? name
  let numericInput := (numberType == some inputType) ||
    (numberType.isNone && (inputType.isConstOf ``Float || inputType.isConstOf ``Float32))
  unless numericInput || inputRecord.isSome do
    throwError "C++ codegen: second argument must be numeric or an exported record"
  let inputNumber ← if numericInput then pure inputType else do
    unless inputType.getAppArgs.size == 1 do throwError "C++ codegen: record input expects one number type"
    pure inputType.getAppArgs[0]!
  unless ← isDefEq inputNumber resultType do
    throwError "C++ codegen: function result must have the input's numeric type"
  unless (numberType == some inputNumber) ||
      (numberType.isNone && (inputNumber.isConstOf ``Float || inputNumber.isConstOf ``Float32)) do
    throwError "C++ codegen: record input must use the exported number type"
  let cppType ← scalarType inputNumber numberType
  let inputFields ← fieldShape inputType numberType
  let (includes, aliases, inputCppType) := match inputRecord with
    | none => (#[], #[], cppType)
    | some record => (#[s!"#include \"{record.header}\"", ""],
        #[s!"    using {inputName} = ::{record.cppName}<{cppType}>;", ""], inputName)
  let locals := [(runtime[0]!.fvarId!, readValue snapshotFields "snapshot"),
    (runtime[1]!.fvarId!, readValue inputFields "input")]
  let (_, emission) ← (do
    let (type, result) ← scalar (← evaluate (mkAppN (mkConst function) parameters) locals)
    unless type == cppType do throwError "C++ codegen: function result type mismatch"
    emit s!"return {result};").run { numberType, arithmetic }
  let mut body := emission.lines
  for name in #["snapshot", "input"] do
    unless body.any (fun line =>
        (line.splitToList fun c => !(c.isAlphanum || c == '_')).contains name) do
      body := #[s!"static_cast<void>({name});"] ++ body
  let record ← renderRecord "Snapshot" snapshotFields
  let declarations := if declareRecords then aliases ++ record else #[]
  return (includes ++ mathIncludes emission.usesMath, declarations ++
    #[s!"    static {cppType} {method}(const Snapshot& snapshot, const {inputCppType}& input)", "    {"] ++
    body.map (fun line => if line.isEmpty then "" else "        " ++ line) ++ #["    }", ""])

/-- Close a record's arithmetic parameter before comparing signatures from
separate telescopes. Static parameters such as a topology remain significant. -/
def recordSignature (type : Expr) (numberType : Option Expr) : MetaM Expr :=
  match numberType with
  | none => pure type
  | some number => mkForallFVars #[number] type

/-- Capture coefficients from the same logical specification used by tuning.
The returned record is independent of the live processor's histories. -/
def renderSnapshot (function : Name) (parameters runtime : Array Expr)
    (resultType : Expr) (numberType : Option Expr) (arithmetic : Array FVarId)
    (expectedSpec : Expr) : MetaM (Array String × Array String) := do
  let inputType ← inferType runtime[0]!
  unless ← isDefEq (← recordSignature inputType numberType) expectedSpec do
    throwError "C++ codegen: snapshot capture uses a different logical specification"
  let inputFields ← fieldShape inputType numberType
  let outputFields ← fieldShape resultType numberType
  let .record _ := outputFields
    | throwError "C++ codegen: snapshot capture must return a record"
  let locals := [(runtime[0]!.fvarId!, readValue inputFields "spec")]
  let (_, emission) ← (do
    let result ← evaluate (mkAppN (mkConst function) parameters) locals
    emit "Snapshot result{};"
    commitValue outputFields result "result"
    emit "return result;").run { numberType, arithmetic }
  let body := if emission.lines.any (fun line => (line.splitOn "spec.").length > 1)
    then emission.lines else #["static_cast<void>(spec);"] ++ emission.lines
  return (mathIncludes emission.usesMath, (← renderRecord "Snapshot" outputFields) ++
    #["    static Snapshot MakeSnapshot(const LogicalSpec& spec)", "    {"] ++
    body.map (fun line => if line.isEmpty then "" else "        " ++ line) ++ #["    }", ""])

def generateParts (function : Name) (cppName : String) (generic : Bool)
    (method : Option String) (inputName : String) (declareRecords : Bool := true)
    (tuningState : Option Expr := none) (captureSpec : Option Expr := none)
    (captureSnapshot : Option Expr := none)
    (tuningMethod : String := "ComputeCoefficients") : MetaM (Array String × Array String) := do
  unless validIdentifier cppName && cppName.toList.head!.isUpper do
    throwError "C++ codegen: processor name must be a simple uppercase C++ identifier"
  if #["State", "Input", "Output"].contains cppName ||
      (generic && #["Number", "Size", "Vector"].contains cppName) then
    throwError "C++ codegen: processor name conflicts with a nested record"
  if (recordExports.getState (← getEnv)).toList.any (fun entry => entry.2.cppName == cppName) then
    throwError "C++ codegen: processor name conflicts with an exported record"
  if let some method := method then
    unless validIdentifier method && method.toList.head!.isUpper do
      throwError "C++ codegen: method name must be a simple uppercase C++ identifier"
    if cppName == "Snapshot" || method == cppName ||
        #["Snapshot", "Input", "Frequency", "Number", "Size", "Vector"].contains method then
      throwError "C++ codegen: function name conflicts with an enclosing or nested name"
  let info ← getConstInfo function
  unless info.levelParams.isEmpty do
    throwError "C++ codegen: specialize the processor's number type before exporting"
  forallTelescopeReducing info.type fun parameters resultType => do
    let mut numberType := none
    let mut arithmetic := #[]
    let mut runtime := parameters
    let runtimeCount := if captureSpec.isSome then 1 else 2
    if generic then
      unless parameters.size ≥ runtimeCount + 1 do
        throwError "C++ codegen: generic export expects one arithmetic type parameter"
      unless (← inferType parameters[0]!) == mkSort (.succ .zero) do
        throwError "C++ codegen: generic export expects one arithmetic type parameter"
      let number := parameters[0]!
      numberType := some number
      for instanceParam in parameters.extract 1 (parameters.size - runtimeCount) do
        let type ← whnf (← inferType instanceParam)
        let name := type.getAppFn.constName?
        let args := type.getAppArgs
        unless args.size ≥ 1 && args[0]! == number &&
            #[``Add, ``Sub, ``Mul, ``Div, ``Neg, ``OfNat, ``OfScientific, ``DSP.IfLess, ``DSP.FMA, ``DSP.Trig, ``DSP.ExpDecay].any
              (fun supported => name == some supported) do
          throwError "C++ codegen: unsupported generic parameter {type}"
        if name == some ``OfNat then
          unless args.size == 2 && (natLiteral (← expose args[1]!)).isSome do
            throwError "C++ codegen: generic OfNat requires a literal numeral"
        arithmetic := arithmetic.push instanceParam.fvarId!
      runtime := parameters.extract (parameters.size - runtimeCount) parameters.size
    unless runtime.size == runtimeCount do
      throwError "C++ codegen: expected exactly {if runtimeCount == 2 then "two" else "one"} runtime arguments"
    if let some expected := captureSpec then
      return ← renderSnapshot function parameters runtime resultType numberType arithmetic expected
    if let some method := method then
      return ← renderFunction function method parameters runtime resultType numberType arithmetic inputName declareRecords
    if tuningState.isSome then runtime := #[runtime[1]!, runtime[0]!]
    let inputVariable := if tuningState.isSome then "spec" else "input"
    let stateType ← inferType runtime[0]!
    if let some expected := tuningState then
      unless ← isDefEq (← recordSignature stateType numberType) expected do
        throwError "C++ codegen: coefficient updater uses a different state record or different numeric type"
    let inputType ← inferType runtime[1]!
    if tuningState.isSome && !declareRecords then
      unless captureSnapshot.isSome &&
          (← isDefEq (← recordSignature inputType numberType) captureSnapshot.get!) do
        throwError "C++ codegen: prepared updater input must be the response Snapshot with matching numeric type and static parameters"
    let resultType ← whnf resultType
    let stateOnly ← isDefEq stateType resultType
    if tuningState.isSome && !stateOnly && captureSnapshot.isNone then
      throwError "C++ codegen: coefficient updater must return its State"
    unless stateOnly || resultType.isAppOfArity ``Prod 2 do
      throwError "C++ codegen: returned state type differs from input state"
    let resultArgs := resultType.getAppArgs
    unless stateOnly do
      unless ← isDefEq stateType resultArgs[0]! do
        throwError "C++ codegen: returned state type differs from input state"
    if let some expected := captureSnapshot then
      unless resultType.isAppOfArity ``Prod 2 do
        throwError "C++ codegen: prepared coefficients must return State and the response Snapshot"
      unless ← isDefEq (← recordSignature resultArgs[1]! numberType) expected do
        throwError "C++ codegen: prepared coefficients must return State and the response Snapshot with matching numeric type and static parameters"
    let stateFields ← fieldShape stateType numberType
    let inputFields ← fieldShape inputType numberType
    let outputFields ← if stateOnly then pure none else some <$> fieldShape resultArgs[1]! numberType
    let locals := [(runtime[0]!.fvarId!, readValue stateFields "m_state"),
      (runtime[1]!.fvarId!, readValue inputFields inputVariable)]
    let (_, emission) ← (do
      let result ← evaluate (mkAppN (mkConst function) parameters) locals
      match outputFields with
      | none =>
        if tuningState.isSome then
          let assignments ← changedAssignments stateFields result "m_state"
          for line in assignments do emit line
        else
          let next ← snapshot result
          commitValue stateFields next "m_state"
      | some outputFields =>
        if captureSnapshot.isSome then
          let next ← selectField result 0
          let output ← snapshot (← selectField result 1)
          let assignments ← changedAssignments stateFields next "m_state"
          for line in assignments do emit line
          commitValue outputFields output "m_responseSnapshot"
        else
          let next ← snapshot (← selectField result 0)
          let output ← snapshot (← selectField result 1)
          commitValue stateFields next "m_state"
          commitValue outputFields output "m_output").run { numberType, arithmetic }
    let body := if emission.lines.any (fun line => (line.splitOn (inputVariable ++ ".")).length > 1)
      then emission.lines else #[s!"static_cast<void>({inputVariable});"] ++ emission.lines
    if tuningState.isSome then
      let cache := if captureSnapshot.isSome && declareRecords then
        #["    Snapshot m_responseSnapshot{};", "",
          "    Snapshot MakeSnapshot() const", "    {", "        return m_responseSnapshot;", "    }", ""]
        else #[]
      let specRecord ← if declareRecords then renderRecord inputName inputFields else pure #[]
      let lines := cache ++ specRecord ++
        #[s!"    void {tuningMethod}(const {inputName}& spec)", "    {"] ++
        body.map (fun line => if line.isEmpty then "" else "        " ++ line) ++ #["    }", ""]
      return (mathIncludes emission.usesMath, lines)
    let outputLines ← match outputFields with
      | none => pure #["    State m_state;", ""]
      | some outputFields => do
        let record ← renderRecord "Output" outputFields
        pure (record ++ #["    State m_state;", "    Output m_output;", ""])
    let lines := (← renderRecord "State" stateFields) ++ (← renderRecord "Input" inputFields) ++
      outputLines ++ #["    void Process(Input& input)", "    {"] ++
      body.map (fun line => if line.isEmpty then "" else "        " ++ line) ++
      #["    }", ""]
    return (mathIncludes emission.usesMath, lines)

def generateWithTarget (function : Name) (cppName : String) (generic : Bool)
    (method : Option String) : MetaM String := do
  let (includes, body) ← generateParts function cppName generic method "Input"
  return String.intercalate "\n" (renderHeader function cppName generic includes ++ body ++ #["};", ""]).toList

/-- Assemble processing and response definitions in one enclosing filter.
They share the same scalar/vector target; each retains its own Lean source. -/
private partial def firstScalarField : Shape → String
  | .record fields => fields[0]!.1 ++ (match fields[0]!.2 with
      | .scalar _ => ""
      | rest => "." ++ firstScalarField rest)
  | _ => ""

def generateFilterResponses (step : Name) (responses : Array (Name × String))
    (cppName : String) (generic : Bool) (tuning : Option Name := none)
    (capture : Bool := false) (preparedTuning : Option Name := none) : MetaM String := do
  if preparedTuning.isSome && !(capture && tuning.isSome) then
    throwError "C++ codegen: prepared updater requires a sampled filter"
  if #["Snapshot", "Frequency", "LogicalSpec"].contains cppName then
    throwError "C++ codegen: filter name conflicts with a response member"
  if responses.isEmpty then throwError "C++ codegen: filter needs at least one response"
  let (stepIncludes, stepBody) ← generateParts step cppName generic none "Input"
  let mut includes := stepIncludes
  let mut body := stepBody
  let mut methods := #["Process", "State", "Output", "LogicalSpec", "ComputeCoefficients", "ComputeCoefficientsFromSnapshot", "MakeSnapshot", "UIState"]
  let mut tuningMembers := #[]
  if let some tuning := tuning then
    let info ← getConstInfo step
    let expected ← forallTelescopeReducing info.type fun parameters _ => do
      let type ← inferType parameters[parameters.size - 2]!
      recordSignature type (if generic then some parameters[0]! else none)
    let snapshotSignature ← if capture then do
      let responseType := (← getConstInfo responses[0]!.1).type
      forallTelescopeReducing responseType fun parameters _ => do
        let type ← inferType parameters[parameters.size - 2]!
        return some (← recordSignature type (if generic then some parameters[0]! else none))
      else pure none
    let (extraIncludes, tuningBody) ← generateParts tuning cppName generic none "LogicalSpec" true
      (some expected) none snapshotSignature
    includes := includes ++ extraIncludes
    tuningMembers := #[s!"    // Coefficients generated from {tuning}.", "    //", ""] ++ tuningBody
    if let some prepared := preparedTuning then
      let (extraIncludes, preparedBody) ← generateParts prepared cppName generic none "Snapshot" false
        (some expected) none snapshotSignature "ComputeCoefficientsFromSnapshot"
      includes := includes ++ extraIncludes
      tuningMembers := tuningMembers ++ #[s!"    // Prepared coefficients generated from {prepared}.", "    //", ""] ++ preparedBody
  let mut signature : Option Expr := none
  for (response, method) in responses do
    if methods.contains method then throwError "C++ codegen: duplicate or reserved response method {method}"
    methods := methods.push method
    -- Compare the full Lean function types, including arithmetic parameters
    -- and snapshot identity. Equal layouts alone are insufficient.
    let type := (← getConstInfo response).type
    let declareRecords := signature.isNone
    match signature with
    | none => signature := some type
    | some expected =>
      unless ← isDefEq type expected do
        throwError "C++ codegen: response methods must have the same snapshot and frequency types"
    let (extraIncludes, responseBody) ← generateParts response cppName generic (some method) "Frequency" declareRecords
    for line in extraIncludes do
      unless includes.contains line do includes := includes.push line
    body := body ++ #[s!"    // Response generated from {response}.", "    //", ""] ++ responseBody
  body := body ++ tuningMembers
  let mut views := #[]
  if capture then
    unless tuning.isSome do throwError "C++ codegen: sampled filters require coefficient preparation"
    includes := includes.push "#include \"GeneratedFilterUI.hpp\""
    let self := if generic then s!"{cppName}<Number, Size>" else cppName
    let number ← if generic then pure "Number" else do
      let info ← getConstInfo step
      let field ← forallTelescopeReducing info.type fun parameters _ => do
        let shape ← fieldShape (← inferType parameters[parameters.size - 2]!) none
        pure (firstScalarField shape)
      pure ("decltype(State{}." ++ field ++ ")")
    body := body ++ #[s!"    using NumberType = {number};",
      s!"    static constexpr size_t x_numChannels = {if generic then "Size" else "1"};", ""]
    for (_, method) in responses do
      let label := if method == "MagnitudeSquared" then "" else (method.dropEnd "MagnitudeSquared".length).toString
      let adapter := s!"GeneratedResponseSnapshot<{self}, &{self}::{method}>"
      let aliasName := label ++ "UIState"
      body := body ++ #[s!"    using {aliasName} = TransferFunctionGeneric<{adapter}>;", ""]
      if label.isEmpty then
        body := body ++ #["    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const",
          "    {", "        writer.PopulateUIStateSample([&]", "        {",
          "            return MakeSnapshot();", "        });", "    }", ""]
      else
        let stem := if generic && cppName.endsWith "Generic" then
          (cppName.dropEnd "Generic".length).toString else cppName
        let viewName := stem ++ label ++ (if generic then "Generic" else "")
        if generic then views := views.push "template<typename Number, size_t Size>"
        views := views ++ #[s!"struct {viewName} : GeneratedFilterView<{self}, &{self}::{method}>", "{", "};", ""]
  return String.intercalate "\n" (renderHeader step cppName generic includes ++ body ++ #["};", ""] ++ views).toList

/-- A processor with coefficient preparation and a snapshot, without imposing
any scalar response method on a multi-output processor. -/
def generateTunedSnapshot (step tuning capture : Name) (cppName : String) : MetaM String := do
  if #["Snapshot", "LogicalSpec"].contains cppName then
    throwError "C++ codegen: processor name conflicts with a coefficient member"
  let (stepIncludes, stepBody) ← generateParts step cppName true none "Input"
  let expectedState ← forallTelescopeReducing (← getConstInfo step).type fun ps _ => do
    recordSignature (← inferType ps[ps.size-2]!) (some ps[0]!)
  let (tuneIncludes, tuneBody) ← generateParts tuning cppName true none "LogicalSpec" true (some expectedState)
  let expectedSpec ← forallTelescopeReducing (← getConstInfo tuning).type fun ps _ => do
    recordSignature (← inferType ps[ps.size-2]!) (some ps[0]!)
  let (captureIncludes, captureBody) ← generateParts capture cppName true none "LogicalSpec" true none (some expectedSpec)
  let includes := (stepIncludes ++ tuneIncludes ++ captureIncludes).toList.eraseDups.toArray
  let body := stepBody ++ #[s!"    // Coefficients generated from {tuning}.", "    //", ""] ++ tuneBody ++
    #[s!"    // Snapshot generated from {capture}.", "    //", ""] ++ captureBody
  return String.intercalate "\n" (renderHeader step cppName true includes ++ body ++ #["};", ""]).toList

def generateFilter (step response : Name) (cppName : String) (generic : Bool) : MetaM String :=
  generateFilterResponses step #[(response, "MagnitudeSquared")] cppName generic

def generate (function : Name) (cppName : String) : MetaM String :=
  generateWithTarget function cppName false none

def generateGeneric (function : Name) (cppName : String) : MetaM String :=
  generateWithTarget function cppName true none

def generateFunction (function : Name) (cppName method : String) (generic : Bool) : MetaM String :=
  generateWithTarget function cppName generic (some method)

/-- Bind a Lean data record to an existing C++ template with matching fields.
Compilation and differential tests check this external representation boundary. -/
elab "#use_cpp_record " record:ident " as " cppName:ident " from " path:str : command => do
  let recordName := record.getId
  let targetName := cppName.getId.toString
  let header := path.getString
  unless header.endsWith ".hpp" && header.toList.all
      (fun c => c.isAlphanum || c == '_' || c == '.') do
    throwError "C++ codegen: external record header must be a simple .hpp filename"
  let exports := recordExports.getState (← getEnv)
  if exports.contains recordName || exports.toList.any (fun entry => entry.2.cppName == targetName) then
    throwError "C++ codegen: shared record is already exported or its C++ name is in use"
  let _ ← liftTermElabM <| generateRecord recordName targetName
  modifyEnv fun env => recordExports.addEntry env (recordName, ⟨targetName, header⟩)

elab "#generate_cpp_filter " step:ident " response " response:ident " as " cppName:ident
    " to " path:str : command => do
  let code ← liftTermElabM <| generateFilter step.getId response.getId cppName.getId.toString false
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with processing and response members"

elab "#generate_cpp_filter_generic " step:ident " response " responseFunction:ident " as " cppName:ident
    " to " path:str : command => do
  let code ← liftTermElabM <| generateFilter step.getId responseFunction.getId cppName.getId.toString true
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with processing and response members using MultiChannelNumber arithmetic"

declare_syntax_cat cppResponse
syntax ident " as " ident : cppResponse

declare_syntax_cat cppPrepared
syntax "prepared " ident : cppPrepared

elab "#generate_cpp_filter_responses " step:ident
    "[" entries:cppResponse,* "]" " as " cppName:ident " to " path:str : command => do
  let pairs ← entries.getElems.mapM fun entry => do
    match entry with
    | `(cppResponse| $r:ident as $m:ident) => pure (r.getId, m.getId.toString)
    | _ => throwError "C++ codegen: invalid response entry"
  let code ← liftTermElabM <| generateFilterResponses step.getId pairs cppName.getId.toString false
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with named response members"

elab "#generate_cpp_filter_responses_generic " step:ident
    "[" entries:cppResponse,* "]" " as " cppName:ident " to " path:str : command => do
  let pairs ← entries.getElems.mapM fun entry => do
    match entry with
    | `(cppResponse| $r:ident as $m:ident) => pure (r.getId, m.getId.toString)
    | _ => throwError "C++ codegen: invalid response entry"
  let code ← liftTermElabM <| generateFilterResponses step.getId pairs cppName.getId.toString true
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with named response members"

elab "#generate_cpp_tuned_filter " step:ident tuning:ident
    "[" entries:cppResponse,* "]" " as " cppName:ident " to " path:str : command => do
  let pairs ← entries.getElems.mapM fun entry => do
    match entry with
    | `(cppResponse| $r:ident as $m:ident) => pure (r.getId, m.getId.toString)
    | _ => throwError "C++ codegen: invalid response entry"
  let code ← liftTermElabM <| generateFilterResponses step.getId pairs cppName.getId.toString false (some tuning.getId)
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with coefficient update and response members"

elab "#generate_cpp_tuned_filter_generic " step:ident tuning:ident
    "[" entries:cppResponse,* "]" " as " cppName:ident " to " path:str : command => do
  let pairs ← entries.getElems.mapM fun entry => do
    match entry with
    | `(cppResponse| $r:ident as $m:ident) => pure (r.getId, m.getId.toString)
    | _ => throwError "C++ codegen: invalid response entry"
  let code ← liftTermElabM <| generateFilterResponses step.getId pairs cppName.getId.toString true (some tuning.getId)
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with coefficient update and response members"

elab "#generate_cpp_tuned_snapshot_generic " step:ident tuning:ident capture:ident
    " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateTunedSnapshot step.getId tuning.getId capture.getId cppName.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with coefficient update and snapshot capture"

elab "#generate_cpp_record " record:ident " as " cppName:ident " to " path:str : command => do
  let recordName := record.getId
  let targetName := cppName.getId.toString
  let output : System.FilePath := path.getString
  unless output.fileName == some (targetName ++ ".hpp") do
    throwError "C++ codegen: shared record header must be named {targetName}.hpp"
  let exports := recordExports.getState (← getEnv)
  if exports.contains recordName || exports.toList.any (fun entry => entry.2.cppName == targetName) then
    throwError "C++ codegen: shared record is already exported or its C++ name is in use"
  let code ← liftTermElabM <| generateRecord recordName targetName
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  modifyEnv fun env => recordExports.addEntry env (recordName, ⟨targetName, targetName ++ ".hpp"⟩)
  logInfo m!"Generated shared record {output} from {recordName}"

elab "#generate_cpp " function:ident " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generate function.getId cppName.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId}"

elab "#generate_cpp_generic " function:ident " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateGeneric function.getId cppName.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId} with MultiChannelNumber arithmetic"

elab "#generate_cpp_function " function:ident " as " cppName:ident " method " methodName:ident
    " to " path:str : command => do
  let code ← liftTermElabM <| generateFunction function.getId cppName.getId.toString
    methodName.getId.toString false
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId} as a pure function"

elab "#generate_cpp_function_generic " function:ident " as " cppName:ident " method " methodName:ident
    " to " path:str : command => do
  let code ← liftTermElabM <| generateFunction function.getId cppName.getId.toString
    methodName.getId.toString true
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId} as a pure function with MultiChannelNumber arithmetic"

elab "#generate_cpp_sampled_filter " step:ident tuning:ident
    preparedSyntax:(cppPrepared)?
    "[" entries:cppResponse,* "]" " as " cppName:ident " to " path:str : command => do
  let pairs ← entries.getElems.mapM fun entry => do
    match entry with
    | `(cppResponse| $r:ident as $m:ident) => pure (r.getId, m.getId.toString)
    | _ => throwError "C++ codegen: invalid response entry"
  let code ← liftTermElabM <| generateFilterResponses step.getId pairs cppName.getId.toString false (some tuning.getId) true
    (preparedSyntax.map (fun p => p.raw[1].getId))
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with captured response and sampled UI"

elab "#generate_cpp_sampled_filter_generic " step:ident tuning:ident
    preparedSyntax:(cppPrepared)?
    "[" entries:cppResponse,* "]" " as " cppName:ident " to " path:str : command => do
  let pairs ← entries.getElems.mapM fun entry => do
    match entry with
    | `(cppResponse| $r:ident as $m:ident) => pure (r.getId, m.getId.toString)
    | _ => throwError "C++ codegen: invalid response entry"
  let code ← liftTermElabM <| generateFilterResponses step.getId pairs cppName.getId.toString true (some tuning.getId) true
    (preparedSyntax.map (fun p => p.raw[1].getId))
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with captured response and sampled UI"

end DSP.Codegen
