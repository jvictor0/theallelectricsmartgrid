import Lean
import DSP.StateFocus
import DSP.Codegen.Control
import DSP.Codegen.Index
import DSP.Codegen.Scratch
import DSP.Codegen.NativeBindings

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
  | indexed (type : String) (element : Shape)
  | storage (type header : String)
  deriving Inhabited

inductive Value where
  | scalar (type text : String)
  | record (fields : Array Value)
  | vector (elements : Array Value)
  | indexed (type : String) (element : Shape) (text : String)
  | indexedWrite (type : String) (element : Shape) (base index : String) (value : Value)
  | storage (type text : String)
  | scratch (elementType array : String)
  | optional (present : String) (payload : Value)
  | borrowed (text : String) (version : Nat)
  deriving Inhabited

structure StateExport extends RecordExport where
  borrowed : Bool := false
  scalarInputs : Bool := false

structure Emission where
  lines : Array String := #[]
  next : Nat := 0
  numberType : Option Expr := none
  arithmetic : Array FVarId := #[]
  usesMath : Bool := false
  constants : Std.HashMap String (Float32 × Float) := {}
  /-- Only the fixed-capacity storage backend binds bounded indices to size_t. -/
  boundedIndices : Bool := false
  checkedIndices : Bool := false
  usesWideIndices : Bool := false
  stateDialect : Bool := false
  stateVersion : Nat := 0
  recordState : Option Shape := none
  focusedReceiver : Option String := none
  /-- Saved record leaves still denote their original fields until the next state update. -/
  stateSnapshots : Std.HashMap String (Nat × String) := {}
  stateCalls : Array (Name × StateExport) := #[]
  stateHeaders : Array String := #[]
  foldVectorConstants : Bool := false

abbrev GenM := StateT Emission MetaM
abbrev Locals := List (FVarId × Value)

/-- Shared data records are exported before functions that use them. The
association is by Lean type, so kernels reuse one C++ type and header. -/
initialize recordExports : SimplePersistentEnvExtension (Name × RecordExport) (NameMap RecordExport) ←
  registerSimplePersistentEnvExtension {
    addEntryFn := fun state (name, target) => state.insert name target
    addImportedFn := fun imports => imports.foldl (init := ∅) fun state entries =>
      entries.foldl (init := state) fun state (name, target) => state.insert name target }

private def availableRecords (env : Environment) : NameMap RecordExport :=
  nativeRecords.toList.foldl (fun records (name, target) => records.insert name target)
    (recordExports.getState env)

initialize stateExports : SimplePersistentEnvExtension (Name × StateExport) (NameMap StateExport) ←
  registerSimplePersistentEnvExtension {
    addEntryFn := fun state (name, target) => state.insert name target
    addImportedFn := fun imports => imports.foldl (init := ∅) fun state entries =>
      entries.foldl (init := state) fun state (name, target) => state.insert name target }

initialize compiledActions : SimplePersistentEnvExtension Name NameSet ←
  registerSimplePersistentEnvExtension {
    addEntryFn := NameSet.insert
    addImportedFn := fun imports => imports.foldl (init := {}) fun state entries =>
      entries.foldl (init := state) NameSet.insert }

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

def vectorLiteral (single : Float32) (double : Float) : MetaM String := do
  let singleText ← hexLiteral single.toBits.toNat 23 8 127 "f"
  let doubleText ← hexLiteral double.toBits.toNat 52 11 1023 ""
  return s!"Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? {singleText} : {doubleText}))"

/-- Track closed arithmetic separately at each target precision, both for
recognizing exact constant trig inputs and for storage backends that fold
coefficients before rendering the processing schedule. -/
def rememberConstant (value : Value) (single : Float32) (double : Float) : GenM Value := do
  let (type, text) ← scalar value
  if !single.isFinite || !double.isFinite then return value
  let text ← if type == "Vector" && (← get).foldVectorConstants then vectorLiteral single double
    else pure text
  modify fun s => {s with constants := s.constants.insert text (single, double)}
  return .scalar type text

def binaryTemporary (type op a b : String) : GenM Value := do
  let value ← temporary type s!"({a} {op} {b})"
  if let some (a32, a64) := (← get).constants[a]? then
    if let some (b32, b64) := (← get).constants[b]? then
      match op with
      | "+" => return ← rememberConstant value (a32 + b32) (a64 + b64)
      | "-" => return ← rememberConstant value (a32 - b32) (a64 - b64)
      | "*" => return ← rememberConstant value (a32 * b32) (a64 * b64)
      | "/" => return ← rememberConstant value (a32 / b32) (a64 / b64)
      | _ => pure ()
  return value

def negativeTemporary (type argument : String) : GenM Value := do
  let value ← temporary type s!"(-{argument})"
  if let some (single, double) := (← get).constants[argument]? then
    return ← rememberConstant value (-single) (-double)
  return value

/-- Expression temporaries and branch joins already hold a saved value. -/
def savedScalar (type text : String) : GenM Value := do
  if text.startsWith "v" && text.length > 1 && (text.drop 1).toString.toList.all Char.isDigit then
    return .scalar type text
  temporary type text

/-- Remove unused pure expression temporaries after projecting compound results.
Mutable branch joins and all observable assignments remain in place. -/
def pruneTemporaries (lines : Array String) : Array String := Id.run do
  let mut live : Std.HashSet String := {}
  let mut kept := #[]
  for line in lines.reverse do
    let tokens := (line.splitToList fun c => !(c.isAlphanum || c == '_')).filter (· != "")
    let temporary := do
      guard (tokens.head? == some "const")
      let declaration ← (line.splitOn " = ").head?
      let name ← (declaration.splitOn " ").getLast?
      guard (name.startsWith "v" && (name.drop 1).toString.toList.all Char.isDigit)
      pure name
    if temporary.any (fun name => !live.contains name) then continue
    for token in tokens do live := live.insert token
    kept := kept.push line
  return kept.reverse

/-- Commit a returned scalar after its last old-state read and last use of its
saved value. This shortens SIMD live ranges without reordering arithmetic.
Only the backend's final, independent scalar stores move; an entire conditional
is one scheduling boundary, including its else branch. -/
def scheduleStores (lines : Array String) : Array String := Id.run do
  let mut body := #[]
  let mut stores : Array (String × String × String) := #[]
  for line in lines do
    if #["m_state.", "m_output."].any (fun stem => line.startsWith stem) then
      match line.splitOn " = " with
      | [target, source] =>
        stores := stores.push (target, (source.dropEnd 1).toString, line)
      | _ => body := body.push line
    else body := body.push line
  let mut boundaries := #[true]
  let mut depth : Nat := 0
  for i in [:body.size] do
    let line := body[i]!.trimAscii.toString
    if line == "{" then depth := depth + 1
    if line.startsWith "}" then depth := depth - 1
    let next := (body.toList.drop (i+1)).find? (fun s => !s.trimAscii.isEmpty)
    let continues := next.any (fun s => #["else", "{"].contains s.trimAscii.toString)
    boundaries := boundaries.push (depth == 0 && !continues)
  let mut pending : Array (Array String) := Array.replicate (body.size + 1) #[]
  for (target, source, line) in stores do
    let mut position := 0
    for i in [:body.size] do
      let tokens := body[i]!.splitToList fun c => !(c.isAlphanum || c == '_')
      if (body[i]!.splitOn target).length > 1 || tokens.contains source then
        position := i + 1
    while position < body.size && !boundaries[position]! do position := position + 1
    pending := pending.modify position (·.push line)
  let mut result := pending[0]!
  for i in [:body.size] do
    result := result.push body[i]! ++ pending[i+1]!
  return result

def enumConstructors (name : Name) : MetaM (Option (Array Name)) := do
  if name == ``Bool then return none
  let .inductInfo info ← getConstInfo name | return none
  if info.numParams != 0 || info.numIndices != 0 || info.ctors.length < 2 then return none
  unless ← info.ctors.allM (fun ctor => return (← getConstInfoCtor ctor).numFields == 0) do
    return none
  return some info.ctors.toArray

private def quadAgesType := "MultiChannelNumber<std::uint32_t, 4>"

def scalarType (type : Expr) (numberType : Option Expr) : MetaM String := do
  let type ← whnf type
  if numberType == some type then return "Vector"
  if type.isAppOfArity ``Fin 1 then return "size_t"
  if type.isAppOfArity ``DSP.Packed 2 then
    let args := type.getAppArgs
    let number ← match ← whnf args[0]! with
      | .const ``Float _ => pure "double"
      | .const ``Float32 _ => pure "float"
      | _ => throwError "C++ codegen: packed numbers must use Float or Float32"
    let .lit (.natVal count) ← whnf args[1]!
      | throwError "C++ codegen: packed lane count must be static"
    unless count == 3 || (0 < count && count &&& (count - 1) == 0) do
      throwError "C++ codegen: unsupported packed lane count {count}"
    return s!"MultiChannelNumber<{number}, {count}>"
  match type with
  | .const ``Float _ => return "double"
  | .const ``Float32 _ => return "float"
  | .const ``Bool _ => return "bool"
  | .const ``Int _ => return "std::int64_t"
  | .const ``UInt64 _ => return "std::uint64_t"
  | .const ``DelayBuffer.QuadAges _ => return quadAgesType
  | .const name _ =>
    if (← enumConstructors name).isSome then return "unsigned"
    throwError "C++ codegen: unsupported field type {type}; expected Float, Float32 or Bool"
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

partial def cppElementType (shape : Shape) (packed : Bool := false) : MetaM String := do
  match shape with
  | .scalar type | .indexed type _ | .storage type _ => return type
  | .vector n element => return s!"std::array<{← cppElementType element packed}, {n}>"
  | .record fields =>
    unless packed do throwError "C++ codegen: an array element needs a nameable C++ type"
    let types ← fields.toList.mapM fun (_, field) => cppElementType field true
    return s!"std::tuple<{String.intercalate ", " types}>"

private def packedStorageShape (type : Expr) (numberType : Option Expr) : MetaM (Option Shape) := do
  let some binding := storageBindings.find? (type.getAppFn.isConstOf ·.source) | return none
  let args := type.getAppArgs
  unless numberType.isSome && args[binding.sampleArgument]? == numberType do return none
  let parameters ← binding.parameters.toList.mapM fun param =>
    match param with
    | .number => pure "Number"
    | .channels => pure "Size"
    | .boolean value => pure (toString value)
    | .natural i => do
      let some arg := args[i]? | throwError "C++ codegen: missing native storage parameter"
      let .lit (.natVal n) ← whnf arg
        | throwError "C++ codegen: native storage capacity must be static: {arg}"
      pure (toString n)
  return some (.storage (binding.cppName ++ "<" ++ String.intercalate ", " parameters ++ ">") binding.header)

partial def fieldShape (type : Expr) (numberType : Option Expr)
    (numberName : String := "Vector") (nativeStorage : Bool := false) : MetaM Shape := do
  let type ← whnf type
  if nativeStorage then
    if let some shape ← packedStorageShape type numberType then return shape
  if numberType == some type then return .scalar numberName
  if type.isConstOf ``PUnit then return .record #[]
  if type.isAppOfArity ``Subtype 2 then return ← fieldShape type.getAppArgs[0]! numberType numberName
  if type.isAppOfArity ``Scratch 3 then
    throwError "C++ codegen: scratch storage must stay local"
  if type.isAppOfArity ``Vector 2 then
    let args := type.getAppArgs
    let .lit (.natVal count) ← whnf args[1]!
      | throwError "C++ codegen: indexed array capacity must be a literal: {args[1]!}"
    let element ← fieldShape args[0]! numberType numberName nativeStorage
    return .indexed s!"std::array<{← cppElementType element true}, {count}>" element
  if let .forallE _ domain body _ := type then
    unless !body.hasLooseBVars do throwError "C++ codegen: dependent vector elements are unsupported"
    return .vector (← finSize domain) (← fieldShape body numberType numberName nativeStorage)
  if #[``Float, ``Float32, ``Bool, ``Int, ``UInt64, ``DelayBuffer.QuadAges].any type.isConstOf ||
      type.isAppOfArity ``Fin 1 || type.isAppOfArity ``DSP.Packed 2 then
    return .scalar (← scalarType type numberType)
  let .const name _ := type.getAppFn
    | throwError "C++ codegen: unsupported field type {type}"
  if (← enumConstructors name).isSome then return .scalar "unsigned"
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
      fields := fields.push (member, ← fieldShape fieldType numberType numberName nativeStorage)
    return .record fields

def memberPlace (base field : String) (index : Nat) (packed : Bool) : String :=
  if packed then s!"std::get<{index}>({base})" else s!"{base}.{field}"

partial def readValue (shape : Shape) (base : String) (packed : Bool := false) : Value :=
  match shape with
  | .scalar type => .scalar type base
  | .record fields => .record (fields.mapIdx fun i (name, shape) =>
      readValue shape (memberPlace base name i packed) packed)
  | .vector n shape => .vector ((List.range n).toArray.map fun i => readValue shape s!"{base}[{i}]" packed)
  | .indexed type element => .indexed type element base
  | .storage type _ => .storage type base

partial def zeroValue : Shape → Value
  | .scalar type => .scalar type (type ++ "{}")
  | .record fields => .record (fields.map fun (_, shape) => zeroValue shape)
  | .vector n element => .vector (Array.replicate n (zeroValue element))
  | .indexed type element => .indexed type element (type ++ "{}")
  | .storage type _ => .storage type (type ++ "{}")

private def compilerPrimitive (name : Name) : Bool :=
  #[``Subtype.mk, ``Index.add, ``Index.sub, ``Index.ofNat, ``Index.toNat, ``Index.addNat, ``Index.fromFloat,
    ``Scratch.ofFn, ``Scratch.get,
    ``Vector.ofFn, ``Vector.get, ``Vector.set, ``Fin.foldl, ``Fin.foldlM, ``DSP.foldUntil, ``DSP.foldUntilM,
    ``Float.add, ``Float.sub, ``Float.mul, ``Float.div, ``Float.neg,
    ``Float32.add, ``Float32.sub, ``Float32.mul, ``Float32.div, ``Float32.neg,
    ``Float32.toFloat, ``Float.toFloat32,
    ``UInt64.ofNat, ``UInt64.add, ``UInt64.sub, ``UInt64.mul, ``UInt64.xor, ``UInt64.shiftRight, ``UInt64.toFloat,
    ``UInt64.decEq, ``Int.ofNat, ``Int.toNat,
    ``Float.toInt64, ``Int64.toInt, ``Int64.ofInt, ``Int64.toFloat,
    ``Int.add, ``Int.sub, ``Int.lt, ``Int.le, ``Int.decLt, ``Int.decLe, ``Int.decEq,
    ``Float.ofNat, ``Float32.ofNat, ``Float.ofScientific, ``Float32.ofScientific,
    ``Float.lt, ``Float.le, ``Float32.lt, ``Float32.le,
    ``Bool.not, ``Bool.and, ``Bool.or, ``Option.isSome, ``Option.isNone,
    ``Float.beq, ``Float32.beq, ``ite, ``Bool.true, ``Bool.false, ``Eq,
    ``Bool.decEq, ``Float.decLt, ``Float.decLe, ``Float32.decLt, ``Float32.decLe,
    ``dite, ``instDecidableOr, ``instDecidableAnd,
    ``Fin.mk, ``Fin.val, ``Nat.lt, ``Nat.le, ``Nat.decLt, ``Nat.decLe].contains name

/-- Unfold one pure definition at a time, so unknown external implementations
cannot silently be replaced with their logical bodies. Value bindings stay
shared, so each is emitted once; only function-typed bindings are inlined. -/
partial def expose (expression : Expr) (opaqueCalls : Array Name := #[]) : MetaM Expr := withIncRecDepth do
  let e := expression.consumeMData.headBeta.consumeMData
  if e.isHeadBetaTarget then return ← expose e opaqueCalls
  match e with
  | .letE _ type value body _ =>
    if (← whnf type).isForall then expose (body.instantiate1 value) opaqueCalls else return e
  | .proj name index value =>
    let value ← expose value opaqueCalls
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
      if opaqueCalls.contains name then return e
      if name == ``Fin.val && e.getAppArgs.size == 2 then
        let value ← expose e.getAppArgs[1]!
        if value.getAppFn.isConstOf ``Fin.mk then return ← expose value.getAppArgs[1]! opaqueCalls
      if name == ``Nat.zero then return .lit (.natVal 0)
      if name == ``Nat.succ && e.getAppArgs.size == 1 then
        if let .lit (.natVal n) ← expose e.getAppArgs[0]! then return .lit (.natVal (n + 1))
      if let some (_, op) := (#[(``Nat.add, Nat.add), (``Nat.sub, Nat.sub),
          (``Nat.mul, Nat.mul), (``Nat.mod, Nat.mod), (``Nat.div, Nat.div)]).find?
          (fun entry => entry.1 == name) then
        let args := e.getAppArgs
        if args.size < 2 then return e
        if args.size == 2 then
          let a ← expose args[0]! opaqueCalls
          let b ← expose args[1]! opaqueCalls
          if let (.lit (.natVal a), .lit (.natVal b)) := (a, b) then
            return .lit (.natVal (op a b))
        if name == ``Nat.add || name == ``Nat.sub then return e
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
          let left ← expose operands[2]! opaqueCalls
          let right ← expose operands[3]! opaqueCalls
          if let (.lit (.natVal left), .lit (.natVal right)) := (left, right) then
            let yes := left < right
            return ← expose args[if yes then 3 else 4]! opaqueCalls
      if name == ``Nat.decEq && e.getAppArgs.size == 2 then
        let a ← expose e.getAppArgs[0]!
        let b ← expose e.getAppArgs[1]!
        if a.isLit && b.isLit then return ← whnf (mkApp2 (mkConst ``Nat.decEq) a b)
        if !opaqueCalls.isEmpty then return e
        throwError "C++ codegen: natural equality must be statically known"
      if name == ``Nat.beq && e.getAppArgs.size == 2 then
        let a ← expose e.getAppArgs[0]!
        let b ← expose e.getAppArgs[1]!
        if let (.lit (.natVal a), .lit (.natVal b)) := (a,b) then
          return mkConst (if a == b then ``Bool.true else ``Bool.false)
        if !opaqueCalls.isEmpty then return e
        throwError "C++ codegen: natural equality must be statically known"
      if compilerPrimitive name || valueLowerings.contains name then return e
      let env ← getEnv
      if (getExternAttrData? env name).isSome || (Compiler.getImplementedBy? env name).isSome then
        throwError "C++ codegen: unsupported external implementation {name}"
      let args := e.getAppArgs
      if let some proj := env.getProjectionFnInfo? name then
        if args.size > proj.numParams then
          let ctor ← getConstInfoCtor proj.ctorName
          let field := Expr.proj ctor.induct proj.i args[proj.numParams]!
          return ← expose (mkAppN field (args.extract (proj.numParams + 1) args.size)) opaqueCalls
      match ← getConstInfo name with
      | .recInfo info =>
        unless args.size > info.getMajorIdx do return e
        if name == ``Eq.rec then
          if let some reduced ← reduceRecMatcher? e then return ← expose reduced opaqueCalls
          throwError "C++ codegen: proof transport did not reduce"
        let major ← expose args[info.getMajorIdx]! opaqueCalls
        unless major.isLit || (← isConstructorApp major) do
          if info.getMajorInduct == ``Prod then return e
          if info.getMajorInduct == ``Option then return e
          if (← enumConstructors info.getMajorInduct).isSome then return e
          throwError "C++ codegen: recursion requires a statically known constructor: {name}, {major}"
        let specialized := mkAppN (mkConst name levels) (args.set! info.getMajorIdx major)
        if let some reduced ← reduceRecMatcher? specialized then return ← expose reduced opaqueCalls
        throwError "C++ codegen: could not specialize recursor {name}"
      | .defnInfo info =>
        unless info.safety == .safe do
          throwError "C++ codegen: unsafe/partial definition {name} is not supported"
        let body := info.value.instantiateLevelParams info.levelParams levels
        return ← expose (mkAppN body args) opaqueCalls
      | _ => return e
    | .proj .. | .letE .. =>
      let fn ← expose e.getAppFn opaqueCalls
      if let .letE binder type bound body nondep := fn then
        let args := e.getAppArgs.map (·.liftLooseBVars 0 1)
        return .letE binder type bound (mkAppN body args) nondep
      let next := (mkAppN fn e.getAppArgs).headBeta
      if next == e then return e else expose next opaqueCalls
    | _ => return e

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
    return ← rememberConstant (.scalar "Vector" (← vectorLiteral single double)) single double
  else if name == ``Float.ofNat || name == ``Float.ofScientific then
    let value := if scientific then Float.ofScientific mantissa negativeExponent exponent
      else Float.ofNat mantissa
    return ← rememberConstant (.scalar "double" (← hexLiteral value.toBits.toNat 52 11 1023 "")) value.toFloat32 value
  else
    let value := if scientific then Float32.ofScientific mantissa negativeExponent exponent
      else Float32.ofNat mantissa
    return ← rememberConstant (.scalar "float" (← hexLiteral value.toBits.toNat 23 8 127 "f")) value value.toFloat

def withLocal {α : Type} (name : Name) (type : Expr) (k : Expr → GenM α) : GenM α :=
  fun state => withLocalDeclD name type fun value => k value state

def selectField (value : Value) (index : Nat) : GenM Value := do
  let .record fields := value | throwError "C++ codegen: field access on a scalar"
  let some field := fields[index]? | throwError "C++ codegen: invalid field index {index}"
  return field

partial def commitValue (shape : Shape) (value : Value) (target : String) (packed : Bool := false) : GenM Unit := do
  match shape, value with
  | .scalar type, .scalar type' text =>
    unless type == type' do throwError "C++ codegen: result field type mismatch"
    emit s!"{target} = {text};"
  | .record fields, .record values =>
    unless fields.size == values.size do throwError "C++ codegen: result shape mismatch"
    for i in [:fields.size] do
      commitValue fields[i]!.2 values[i]! (memberPlace target fields[i]!.1 i packed) packed
  | .vector n element, .vector values =>
    unless n == values.size do throwError "C++ codegen: result vector size mismatch"
    for i in [:n] do commitValue element values[i]! s!"{target}[{i}]" packed
  | .indexed type _, .indexed type' _ text =>
    unless type == type' do throwError "C++ codegen: result array type mismatch"
    if target != text then emit s!"{target} = {text};"
  | .indexed type element, .indexedWrite type' _ base index value =>
    unless type == type' do throwError "C++ codegen: result array type mismatch"
    let copy ← fresh
    emit s!"{type} {copy} = {base};"
    commitValue element value s!"{copy}[{index}]" true
    emit s!"{target} = {copy};"
  | _, _ => throwError "C++ codegen: result shape mismatch"

def materializeArray : Value → GenM Value
  | .indexedWrite type element base index value => do
    let copy ← fresh
    emit s!"{type} {copy} = {base};"
    commitValue element value s!"{copy}[{index}]" true
    return .indexed type element copy
  | value => pure value

def collectLines (action : GenM Unit) : GenM (Array String) := do
  let before := (← get).lines
  modify fun s => { s with lines := #[] }
  action
  let lines := (← get).lines
  modify fun s => { s with lines := before }
  return lines

partial def joinValues (yes no : Value) : GenM (Value × Array String × Array String) := do
  match yes, no with
  | .scalar ty a, .scalar ty' b =>
    unless ty == ty' do throwError "C++ codegen: branch type mismatch"
    if (← get).recordState.isSome && a == b then return (yes, #[], #[])
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
  | .indexed type element _, _ | .indexedWrite type element .., _ =>
    let name ← fresh
    emit s!"{type} {name};"
    let shape := Shape.indexed type element
    let ys ← collectLines (commitValue shape yes name)
    let ns ← collectLines (commitValue shape no name)
    return (.indexed type element name, ys, ns)
  | .optional a av, .optional b bv =>
    let (present, ys, ns) ← joinValues (.scalar "bool" a) (.scalar "bool" b)
    let (_, present) ← scalar present
    let (payload, yv, nv) ← joinValues av bv
    return (.optional present payload, ys ++ yv, ns ++ nv)
  | .borrowed a _, .borrowed b _ =>
    unless a == b do throwError "C++ codegen: branches must retain the same borrowed state"
    return (.borrowed a (← get).stateVersion, #[], #[])
  | _, _ => throwError "C++ codegen: branch shape mismatch"

private partial def checkBorrowedVersion (version : Nat) : Value → GenM Unit
  | .borrowed _ actual =>
    unless actual == version do throwError "C++ codegen: branch returned an old storage snapshot"
  | .record fields | .vector fields => fields.forM (checkBorrowedVersion version)
  | .optional _ payload | .indexedWrite _ _ _ _ payload => checkBorrowedVersion version payload
  | _ => pure ()

private def checkFocusedReceiver (receiver : String) : GenM Unit := do
  if let some active := (← get).focusedReceiver then
    unless receiver == active do
      throwError "C++ codegen: a focused action may only use its current child storage"

def emitBranch (condition : String) (yes no : GenM Value) : GenM Value := do
  let checked := fun (branch : GenM Value) => do
    let value ← branch
    checkBorrowedVersion (← get).stateVersion value
    return value
  if condition == "true" then return ← checked yes
  if condition == "false" then return ← checked no
  let before := (← get).lines
  let version := (← get).stateVersion
  modify fun s => { s with lines := #[] }
  let yesValue ← checked yes
  let yesLines := (← get).lines
  let yesVersion := (← get).stateVersion
  modify fun s => { s with lines := #[], stateVersion := version }
  let noValue ← checked no
  let noLines := (← get).lines
  modify fun s => { s with lines := before, stateVersion := max yesVersion s.stateVersion }
  let (result, yesAssign, noAssign) ← joinValues yesValue noValue
  emit s!"if ({condition})"
  emit "{"
  for line in yesLines ++ yesAssign do emit (if line.isEmpty then "" else "    " ++ line)
  emit "}"
  emit "else"
  emit "{"
  for line in noLines ++ noAssign do emit (if line.isEmpty then "" else "    " ++ line)
  emit "}"
  emit ""
  return result

def mathIncludes (used : Bool) : Array String :=
  if used then #["#include \"Math.hpp\"", "#include \"MultiChannelNumber.hpp\"", ""] else #[]

private def emissionIncludes (emission : Emission) (storage : Array String := #[]) : Array String :=
  let headers := (storage ++ emission.stateHeaders).toList.eraseDups.toArray
  mathIncludes emission.usesMath ++ headers.map (fun header => s!"#include \"{header}\"")

/-- Only exact eighth-cycle phases have a constexpr Math4096 table entry.
Other constants and all runtime phases retain the ordinary lookup path. -/
def eighthCycle (x : Float) : Option Nat := do
  if !x.isFinite then none else
    let phase := x - x.floor
    let index := (phase * 8).toUInt64.toNat
    if index < 8 && phase == Float.ofNat index / 8 then some index else none

def constantEighth (type value : String) : GenM (Option Nat) := do
  let some (single, double) := (← get).constants[value]? | return none
  if type == "float" then return eighthCycle single.toFloat
  if type == "double" then return eighthCycle double
  let a := eighthCycle single.toFloat
  let b := eighthCycle double
  return if a == b then a else none

/-- Scalar exports use the one-lane overload too, preserving target precision
and the same lookup/interpolation path as the multichannel export. -/
def emitTranscendental (operation type value : String) : GenM Value := do
  modify fun s => { s with usesMath := true }
  if operation == "Cos2pi" then
    if let some index ← constantEighth type value then
      return ← temporary type (if type == "Vector" then
        s!"Vector::Splat(static_cast<Number>(Math4096::CosEighth({index})))"
        else s!"static_cast<{type}>(Math4096::CosEighth({index}))")
  if operation == "ExpNegTwoPi" then
    let literal := if type == "Vector" then
        "Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? 0xC90FDBp-21f : 0x1921FB54442D18p-50))"
      else if type == "float" then "0xC90FDBp-21f" else "0x1921FB54442D18p-50"
    let (_, argument) ← scalar (← temporary type s!"(-({literal} * {value}))")
    return ← temporary type (if type == "Vector" then s!"{argument}.Exp()" else s!"MultiChannelNumber<{type}, 1>::Splat({argument}).Exp()[0]")
  return ← temporary type (if type == "Vector" then s!"Math4096::{operation}({value})"
    else s!"Math4096::{operation}(MultiChannelNumber<{type}, 1>::Splat({value}))[0]")

private def emitNumeric (operation : NumericLowering) (type : String) (values : Array String) :
    GenM Value := do
  let vector := type == "Vector"
  let literal (single double : String) := if vector then
    s!"Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? {single} : {double}))"
    else if type == "float" then single else double
  match operation, values with
  | .trig method, #[x] => emitTranscendental method type x
  | .expDecay, #[x] => emitTranscendental "ExpNegTwoPi" type x
  | .exp2, #[x] =>
    modify fun s => { s with usesMath := true }
    temporary type (if vector then s!"{x}.Exp2()" else s!"MultiChannelNumber<{type}, 1>::Splat({x}).Exp2()[0]")
  | .floor, #[x] =>
    modify fun s => { s with usesMath := true }
    temporary type (if vector then s!"{x}.Floor()" else s!"std::floor({x})")
  | .phaseTurns, #[y, x] =>
    modify fun s => { s with usesMath := true }
    temporary type s!"(Math::ATan2({y}, {x}) / {literal "0xC90FDBp-21f" "0x1921FB54442D18p-50"})"
  | .decay, #[x] =>
    modify fun s => { s with usesMath := true }
    let ln1000f ← hexLiteral (6.907755278982137052053974364053 : Float32).toBits.toNat 23 8 127 "f"
    let ln1000 ← hexLiteral (6.907755278982137052053974364053 : Float).toBits.toNat 52 11 1023 ""
    let (_, arg) ← scalar (← temporary type s!"(-({literal ln1000f ln1000} * {x}))")
    temporary type (if vector then s!"{arg}.Exp()" else s!"MultiChannelNumber<{type}, 1>::Splat({arg}).Exp()[0]")
  | .sqrt, #[x] => temporary type (if vector then s!"{x}.Sqrt()" else s!"std::sqrt({x})")
  | .channelMean, #[x] => temporary type s!"{x}.MeanBroadcast()"
  | .tanh, #[x] =>
    -- Preserve the multiplication, FMA and ordered clamp of Tanh.rational.
    let (_, x2) ← scalar (← temporary type s!"({x} * {x})")
    let (_, numerator) ← scalar (← temporary type s!"({x} * ({x2} + Vector::Splat(27)))")
    let (_, denominator) ← scalar (← temporary type
      s!"Vector::FusedMultiplyAdd({x} * Vector::Splat(9), {x}, Vector::Splat(27))")
    let (_, y) ← scalar (← temporary type s!"({numerator} / {denominator})")
    temporary type s!"Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), {y}))"
  | .ifLess, #[a, b, yes, no] => temporary type s!"Vector::IfLess({a}, {b}, {yes}, {no})"
  | .fma, #[a, b, c] =>
    let method := if vector then "Vector::FusedMultiplyAdd" else "std::fma"
    temporary type s!"{method}({a}, {b}, {c})"
  | .libm method result, #[x] => temporary result s!"std::{method}({x})"
  | .curvePair header method, #[x] => do
    modify fun s => { s with stateHeaders := s.stateHeaders.push header }
    let pair ← fresh
    emit s!"const auto {pair} = {method}({x});"
    return .record #[.scalar type s!"{pair}.first", .scalar type s!"{pair}.second"]
  | _, _ => throwError "C++ codegen: invalid native numeric application"

private partial def callArgument : Value → GenM String
  | .scalar _ text => pure text
  | .record fields => return "{" ++ String.intercalate ", " (← fields.toList.mapM callArgument) ++ "}"
  | .vector _ => throwError "C++ codegen: a bound call requires packed vectors"
  | .indexed _ _ text => pure text
  | .scratch .. => throwError "C++ codegen: scratch storage cannot be passed to a bound call"
  | value@(.indexedWrite ..) => do callArgument (← materializeArray value)
  | .optional .. => throwError "C++ codegen: a bound call needs an explicit option representation"
  | .borrowed .. => throwError "C++ codegen: a value binding cannot consume borrowed state"
  | .storage .. => throwError "C++ codegen: native storage is passed through a focused state action"

def emitIndexLoop (count : String) (body : String → GenM Unit) : GenM Unit := do
  if count == "0" then return
  let index ← fresh
  let lines ← collectLines (body index)
  emit s!"for (std::size_t {index} = 0; {index} < {count}; ++{index})"
  emit "{"
  for line in lines do emit (if line.isEmpty then "" else "    " ++ line)
  emit "}"
  emit ""

def emitBreak (condition : String) : GenM Unit := do
  if condition == "false" then return
  if condition == "true" then emit "break;"
  else
    emit s!"if ({condition})"
    emit "{"
    emit "    break;"
    emit "}"

partial def renderField (name : String) (shape : Shape) (indent : String) : MetaM (Array String) := do
  match shape with
  | .record fields =>
    let mut lines := #[indent ++ "struct", indent ++ "{"]
    for (name, shape) in fields do
      lines := lines ++ (← renderField name shape (indent ++ "    "))
    return lines ++ #[indent ++ "} " ++ name ++ ";", ""]
  | .vector n (.record fields) =>
    let elementName := name ++ "_Element"
    let mut lines := #[s!"{indent}struct {elementName}", indent ++ "{"]
    for (field, shape) in fields do
      lines := lines ++ (← renderField field shape (indent ++ "    "))
    return lines ++ #[indent ++ "};", "", s!"{indent}std::array<{elementName}, {n}> {name};"]
  | _ => return #[s!"{indent}{← cppElementType shape} {name};"]

def renderRecord (name : String) (shape : Shape) (indent : String := "    ") : MetaM (Array String) := do
  let .record fields := shape | throwError "C++ codegen: expected a named State/Input/Output record"
  let mut lines := #[s!"{indent}struct {name}", indent ++ "{"]
  for (name, shape) in fields do
    lines := lines ++ (← renderField name shape (indent ++ "    "))
  return lines ++ #[indent ++ "};", ""]

private def sameStatePlace (text target : String) : GenM Bool := do
  let emission ← get
  return text == target || emission.stateSnapshots[text]? == some (emission.stateVersion, target)

/-- Collect changed leaves, capturing projected reads before committing them.
An identity projection is omitted, including unchanged process coefficients. -/
partial def changedAssignments (shape : Shape) (value : Value) (target : String)
    (owned : Bool := false) (packed : Bool := false) : GenM (Array String) := do
  match shape, value with
  | .scalar type, .scalar type' text =>
    unless type == type' do throwError "C++ codegen: updated field type mismatch"
    if ← sameStatePlace text target then return #[]
    let (_, saved) ← scalar (← savedScalar type text)
    return #[s!"{target} = {saved};"]
  | .record fields, .record values =>
    unless fields.size == values.size do throwError "C++ codegen: updated record shape mismatch"
    let mut lines := #[]
    for i in [:fields.size] do
      lines := lines ++ (← changedAssignments fields[i]!.2 values[i]!
        (memberPlace target fields[i]!.1 i packed) owned packed)
    return lines
  | .vector n element, .vector values =>
    unless n == values.size do throwError "C++ codegen: updated vector shape mismatch"
    let mut lines := #[]
    for i in [:n] do
      lines := lines ++ (← changedAssignments element values[i]! s!"{target}[{i}]" owned packed)
    return lines
  | .indexed type _, .indexed type' _ text =>
    unless type == type' do throwError "C++ codegen: updated array type mismatch"
    if ← sameStatePlace text target then return #[]
    let (_, saved) ← scalar (← savedScalar type text)
    return #[s!"{target} = {saved};"]
  | .storage type _, .storage type' text =>
    unless type == type' && text == target do
      throwError "C++ codegen: native storage must be updated through a focused action"
    return #[]
  | .indexed type element, .indexedWrite type' _ base index next =>
    unless type == type' do throwError "C++ codegen: updated array type mismatch"
    if owned && (← sameStatePlace base target) then
      let (_, index) ← scalar (← savedScalar "size_t" index)
      return ← changedAssignments element next s!"{target}[{index}]" owned true
    changedAssignments shape (← materializeArray value) target owned packed
  | _, _ => throwError "C++ codegen: updated result shape mismatch"

private def snapshotScalar (type text : String) (stateRead : Bool) : GenM Value := do
  let saved ← savedScalar type text
  if stateRead then
    let (_, name) ← scalar saved
    modify fun s => {s with stateSnapshots := s.stateSnapshots.insert name (s.stateVersion, text)}
  return saved

/-- Capture projected leaves while reusing existing saved expression values. -/
partial def snapshot (value : Value) (stateRead : Bool := false) : GenM Value := do
  match value with
  | .scalar type text => snapshotScalar type text stateRead
  | .record fields => return .record (← fields.mapM (snapshot · stateRead))
  | .vector fields => return .vector (← fields.mapM (snapshot · stateRead))
  | .indexed type element text =>
    let (_, saved) ← scalar (← snapshotScalar type text stateRead)
    return .indexed type element saved
  | value@(.scratch ..) => return value
  | value@(.indexedWrite ..) => snapshot (← materializeArray value)
  | .optional present payload =>
    let (_, present) ← scalar (← savedScalar "bool" present)
    return .optional present (← snapshot payload)
  | .borrowed .. => throwError "C++ codegen: borrowed state cannot be saved by value"
  | .storage .. => throwError "C++ codegen: focus on a field to read state containing native storage"

def initializeLoopValue (shape : Shape) (initial : Value) : GenM String := do
  let name := "loop_" ++ (← fresh)
  let type ← match shape with
    | .record _ =>
      let type := name ++ "_Type"
      for line in ← renderRecord type shape "" do emit line
      pure type
    | _ => cppElementType shape
  emit s!"[[maybe_unused]] {type} {name};"
  commitValue shape initial name
  return name

inductive Destination where
  | value
  | state (shape : Shape) (receiver : String)
  | stateAndResult (shape : Shape) (receiver : String)
  | action (shape : Shape) (receiver : String)

private def writeState (shape : Shape) (receiver : String) (value : Value) : GenM Unit := do
  for assignment in ← changedAssignments shape value receiver true do
    emit assignment

private def finishAction (shape : Shape) (receiver : String) (result : Value) : GenM Value := do
  let value ← selectField result 0
  let value ← match value with
    | .borrowed source version =>
      checkBorrowedVersion (← get).stateVersion (.borrowed source version)
      snapshot (readValue shape source) true
    | _ => pure value
  let next ← selectField result 1
  if let .borrowed source version := next then
    unless source == receiver do throwError "C++ codegen: action changed its storage receiver"
    checkBorrowedVersion (← get).stateVersion next
    return .record #[value, .borrowed receiver version]
  let result ← snapshot value
  writeState shape receiver next
  let version := (← get).stateVersion + 1
  modify fun s => {s with stateVersion := version}
  return .record #[result, .borrowed receiver version]

private def focusedField (parentType childType getter setter : Expr) : MetaM Nat := do
  let parentType ← whnf parentType
  let .const name _ := parentType.getAppFn
    | throwError "C++ codegen: focus expects a structure parent"
  let some info := getStructureInfo? (← getEnv) name
    | throwError "C++ codegen: focus expects a structure parent"
  withLocalDeclD `parent parentType fun parent =>
    withLocalDeclD `child childType fun child => do
      let projection := mkApp getter parent
      let updated := mkApp2 setter parent child
      for index in [:info.fieldNames.size] do
        let field := Expr.proj name index parent
        unless ← isDefEq projection field do continue
        for other in [:info.fieldNames.size] do
          let expected := if other == index then child else .proj name other parent
          unless ← isDefEq (.proj name other updated) expected do
            throwError "C++ codegen: focus setter must replace only the selected field"
        return index
      throwError "C++ codegen: focus getter must select a direct field"

partial def evaluate (expression : Expr) (locals : Locals)
    (destination : Destination := .value) : GenM Value := do
  let effects := if (← get).stateDialect then stateBindings else #[]
  let stateCalls := (← get).stateCalls
  let opaqueCalls := effects.map (·.source) ++ stateCalls.map (·.1) ++
    (if (← get).stateDialect then #[``StateT.get] else #[]) ++
    (if (← get).recordState.isSome then #[``StateT.bind, ``StateT.set, ``StateT.modifyGet, ``DSP.focus] else #[]) ++
    (if (← get).boundedIndices then #[``Nat.decEq, ``Nat.beq] else #[])
  let e ← expose expression opaqueCalls
  -- Tail branches of an update write into the record directly. This keeps
  -- writes that belong to one branch out of the common continuation.
  let inTail := match e with
    | .letE .. => true
    | _ => e.getAppFn.isConstOf ``ite || e.getAppFn.isConstOf ``dite ||
      (e.getAppFn.isConstOf ``StateT.bind && (match destination with | .action .. => true | _ => false))
  unless inTail do
    match destination with
    | .value => pure ()
    | .action shape receiver => return ← finishAction shape receiver (← evaluate e locals)
    | .state shape receiver =>
      writeState shape receiver (← evaluate e locals)
      return .record #[]
    | .stateAndResult shape receiver =>
      if e.isAppOfArity ``Prod.mk 4 then
        let args := e.getAppArgs
        let result ← snapshot (← evaluate args[2]! locals)
        let _ ← evaluate args[3]! locals (.state shape receiver)
        return result
      let pair ← evaluate e locals
      let result ← snapshot (← selectField pair 0)
      writeState shape receiver (← selectField pair 1)
      return result
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
      if let some operation := (arithmeticLowerings.find? name).bind (·[index]?) then
        return ← emitNumeric operation "Vector" values
      if name == ``Neg && values.size == 1 then
        return ← negativeTemporary "Vector" values[0]!
      if let some (_, op) := #[(``Add, "+"), (``Sub, "-"), (``Mul, "*"), (``Div, "/")].find?
          (fun item => item.1 == name) then
        unless values.size == 2 do throwError "C++ codegen: invalid generic arithmetic application"
        return ← binaryTemporary "Vector" op values[0]! values[1]!
      throwError "C++ codegen: unsupported generic arithmetic {e}"
  match e with
  | .lit (.natVal n) =>
    unless (← get).boundedIndices do
      throwError "C++ codegen: runtime natural values require the bounded buffer backend"
    return .scalar "size_t" s!"{n}"
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
    if (← whnf type).isForall || (← isProp type) then
      return ← evaluate (body.instantiate1 value) locals destination
    let value ← evaluate value locals
    withLocal name type fun bound =>
      evaluate (body.instantiate1 bound) ((bound.fvarId!, value) :: locals) destination
  | .proj record index value =>
    let value ← evaluate value locals
    if record == ``Subtype && index == 0 then return value
    if record == ``Fin && index == 0 then
      let (type, _) ← scalar value
      unless type == "size_t" do throwError "C++ codegen: expected a bounded index"
      return value
    let .record _ := value | throwError "C++ codegen: cannot project field {index} of {record} in {e}"
    selectField value index
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
    if ((name == ``Fin.foldl || name == ``DSP.foldUntil) && args.size == 4) ||
        ((name == ``Fin.foldlM || name == ``DSP.foldUntilM) && args.size == 7) then
      let monadic := name == ``Fin.foldlM || name == ``DSP.foldUntilM
      let earlyExit := name == ``DSP.foldUntilM
      let offset := if monadic then 3 else 1
      let countExpr := args[offset]!
      let count ← if let .lit (.natVal count) ← expose countExpr then
        pure (toString count)
      else do
        let (type, count) ← scalar (← evaluate countExpr locals)
        unless type == "size_t" do throwError "C++ codegen: finite fold requires a bounded count"
        let (_, saved) ← scalar (← savedScalar type count)
        pure saved
      let accType := args[if monadic then 1 else 0]!
      let shape ← fieldShape accType (← get).numberType
      let initial ← evaluate args[offset + 2]! locals
      let acc ← initializeLoopValue shape initial
      let accValue := readValue shape acc
      if name == ``DSP.foldUntil then
        emitIndexLoop count fun _ => withLocal `acc accType fun accVar => do
          let .optional done next ← evaluate (mkApp args[2]! accVar) ((accVar.fvarId!, accValue) :: locals)
            | throwError "C++ codegen: bounded traversal expects a ForInStep result"
          for line in ← changedAssignments shape next acc true do emit line
          emitBreak done
        return accValue
      let indexType ← mkAppM ``Fin #[countExpr]
      let state ← if monadic then do
        let stateType ← inferType args[6]!
        let monad ← mkAppM ``StateM #[stateType]
        unless ← isDefEq args[0]! monad do
          throwError "C++ codegen: monadic finite folds require StateM"
        let standard ← synthInstance (← mkAppM ``Monad #[monad])
        unless ← isDefEq args[2]! standard do
          throwError "C++ codegen: finite fold requires the standard StateM instance"
        let initial ← evaluate args[6]! locals
        checkBorrowedVersion (← get).stateVersion initial
        let owned ← match initial with
          | .borrowed .. => pure none
          | _ =>
            let shape ← fieldShape stateType (← get).numberType
            let target ← initializeLoopValue shape initial
            pure (some (shape, target))
        let value ← match initial, owned with
          | .borrowed receiver version, _ => do
            if count == "0" then return initial
            -- Only the loop-carried handle denotes the state of this iteration.
            let version := version + 1
            modify fun s => {s with stateVersion := version}
            pure (.borrowed receiver version)
          | _, some (shape, target) => pure (readValue shape target)
          | _, none => throwError "C++ codegen: missing owned loop storage"
        pure (some (stateType, value, owned))
      else pure none
      emitIndexLoop count fun index =>
        withLocal `acc accType fun accVar =>
          withLocal `index indexType fun indexVar => do
            let locals := (accVar.fvarId!, accValue) :: (indexVar.fvarId!, .scalar "size_t" index) :: locals
            let step := if earlyExit then mkApp args[offset + 1]! accVar
              else mkApp2 args[offset + 1]! accVar indexVar
            if let some (stateType, stateValue, owned) := state then
              withLocal `state stateType fun stateVar => do
                let target := match stateValue, (← get).recordState with
                  | .borrowed receiver _, some stateShape => Destination.action stateShape receiver
                  | _, _ => .value
                let result ← evaluate (mkApp step stateVar)
                  ((stateVar.fvarId!, stateValue) :: locals) target
                let stepValue ← selectField result 0
                let (done, nextAcc) ← if earlyExit then do
                  let .optional done next ← pure stepValue
                    | throwError "C++ codegen: bounded traversal expects a ForInStep result"
                  pure (done, next)
                else pure ("false", stepValue)
                let accStores ← changedAssignments shape nextAcc acc true
                let next ← selectField result 1
                let stateStores ← if let some (stateShape, target) := owned then
                  changedAssignments stateShape next target true
                else do
                  let (.borrowed previous _, .borrowed next version) := (stateValue, next)
                    | throwError "C++ codegen: expected borrowed loop state"
                  unless previous == next && version == (← get).stateVersion do
                    throwError "C++ codegen: effect did not return its current storage handle"
                  pure #[]
                for line in accStores ++ stateStores do emit line
                emitBreak done
            else
              for line in ← changedAssignments shape (← evaluate step locals) acc true do emit line
      if let some (_, stateValue, _) := state then
        let stateValue ← match stateValue with
          | .borrowed text _ => pure (.borrowed text (← get).stateVersion)
          | _ => pure stateValue
        return .record #[accValue, stateValue]
      return accValue
    if name == ``Scratch.ofFn && args.size == 5 then
      unless (← get).boundedIndices do
        throwError "C++ codegen: scratch storage requires the bounded buffer backend"
      let some capacity := natLiteral (← expose args[1]! opaqueCalls)
        | throwError "C++ codegen: scratch capacity must be a natural literal"
      let element ← fieldShape args[0]! (← get).numberType
      let .scalar type := element
        | throwError "C++ codegen: scratch elements must be native scalars"
      unless #["double", "float", "bool", "std::int64_t", "std::uint64_t", "size_t", "unsigned"].contains type do
        throwError "C++ codegen: scratch elements must be native scalars"
      let (countType, count) ← scalar (← evaluate args[2]! locals)
      unless countType == "size_t" do throwError "C++ codegen: scratch requires a bounded count"
      let (_, count) ← scalar (← savedScalar countType count)
      let array ← fresh
      emit s!"std::array<{type}, {capacity}> {array};"
      let indexType ← mkAppM ``Fin #[args[2]!]
      let version := (← get).stateVersion
      emitIndexLoop count fun index =>
        withLocal `index indexType fun var => do
          let value ← evaluate (mkApp args[4]! var) ((var.fvarId!, .scalar "size_t" index) :: locals)
          unless version == (← get).stateVersion do
            throwError "C++ codegen: scratch element generation must be pure"
          commitValue element value s!"{array}[{index}]"
      return .scratch type array
    if name == ``Scratch.get && args.size == 6 then
      let .scratch type array ← evaluate args[3]! locals
        | throwError "C++ codegen: expected local scratch storage"
      let (indexType, index) ← scalar (← evaluate (← mkAppM ``Fin.val #[args[4]!]) locals)
      unless indexType == "size_t" do throwError "C++ codegen: expected a bounded scratch index"
      return .scalar type s!"{array}[{index}]"
    if name == ``Vector.ofFn && args.size == 3 then
      let .lit (.natVal count) ← whnf args[0]!
        | throwError "C++ codegen: indexed array capacity must be a literal: {args[0]!}"
      let shape ← fieldShape (← inferType e) (← get).numberType
      let .indexed type element := shape
        | throwError "C++ codegen: expected an indexed array"
      let array ← fresh
      emit s!"{type} {array};"
      let indexType ← mkAppM ``Fin #[args[0]!]
      emitIndexLoop (toString count) fun index =>
        withLocal `index indexType fun var => do
          let value ← evaluate (mkApp args[2]! var) ((var.fvarId!, .scalar "size_t" index) :: locals)
          commitValue element value s!"{array}[{index}]" true
      return .indexed type element array
    if (name == ``Vector.get && args.size == 4) || (name == ``Vector.set && args.size == 6) then
      let .indexed type element array ← materializeArray (← evaluate args[2]! locals)
        | throwError "C++ codegen: expected an indexed array"
      let indexExpr := args[3]!
      let indexValue ← if name == ``Vector.get then mkAppM ``Fin.val #[indexExpr] else pure indexExpr
      let index ← if let .lit (.natVal index) ← expose indexValue then
        pure (toString index)
      else do
        let (type, index) ← scalar (← evaluate indexExpr locals)
        unless type == "size_t" do throwError "C++ codegen: expected a bounded array index"
        pure index
      if name == ``Vector.get then
        let emission ← get
        if let some (version, source) := emission.stateSnapshots[array]? then
          if version == emission.stateVersion then
            return ← snapshot (readValue element s!"{source}[{index}]" true)
        return readValue element s!"{array}[{index}]" true
      let value ← evaluate args[4]! locals
      return .indexedWrite type element array index value
    if let some shape := (← get).recordState then
      if name == ``DSP.focus then
        let .record fields := shape | throwError "C++ codegen: focus expects record storage"
        let .borrowed receiver version ← evaluate args.back! locals
          | throwError "C++ codegen: focus needs the current parent storage"
        checkFocusedReceiver receiver
        checkBorrowedVersion (← get).stateVersion (.borrowed receiver version)
        let index ← focusedField args[0]! args[1]! args[3]! args[4]!
        let (field, childShape) := fields[index]!
        let child := s!"{receiver}.{field}"
        let outer := (← get).focusedReceiver
        modify fun s => {s with recordState := some childShape, focusedReceiver := some child}
        let result ← withLocal `child args[1]! fun childVar =>
          evaluate (mkApp args[5]! childVar)
            ((childVar.fvarId!, .borrowed child version) :: locals) (.action childShape child)
        modify fun s => {s with recordState := some shape, focusedReceiver := outer}
        let value ← selectField result 0
        return .record #[value, .borrowed receiver (← get).stateVersion]
      if name == ``StateT.bind then
        let action := args[args.size - 3]!
        let continuation := args[args.size - 2]!
        let state := args.back!
        let resultType ← whnf (← inferType (mkApp action state))
        let stateType ← inferType state
        let current ← evaluate state locals
        let actionDestination := match current with
          | .borrowed receiver _ => Destination.action shape receiver
          | _ => .value
        let result ← withLocal `current stateType fun currentVar =>
          evaluate (mkApp action currentVar) ((currentVar.fvarId!, current) :: locals) actionDestination
        let value ← selectField result 0
        let next ← selectField result 1
        return ← withLocal `result resultType.getAppArgs[0]! fun resultVar =>
          withLocal `state stateType fun stateVar =>
            evaluate (mkApp2 continuation resultVar stateVar)
              ((resultVar.fvarId!, value) :: (stateVar.fvarId!, next) :: locals) destination
      if #[``StateT.get, ``StateT.set, ``StateT.modifyGet].contains name then
        let .borrowed receiver version ← evaluate args.back! locals
          | throwError "C++ codegen: record state operation needs the current storage"
        checkFocusedReceiver receiver
        checkBorrowedVersion (← get).stateVersion (.borrowed receiver version)
        if name == ``StateT.get then
          let value ← snapshot (readValue shape receiver) true
          return .record #[value, .borrowed receiver version]
        let argument := args[args.size - 2]!
        let result ← if name == ``StateT.set then
          evaluate argument locals (.state shape receiver)
        else
          withLocal `current (← inferType args.back!) fun current =>
            evaluate (mkApp argument current) ((current.fvarId!, readValue shape receiver) :: locals)
              (.stateAndResult shape receiver)
        checkBorrowedVersion (← get).stateVersion (.borrowed receiver version)
        modify fun s => {s with stateVersion := version + 1}
        return .record #[result, .borrowed receiver (version + 1)]
    if let some (_, call) := stateCalls.find? (·.1 == name) then
      let values ← args.filterM fun arg => return !(← isProp (← inferType arg))
      unless values.size == 2 do throwError "C++ codegen: generated state call expects input and state"
      let .borrowed receiver version ← evaluate values[1]! locals
        | throwError "C++ codegen: generated state call needs the current storage"
      checkFocusedReceiver receiver
      checkBorrowedVersion (← get).stateVersion (.borrowed receiver version)
      let inputType ← whnf (← inferType values[0]!)
      let value ← evaluate values[0]! locals
      let arguments ← if inputType.isConstOf ``PUnit then pure receiver
        else if call.scalarInputs then do
          let .record fields := value
            | throwError "C++ codegen: scalar state call requires a record input"
          let fields ← fields.mapM fun field => return (← scalar field).2
          pure (String.intercalate ", " (receiver :: fields.toList))
        else do
          let shape ← fieldShape inputType none
          let input ← fresh
          emit s!"{call.cppName}::Input {input};"
          commitValue shape value input
          pure s!"{receiver}, {input}"
      checkBorrowedVersion (← get).stateVersion (.borrowed receiver version)
      let outputType ← whnf (← inferType e)
      unless outputType.isAppOfArity ``Prod 2 do
        throwError "C++ codegen: invalid generated state call result"
      let output ← whnf outputType.getAppArgs[0]!
      let invocation := s!"{call.cppName}::Process({arguments})"
      let result ← if output.isConstOf ``PUnit then do
        emit (invocation ++ ";")
        pure (.record #[])
      else do
        let shape ← fieldShape output none
        let result ← fresh
        emit s!"[[maybe_unused]] const auto {result} = {invocation};"
        pure (readValue shape result)
      modify fun s => {s with stateVersion := version + 1, stateHeaders :=
        if s.stateHeaders.contains call.header then s.stateHeaders else s.stateHeaders.push call.header}
      return .record #[result, .borrowed receiver (version + 1)]
    if let some effect := effects.find? (·.source == name) then
      if let some (single, double) := effect.generated then
        let exported := compiledActions.getState (← getEnv)
        let state := stateExports.getState (← getEnv)
        unless (exported.contains single || state.contains single) &&
            (exported.contains double || state.contains double) do
          throwError "C++ codegen: generated procedures for {name} must be exported first"
      unless effect.arguments + 1 ≤ args.size do
        throwError "C++ codegen: a state operation must be applied to its current state"
      let state ← evaluate args.back! locals
      let .borrowed receiver version := state
        | throwError "C++ codegen: a state binding needs borrowed storage"
      checkFocusedReceiver receiver
      unless version == (← get).stateVersion do throwError "C++ codegen: effect used an old state snapshot"
      let arguments ← (args.extract (args.size - effect.arguments - 1) (args.size - 1)).mapM fun arg => do
        let value ← evaluate arg locals
        if let .vector elements := value then
          let shape ← fieldShape (← inferType arg) (← get).numberType
          let type ← cppElementType shape true
          let elements ← elements.toList.mapM callArgument
          pure (type ++ "{{" ++ String.intercalate ", " elements ++ "}}")
        else callArgument value
      unless version == (← get).stateVersion do
        throwError "C++ codegen: a storage argument invalidated the current state"
      let call := s!"{receiver}.{effect.method}({String.intercalate ", " arguments.toList})"
      let result ← whnf (← inferType e)
      unless result.isAppOfArity ``Prod 2 do throwError "C++ codegen: invalid state operation result"
      let output ← whnf result.getAppArgs[0]!
      let value ← if output.isConstOf ``PUnit then do
        emit (call ++ ";")
        pure (.record #[])
      else if output.isAppOfArity ``Option 1 then do
        let inner ← fieldShape output.getAppArgs[0]! (← get).numberType
        let name ← fresh
        emit s!"[[maybe_unused]] const auto {name} = {call};"
        pure (.optional s!"{name}.m_present" (readValue inner s!"{name}.m_value"))
      else do
        let shape ← fieldShape output (← get).numberType
        let name ← fresh
        emit s!"[[maybe_unused]] const auto {name} = {call};"
        pure (readValue shape name)
      modify fun s => { s with stateVersion := s.stateVersion + 1 }
      return .record #[value, .borrowed receiver (← get).stateVersion]
    if name == ``Option.none && args.size == 1 then
      return .optional "false" (zeroValue (← fieldShape args[0]! (← get).numberType))
    if name == ``Option.some && args.size == 2 then
      return .optional "true" (← evaluate args[1]! locals)
    if (name == ``Option.isSome || name == ``Option.isNone) && args.size == 2 then
      let .optional present _ ← evaluate args[1]! locals
        | throwError "C++ codegen: expected an option"
      if name == ``Option.isSome then return .scalar "bool" present
      if present == "true" then return .scalar "bool" "false"
      if present == "false" then return .scalar "bool" "true"
      return ← temporary "bool" s!"!{present}"
    if (name == ``ForInStep.done || name == ``ForInStep.yield) && args.size == 2 then
      return .optional (if name == ``ForInStep.done then "true" else "false") (← evaluate args[1]! locals)
    if (name == ``ite || name == ``dite) && args.size ≥ 5 then
      let rest := args.extract 5 args.size
      let decision ← expose args[2]! opaqueCalls
      let .const decisionName _ := decision.getAppFn
        | throwError "C++ codegen: unsupported conditional decision {decision}"
      let decisionArgs := decision.getAppArgs
      let yes := if name == ``dite then
          withLocal `proof args[1]! fun proof => evaluate (mkAppN (mkApp args[3]! proof) rest) locals destination
        else evaluate (mkAppN args[3]! rest) locals destination
      let no := if name == ``dite then
          withLocal `proof (mkApp (mkConst ``Not) args[1]!) fun proof =>
            evaluate (mkAppN (mkApp args[4]! proof) rest) locals destination
        else evaluate (mkAppN args[4]! rest) locals destination
      if name == ``dite && decisionName == ``instDecidableAnd && decisionArgs.size == 4 then
        let test := fun i => mkAppOptM ``ite #[some (mkConst ``Bool), some decisionArgs[i]!,
          some decisionArgs[i + 2]!, some (mkConst ``Bool.true), some (mkConst ``Bool.false)]
        let (_, left) ← scalar (← evaluate (← test 0) locals)
        return ← emitBranch left (do
          let (_, right) ← scalar (← evaluate (← test 1) locals)
          emitBranch right yes no) no
      if name == ``ite && decisionName == ``instDecidableOr && decisionArgs.size == 4 then
        let inner ← mkAppOptM ``ite #[some args[0]!, some decisionArgs[1]!, some decisionArgs[3]!,
          some args[3]!, some args[4]!]
        let outer ← mkAppOptM ``ite #[some args[0]!, some decisionArgs[0]!, some decisionArgs[2]!,
          some args[3]!, some inner]
        return ← evaluate (mkAppN outer rest) locals destination
      if name == ``ite && decisionName == ``instDecidableAnd && decisionArgs.size == 4 then
        let inner ← mkAppOptM ``ite #[some args[0]!, some decisionArgs[1]!, some decisionArgs[3]!,
          some args[3]!, some args[4]!]
        let outer ← mkAppOptM ``ite #[some args[0]!, some decisionArgs[0]!, some decisionArgs[2]!,
          some inner, some args[4]!]
        return ← evaluate (mkAppN outer rest) locals destination
      if #[``Nat.decLt, ``Nat.decLe, ``Nat.decEq, ``Int.decLt, ``Int.decLe, ``Int.decEq].contains decisionName &&
          !(← get).boundedIndices then
        throwError "C++ codegen: unsupported external implementation {decisionName}"
      unless decisionArgs.size == 2 do
        throwError "C++ codegen: unsupported conditional decision {decision}"
      let conditionExpr ← if decisionName == ``Bool.decEq then
          pure (mkApp3 (mkConst ``Eq [1]) (mkConst ``Bool) decisionArgs[0]! decisionArgs[1]!)
        else if let some (_, predicate) := #[(``Float.decLt, ``Float.lt),
            (``Float.decLe, ``Float.le), (``Float32.decLt, ``Float32.lt),
            (``Float32.decLe, ``Float32.le), (``Nat.decLt, ``Nat.lt),
            (``Nat.decLe, ``Nat.le), (``Nat.decEq, ``Nat.beq), (``Int.decLt, ``Int.lt),
            (``Int.decLe, ``Int.le)].find? (fun entry => entry.1 == decisionName) then
          pure (mkAppN (mkConst predicate) decisionArgs)
        else if decisionName == ``Int.decEq || decisionName == ``UInt64.decEq then
          let type := if decisionName == ``Int.decEq then ``Int else ``UInt64
          pure (mkApp3 (mkConst ``Eq [1]) (mkConst type) decisionArgs[0]! decisionArgs[1]!)
        else throwError "C++ codegen: unsupported conditional decision {decision}"
      let (_, condition) ← scalar (← evaluate conditionExpr locals)
      return ← emitBranch condition yes no
    if name == ``Eq && args.size == 3 && (args[0]!.isConstOf ``Bool || args[0]!.isConstOf ``UInt64 ||
        (args[0]!.isConstOf ``Int && (← get).boundedIndices)) then
      let (_, a) ← scalar (← evaluate args[1]! locals)
      let (_, b) ← scalar (← evaluate args[2]! locals)
      if (a == "true" || a == "false") && (b == "true" || b == "false") then
        return .scalar "bool" (if a == b then "true" else "false")
      return ← temporary "bool" s!"({a} == {b})"
    if name == ``Bool.true then return .scalar "bool" "true"
    if name == ``Bool.false then return .scalar "bool" "false"
    if name == ``PUnit.unit then return .record #[]
    if name == ``Subtype.mk && args.size == 4 then
      return ← evaluate args[2]! locals destination
    if #[``Index.add, ``Index.sub, ``Index.ofNat, ``Index.toNat, ``Index.addNat, ``Index.fromFloat].contains name then
      unless (← get).boundedIndices do
        throwError "C++ codegen: proved index operations require bounded storage"
      if name == ``Index.toNat || name == ``Index.addNat then
        modify fun s => {s with usesWideIndices := true}
      let (_, left) ← scalar (← evaluate args[0]! locals)
      if name == ``Index.fromFloat || name == ``Index.ofNat then
        return ← temporary "std::int64_t" s!"static_cast<std::int64_t>({left})"
      if name == ``Index.toNat then return ← temporary "size_t" s!"static_cast<size_t>({left})"
      let (_, right) ← scalar (← evaluate args[1]! locals)
      let type := if name == ``Index.addNat then "size_t" else "std::int64_t"
      let op := if name == ``Index.sub then "-" else "+"
      return ← temporary type s!"({left} {op} {right})"
    if name == ``Fin.mk || name == ``Fin.val then
      let value ← evaluate args[1]! locals
      let (type, _) ← scalar value
      unless type == "size_t" do throwError "C++ codegen: expected a bounded index"
      return value
    if (name == ``Float32.toFloat || name == ``Float.toFloat32) && args.size == 1 then
      let (_, value) ← scalar (← evaluate args[0]! locals)
      let type := if name == ``Float32.toFloat then "double" else "float"
      return ← temporary type s!"static_cast<{type}>({value})"
    if #[``Float.toInt64, ``Int64.toInt, ``Int64.ofInt, ``Int64.toFloat].contains name then
      unless (← get).boundedIndices && args.size == 1 do
        throwError "C++ codegen: signed conversions require bounded storage"
      let value ← evaluate args[0]! locals
      let (_, a) ← scalar value
      if name == ``Float.toInt64 then
        return ← temporary "std::int64_t" s!"CodegenInteger::FromFloat({a})"
      if name == ``Int64.toFloat then return ← temporary "double" s!"static_cast<double>({a})"
      return value
    if name == ``Bool.not && args.size == 1 then
      let (_, value) ← scalar (← evaluate args[0]! locals)
      if value == "true" then return .scalar "bool" "false"
      if value == "false" then return .scalar "bool" "true"
      return ← temporary "bool" s!"!{value}"
    if (name == ``Bool.and || name == ``Bool.or) && args.size == 2 then
      let (_, left) ← scalar (← evaluate args[0]! locals)
      let right := evaluate args[1]! locals
      if name == ``Bool.and then return ← emitBranch left right (pure (.scalar "bool" "false"))
      return ← emitBranch left (pure (.scalar "bool" "true")) right
    if name == ``UInt64.ofNat then
      let some number := natLiteral (← expose args[0]!)
        | throwError "C++ codegen: UInt64 conversion needs a natural literal"
      return .scalar "std::uint64_t" s!"static_cast<std::uint64_t>({number % UInt64.size}ULL)"
    if name == ``UInt64.toFloat then
      let (_, value) ← scalar (← evaluate args[0]! locals)
      return ← temporary "double" s!"static_cast<double>({value})"
    match valueLowerings.find? name with
    | some (.numeric operation) =>
      let pairs ← args.mapM fun arg => do scalar (← evaluate arg locals)
      let some (type, _) := pairs[0]? | throwError "C++ codegen: native numeric operation needs operands"
      return ← emitNumeric operation type (pairs.map Prod.snd)
    | some (.packedArithmetic operation arity instances) =>
      if let some (doubleInstance, singleInstance) := instances then
        let expected ← match ← whnf args[0]! with
          | .const ``Float _ => pure doubleInstance
          | .const ``Float32 _ => pure singleInstance
          | _ => throwError "C++ codegen: packed arithmetic requires Float or Float32"
        let actual ← expose args[2]! #[expected]
        unless actual.isConstOf expected do
          throwError "C++ codegen: {name} requires the standard packed arithmetic instance {expected}"
      let pairs ← (args.extract (args.size - arity) args.size).mapM fun arg => do
        scalar (← evaluate arg locals)
      let values := pairs.map Prod.snd
      modify fun s => { s with stateHeaders := s.stateHeaders.push "MultiChannelNumber.hpp" }
      if let .curvePair header method := operation then
        return ← emitNumeric (.curvePair header method) pairs[0]!.1 values
      let result ← scalarType (← inferType e) (← get).numberType
      match operation, values with
      | .splat, #[x] => return ← temporary result s!"{result}::Splat({x})"
      | .binary op, #[a, b] => return ← binaryTemporary result op a b
      | .neg, #[a] => return ← negativeTemporary result a
      | .member method, #[x] => return ← temporary result s!"{x}.{method}()"
      | .method method, _ =>
        return ← temporary result (s!"{result}::{method}(" ++ String.intercalate ", " values.toList ++ ")")
      | .convert number, #[x] => return ← temporary result s!"{x}.template Convert<{number}>()"
      | _, _ => throwError "C++ codegen: invalid packed arithmetic application"
    | some (.packedStorage (.sampleWord fromWord)) =>
      let (type, value) ← scalar (← evaluate args.back! locals)
      unless (← get).numberType == some args[0]! && (← get).checkedIndices do
        throwError "C++ codegen: packed sample index requires the vector number type"
      if fromWord then
        unless type == "std::uint64_t" do throwError "C++ codegen: expected an unsigned sample word"
        return ← temporary "Vector" s!"Vector::Splat(static_cast<Number>({value}))"
      unless type == "Vector" do throwError "C++ codegen: expected a packed sample index"
      return ← temporary "std::uint64_t" s!"CodegenInteger::ToWord({value}[0])"
    | some (.packedStorage .sampleAges) =>
      let (type, value) ← scalar (← evaluate args.back! locals)
      unless type == "Vector" && (← get).numberType == some args[0]! do
        throwError "C++ codegen: quad age conversion expects packed sample positions"
      emit "static_assert(Size == 4);"
      return ← temporary quadAgesType s!"{value}.template Convert<std::uint32_t>()"
    | some (.packedStorage .packAges) =>
      let lanes ← args.mapM fun arg => do
        let (_, lane) ← scalar (← evaluate arg locals)
        pure s!"static_cast<std::uint32_t>({lane})"
      return ← temporary quadAgesType
        (quadAgesType ++ "{" ++ String.intercalate ", " lanes.toList ++ "}")
    | some (.packedStorage .broadcastLane) =>
      let (type, sample) ← scalar (← evaluate args[2]! locals)
      let some index := natLiteral (← expose (← mkAppM ``Fin.val #[args[3]!]))
        | throwError "C++ codegen: lane projection expects a static index"
      unless type == "Vector" && (← get).numberType == some args[0]! do
        throwError "C++ codegen: lane projection expects a packed sample"
      return ← temporary "Vector" s!"Vector::Splat({sample}[{index}])"
    | some (.packedStorage .gather) =>
      let .vector lanes ← evaluate args.back! locals
        | throwError "C++ codegen: gather expects four packed sources"
      unless lanes.size == 4 && (← get).numberType == some args[0]! do
        throwError "C++ codegen: gather expects four packed sources"
      let values ← lanes.mapIdxM fun i lane => do
        let ("Vector", name) ← scalar lane | throwError "C++ codegen: gather expects packed samples"
        pure s!"{name}[{i}]"
      emit "static_assert(Size == 4);"
      return ← temporary "Vector" ("Vector{" ++ String.intercalate ", " values.toList ++ "}")
    | none => pure ()
    if let some (_, operation) := #[(``UInt64.add, "+"), (``UInt64.sub, "-"), (``UInt64.mul, "*"),
        (``UInt64.xor, "^"), (``UInt64.shiftRight, ">>")].find? (fun entry => entry.1 == name) then
      let (_, left) ← scalar (← evaluate args[0]! locals)
      let (_, right) ← scalar (← evaluate args[1]! locals)
      let right := if operation == ">>" then s!"({right} & 63ULL)" else right
      return ← temporary "std::uint64_t" s!"({left} {operation} {right})"
    if #[``Nat.add, ``Nat.sub, ``Nat.lt, ``Nat.le, ``Nat.beq].contains name then
      unless (← get).boundedIndices do
        throwError "C++ codegen: runtime natural arithmetic requires the bounded buffer backend"
      let (_, a) ← scalar (← evaluate args[0]! locals)
      let (_, b) ← scalar (← evaluate args[1]! locals)
      if name == ``Nat.lt then return ← temporary "bool" s!"({a} < {b})"
      if name == ``Nat.le then return ← temporary "bool" s!"({a} <= {b})"
      if name == ``Nat.beq then return ← temporary "bool" s!"({a} == {b})"
      if name == ``Nat.sub then return ← temporary "size_t" s!"({a} < {b} ? 0 : {a} - {b})"
      if (← get).checkedIndices then return ← temporary "size_t" s!"CodegenInteger::Add<size_t>({a}, {b})"
      return ← temporary "size_t" s!"({a} + {b})"
    if (name == ``Int.ofNat || name == ``Int.toNat) && (← get).boundedIndices then
      let (_, a) ← scalar (← evaluate args[0]! locals)
      if name == ``Int.ofNat then return ← temporary "std::int64_t" s!"CodegenInteger::FromIndex({a})"
      return ← temporary "size_t" s!"CodegenInteger::ToIndex({a})"
    if let some (_, op) := #[(``Int.add, "+"), (``Int.sub, "-"), (``Int.lt, "<"),
        (``Int.le, "<=")].find? (fun entry => entry.1 == name) then
      unless (← get).boundedIndices do throwError "C++ codegen: signed arithmetic requires bounded storage"
      let (_, a) ← scalar (← evaluate args[0]! locals)
      let (_, b) ← scalar (← evaluate args[1]! locals)
      if op == "+" then return ← temporary "std::int64_t" s!"CodegenInteger::Add<std::int64_t>({a}, {b})"
      if op == "-" then return ← temporary "std::int64_t" s!"CodegenInteger::Subtract({a}, {b})"
      return ← temporary "bool" s!"({a} {op} {b})"
    if (name == ``Float.ofNat || name == ``Float32.ofNat) && args.size == 1 && (← get).boundedIndices then
      let arg ← expose args[0]! opaqueCalls
      if (natLiteral arg).isNone then
        let (source, a) ← scalar (← evaluate arg locals)
        unless source == "size_t" do throwError "C++ codegen: expected a bounded natural conversion"
        let type := if name == ``Float.ofNat then "double" else "float"
        return ← temporary type s!"static_cast<{type}>({a})"
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
      return ← binaryTemporary cppType op.2 a b
    if name == ``Float.neg || name == ``Float32.neg then
      let (type, value) ← scalar (← evaluate args[0]! locals)
      return ← negativeTemporary type value
    match ← getConstInfo name with
    | .ctorInfo info =>
      if args.isEmpty && (← enumConstructors info.induct).isSome then
        return .scalar "unsigned" s!"{info.cidx}u"
      unless isStructure (← getEnv) info.induct && args.size == info.numParams + info.numFields do
        throwError "C++ codegen: unsupported constructor {name}"
      return .record (← (args.extract info.numParams args.size).mapM (evaluate · locals))
    | .recInfo info =>
      if info.getMajorInduct == ``Prod then
        let major := info.getMajorIdx
        unless major < args.size do throwError "C++ codegen: missing argument for {name}"
        let .record #[first, second] ← evaluate args[major]! locals
          | throwError "C++ codegen: expected a pair for {name}"
        let arm := args[info.getFirstMinorIdx]!
        let rest := args.extract (major + 1) args.size
        return ← withLocal `first args[0]! fun a => withLocal `second args[1]! fun b =>
          evaluate (mkAppN (mkApp2 arm a b) rest) ((a.fvarId!, first) :: (b.fvarId!, second) :: locals)
      if info.getMajorInduct == ``Option then
        let major := info.getMajorIdx
        unless major < args.size do throwError "C++ codegen: missing argument for {name}"
        let .optional present payload ← evaluate args[major]! locals
          | throwError "C++ codegen: expected an option for {name}"
        let minors := info.getFirstMinorIdx
        let rest := args.extract (major + 1) args.size
        let yes := withLocal `payload args[0]! fun value =>
          evaluate (mkAppN (mkApp args[minors + 1]! value) rest) ((value.fvarId!, payload) :: locals)
        let no := evaluate (mkAppN args[minors]! rest) locals
        return ← emitBranch present yes no
      if let some ctors ← enumConstructors info.getMajorInduct then
        let major := info.getMajorIdx
        unless major < args.size do throwError "C++ codegen: missing argument for {name}"
        let ("unsigned", tag) ← scalar (← evaluate args[major]! locals)
          | throwError "C++ codegen: expected a constructor tag for {name}"
        let arm := fun ctor => evaluate (mkAppN e.getAppFn (args.set! major (mkConst ctor))) locals
        let last := ctors.size - 1
        let mut branches := arm ctors[last]!
        for i in (List.range last).reverse do
          branches := emitBranch s!"{tag} == {i}u" (arm ctors[i]!) branches
        return ← branches
    | _ => pure ()
    throwError "C++ codegen: unsupported expression {e}"

private partial def usesConcretePackedNumber : Shape → Bool
  | .scalar type => type.startsWith "MultiChannelNumber<float, " ||
      type.startsWith "MultiChannelNumber<double, "
  | .record fields => fields.any fun (_, field) => usesConcretePackedNumber field
  | .vector _ element | .indexed _ element => usesConcretePackedNumber element
  | .storage _ _ => false

private def withPackedNumberInclude (includes : Array String) (shapes : Array Shape) :
    Array String :=
  let header := "#include \"MultiChannelNumber.hpp\""
  if !includes.contains header && shapes.any usesConcretePackedNumber then includes.push header
  else includes

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
    let includes := withPackedNumberInclude #["#include <array>", "#include <cstdint>", "#include <stddef.h>"]
      #[fields]
    let header := #["#pragma once", "", s!"// Generated from {record}. Do not edit.", "//", ""] ++
      includes ++ #["", "template<typename Number>"]
    return String.intercalate "\n" (header ++ (← renderRecord cppName fields "")).toList

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
  let exports := availableRecords (← getEnv)
  let inputRecord := inputType.getAppFn.constName?.bind fun name =>
    exports.find? name
  let numericInput := (numberType == some inputType) ||
    (numberType.isNone && (inputType.isConstOf ``Float || inputType.isConstOf ``Float32))
  unless numericInput || inputRecord.isSome do
    throwError "C++ codegen: second argument must be numeric or an exported record"
  let inputNumber ← if numericInput then pure inputType else do
    unless inputType.getAppArgs.size == 1 do throwError "C++ codegen: record input expects one number type"
    pure inputType.getAppArgs[0]!
  unless (← isDefEq inputNumber resultType) || (numberType.isNone && inputRecord.isNone &&
      (resultType.isConstOf ``Float || resultType.isConstOf ``Float32)) do
    throwError "C++ codegen: function result must have the input's numeric type"
  unless (numberType == some inputNumber) ||
      (numberType.isNone && (inputNumber.isConstOf ``Float || inputNumber.isConstOf ``Float32)) do
    throwError "C++ codegen: record input must use the exported number type"
  let cppType ← scalarType inputNumber numberType
  let resultCppType ← scalarType resultType numberType
  let inputFields ← fieldShape inputType numberType
  let (includes, aliases, inputCppType) := match inputRecord with
    | none => (#[], #[], cppType)
    | some record => (#[s!"#include \"{record.header}\"", ""],
        #[s!"    using {inputName} = ::{record.cppName}<{cppType}>;", ""], inputName)
  let locals := [(runtime[0]!.fvarId!, readValue snapshotFields "snapshot"),
    (runtime[1]!.fvarId!, readValue inputFields "input")]
  let (_, emission) ← (do
    let (type, result) ← scalar (← evaluate (mkAppN (mkConst function) parameters) locals)
    unless type == resultCppType do throwError "C++ codegen: function result type mismatch"
    emit s!"return {result};").run { numberType, arithmetic }
  let mut body := pruneTemporaries emission.lines
  for name in #["snapshot", "input"] do
    unless body.any (fun line =>
        (line.splitToList fun c => !(c.isAlphanum || c == '_')).contains name) do
      body := #[s!"static_cast<void>({name});"] ++ body
  let record ← renderRecord "Snapshot" snapshotFields
  let declarations := if declareRecords then aliases ++ record else #[]
  let includes := withPackedNumberInclude (includes ++ emissionIncludes emission)
    #[snapshotFields, inputFields]
  return (includes, declarations ++
    #[s!"    static {resultCppType} {method}(const Snapshot& snapshot, const {inputCppType}& input)", "    {"] ++
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
  return (withPackedNumberInclude (emissionIncludes emission) #[inputFields, outputFields],
    (← renderRecord "Snapshot" outputFields) ++
    #["    static Snapshot MakeSnapshot(const LogicalSpec& spec)", "    {"] ++
    (pruneTemporaries body).map (fun line => if line.isEmpty then "" else "        " ++ line) ++ #["    }", ""])

def generateParts (function : Name) (cppName : String) (generic : Bool)
    (method : Option String) (inputName : String) (declareRecords : Bool := true)
    (tuningState : Option Expr := none) (captureSpec : Option Expr := none)
    (captureSnapshot : Option Expr := none)
    (tuningMethod : String := "ComputeCoefficients")
    (resetState : Option Expr := none) (preparation : Bool := false)
    (declareCache : Bool := true) (copySpec : Bool := false)
    (resetOutput : Option Expr := none) (boundedIndices : Bool := false) :
    MetaM (Array String × Array String) := do
  unless validIdentifier cppName && cppName.toList.head!.isUpper do
    throwError "C++ codegen: processor name must be a simple uppercase C++ identifier"
  if #["State", "Input", "Output"].contains cppName ||
      (generic && #["Number", "Size", "Vector"].contains cppName) then
    throwError "C++ codegen: processor name conflicts with a nested record"
  if (availableRecords (← getEnv)).toList.any (fun entry => entry.2.cppName == cppName) then
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
    let runtimeCount := if captureSpec.isSome || resetState.isSome || preparation then 1 else 2
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
            (name.any (fun source =>
              #[``Add, ``Sub, ``Mul, ``Div, ``Neg, ``OfNat, ``OfScientific].contains source ||
                arithmeticLowerings.contains source)) do
          throwError "C++ codegen: unsupported generic parameter {type}"
        if name == some ``OfNat then
          unless args.size == 2 && (natLiteral (← expose args[1]!)).isSome do
            throwError "C++ codegen: generic OfNat requires a literal numeral"
        arithmetic := arithmetic.push instanceParam.fvarId!
      runtime := parameters.extract (parameters.size - runtimeCount) parameters.size
    unless runtime.size == runtimeCount do
      throwError "C++ codegen: expected exactly {if runtimeCount == 2 then "two" else "one"} runtime arguments"
    if preparation then
      let inputType ← inferType runtime[0]!
      let inputShape ← fieldShape inputType numberType
      let outputShape ← fieldShape resultType numberType
      let .record _ := outputShape | throwError "C++ codegen: preparation must return a record"
      let (inputDeclaration, inputCppType) ← match inputShape with
        | .scalar ty => pure (#[],ty)
        | .record _ => pure (← renderRecord "Input" inputShape,"Input")
        | _ => throwError "C++ codegen: preparation input must be numeric or a record"
      let (_, emission) ← (do
        let output ← evaluate (mkAppN (mkConst function) parameters)
          [(runtime[0]!.fvarId!,readValue inputShape "input")]
        emit "Prepared result{};"
        commitValue outputShape output "result"
        emit "return result;").run {numberType,arithmetic}
      return (withPackedNumberInclude (emissionIncludes emission) #[inputShape, outputShape],
        inputDeclaration ++ (← renderRecord "Prepared" outputShape) ++
        #[s!"    static Prepared Prepare(const {inputCppType}& input)", "    {"] ++
        (pruneTemporaries emission.lines).map ("        " ++ ·) ++ #["    }", ""])
    if let some expected := resetState then
      let stateType ← inferType runtime[0]!
      let resultType ← whnf resultType
      let paired := resultType.isAppOfArity ``Prod 2
      let stateResult := if paired then resultType.getAppArgs[0]! else resultType
      unless (← isDefEq (← recordSignature stateType numberType) expected) &&
          (← isDefEq stateType stateResult) do
        throwError "C++ codegen: reset must take and return the processor's State"
      let outputFields ← if paired then do
        let some expectedOutput := resetOutput
          | throwError "C++ codegen: reset returns Output for a state-only processor"
        let outputType := resultType.getAppArgs[1]!
        unless ← isDefEq (← recordSignature outputType numberType) expectedOutput do
          throwError "C++ codegen: reset output differs from the processor's Output"
        pure (some (← fieldShape outputType numberType))
        else pure none
      let fields ← fieldShape stateType numberType
      let (_, emission) ← (do
        let result ← evaluate (mkAppN (mkConst function) parameters)
          [(runtime[0]!.fvarId!, readValue fields "m_state")]
        let next ← if paired then selectField result 0 else pure result
        let output ← if paired then some <$> snapshot (← selectField result 1) else pure none
        let assignments ← changedAssignments fields next "m_state"
        for line in assignments do emit line
        if let some outputFields := outputFields then
          commitValue outputFields output.get! "m_output").run {numberType, arithmetic}
      return (withPackedNumberInclude (emissionIncludes emission)
          (#[fields] ++ outputFields.toArray),
        #["    void Reset()", "    {"] ++ (pruneTemporaries emission.lines).map ("        " ++ ·) ++ #["    }", ""])
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
        let assignments ← changedAssignments stateFields result "m_state"
        for line in assignments do emit line
      | some outputFields =>
        if captureSnapshot.isSome then
          let next ← selectField result 0
          let output ← snapshot (← selectField result 1)
          let assignments ← changedAssignments stateFields next "m_state"
          for line in assignments do emit line
          commitValue outputFields output "m_responseSnapshot"
        else
          let output ← snapshot (← selectField result 1)
          let assignments ← changedAssignments stateFields (← selectField result 0) "m_state"
          for line in assignments do emit line
          commitValue outputFields output "m_output").run {
            numberType, arithmetic, boundedIndices, checkedIndices := boundedIndices}
    let body := if emission.lines.any (fun line => (line.splitOn (inputVariable ++ ".")).length > 1)
      then emission.lines else #[s!"static_cast<void>({inputVariable});"] ++ emission.lines
    if tuningState.isSome then
      let cache := if captureSnapshot.isSome && declareRecords && declareCache then
        #["    Snapshot m_responseSnapshot{};", "",
          "    Snapshot MakeSnapshot() const", "    {", "        return m_responseSnapshot;", "    }", ""]
        else #[]
      let specRecord ← if declareRecords then renderRecord inputName inputFields else pure #[]
      let specArgument := if copySpec then s!"{inputName} spec" else s!"const {inputName}& spec"
      -- Prepared banks are short-lived value aggregates. Keep their installation
      -- in the caller so scalar replacement can remove the aggregate copy.
      let inlineAttribute := if copySpec then #["    [[gnu::always_inline]]"] else #[]
      let lines := cache ++ specRecord ++ inlineAttribute ++
        #[s!"    void {tuningMethod}({specArgument})", "    {"] ++
        (pruneTemporaries body).map (fun line => if line.isEmpty then "" else "        " ++ line) ++ #["    }", ""]
      return (withPackedNumberInclude (emissionIncludes emission) #[inputFields], lines)
    let outputLines ← match outputFields with
      | none => pure #["    State m_state;", ""]
      | some outputFields => do
        let record ← renderRecord "Output" outputFields
        pure (record ++ #["    State m_state;", "    Output m_output;", ""])
    let lines := (← renderRecord "State" stateFields) ++ (← renderRecord "Input" inputFields) ++
      outputLines ++ #["    void Process(Input& input)", "    {"] ++
      (scheduleStores (pruneTemporaries body)).map (fun line => if line.isEmpty then "" else "        " ++ line) ++
      #["    }", ""]
    let indexIncludes := if boundedIndices then #["#include \"CodegenInteger.hpp\""] else #[]
    return (withPackedNumberInclude (indexIncludes ++ emissionIncludes emission)
        (#[stateFields, inputFields] ++ outputFields.toArray), lines)

private def generateResetParts (step reset : Name) (cppName : String) (generic : Bool) :
    MetaM (Array String × Array String) := do
  let (state, output) ← forallTelescopeReducing (← getConstInfo step).type fun parameters result => do
    let number := if generic then some parameters[0]! else none
    let state ← recordSignature (← inferType parameters[parameters.size - 2]!) number
    let result ← whnf result
    let output ← if result.isAppOfArity ``Prod 2 then
      some <$> recordSignature result.getAppArgs[1]! number
      else pure none
    return (state, output)
  let (includes, body) ← generateParts reset cppName generic none "Input"
    (resetState := some state) (resetOutput := output)
  return (includes, #[s!"    // Reset generated from {reset}.", "    //", ""] ++ body)

def generateWithTarget (function : Name) (cppName : String) (generic : Bool)
    (method : Option String) (reset : Option Name := none) : MetaM String := do
  let (includes, body) ← generateParts function cppName generic method "Input"
  let (resetIncludes, resetBody) ← match reset with
    | none => pure (#[], #[])
    | some name => generateResetParts function name cppName generic
  return String.intercalate "\n" (renderHeader function cppName generic (includes ++ resetIncludes) ++
    body ++ resetBody ++ #["};", ""]).toList

/-- Export a read-only computation over any compatible storage view. A view
provides the record members and indexing interface derived from the Lean type. -/
def generateView (function : Name) (cppName : String) (into : Bool := false) : MetaM String := do
  unless validIdentifier cppName do throwError "C++ codegen: invalid view export name"
  let info ← getConstInfo function
  unless info.levelParams.isEmpty do throwError "C++ codegen: view export needs a specialized function"
  forallTelescopeReducing info.type fun parameters resultType => do
    unless parameters.size == 1 do throwError "C++ codegen: view export expects one record input"
    let input := parameters[0]!
    let shape ← fieldShape (← inferType input) none
    let .record _ := shape | throwError "C++ codegen: view export expects one record input"
    let outputShape ← fieldShape resultType none
    if into then
      let .record _ := outputShape | throwError "C++ codegen: a result view needs a record output"
      pure ()
    let result ← match outputShape with
      | .scalar type => pure type
      | .record _ => pure "Output"
      | _ => cppElementType outputShape
    let declarations ← match outputShape with
      | .record _ => if into then pure #[] else renderRecord result outputShape
      | _ => pure #[]
    let (_, emission) ← (do
      let value ← evaluate (mkApp (mkConst function) input) [(input.fvarId!, readValue shape "input")]
      match outputShape with
      | .scalar type =>
        let (found, text) ← scalar value
        unless type == found do throwError "C++ codegen: view result type mismatch"
        emit s!"return {text};"
      | _ =>
        unless into do emit s!"{result} output;"
        let value ← if into then snapshot value else pure value
        commitValue outputShape value "output"
        unless into do emit "return output;").run {boundedIndices := true, checkedIndices := true}
    let body := (pruneTemporaries emission.lines).map fun line =>
      if line.isEmpty then "" else "        " ++ line
    let signature := if into then
      #["    template<typename Input, typename Output>",
        "    static void Evaluate([[maybe_unused]] const Input& input, Output& output)"]
      else #["    template<typename Input>",
        s!"    static {result} Evaluate([[maybe_unused]] const Input& input)"]
    let includes := withPackedNumberInclude (#["#include \"CodegenInteger.hpp\""] ++ emissionIncludes emission)
      #[shape, outputShape]
    return String.intercalate "\n" (renderHeader function cppName false includes ++ declarations ++
      signature ++ #["    {"] ++ body ++ #["    }", "", "};", ""]).toList

private partial def storageHeaders : Shape → Array String
  | .storage _ header => #[header]
  | .record fields => fields.flatMap (fun (_, field) => storageHeaders field)
  | .vector _ element | .indexed _ element => storageHeaders element
  | .scalar _ => #[]

/-- Export a StateM action over exclusive storage. A record view also supports
ordinary StateM reads and updates derived from its generated field layout.
The action returns its Lean result, with `PUnit` emitted as `void`. -/
def generateEffect (function : Name) (cppName : String) (ownRecord : Bool := false)
    (outlined : Bool := false) (generic : Bool := false) (declareState : Bool := false)
    (entryContract : Bool := false)
    (scalarInputs : Bool := false) (callable : Bool := false) : MetaM String := do
  unless validIdentifier cppName do throwError "C++ codegen: invalid state view export name"
  let info ← getConstInfo function
  unless info.levelParams.isEmpty do throwError "C++ codegen: state export needs a specialized action"
  forallTelescopeReducing info.type fun parameters resultType => do
    let values ← parameters.filterM fun parameter => return !(← isProp (← inferType parameter))
    unless (if generic then values.size ≥ 3 else values.size == 2) do
      throwError "C++ codegen: state export expects one input and one state"
    unless entryContract == (values.size < parameters.size) do
      throwError "C++ codegen: contract exports must explicitly declare their proof parameters with with_contract"
    let numberType ← if generic then do
      let number := values[0]!
      unless (← inferType number) == mkSort (.succ .zero) do
        throwError "C++ codegen: generic state export expects a number type"
      pure (some number)
    else pure none
    let arithmetic := if generic then (values.extract 1 (values.size - 2)).map Expr.fvarId! else #[]
    let input := values[values.size - 2]!
    let state := values.back!
    let inputType ← whnf (← inferType input)
    let shape ← fieldShape inputType numberType
    let noInput := (ownRecord || callable) && inputType.isConstOf ``PUnit
    let viewInput := (ownRecord && generic || callable) && !noInput && !scalarInputs
    let (inputValue, inputParameters) ← if scalarInputs then do
      let .record fields := shape
        | throwError "C++ codegen: scalar_inputs requires a record"
      let values ← fields.mapM fun (name, field) => do
        let .scalar type := field
          | throwError "C++ codegen: scalar_inputs requires scalar fields"
        let parameter := "input_" ++ name
        pure (Value.scalar type parameter, s!", {type} {parameter}")
      pure (Value.record (values.map Prod.fst), String.join (values.map Prod.snd).toList)
    else
      let inputParameterType := if viewInput then "InputView" else "Input"
      pure (readValue shape "input", if noInput then "" else s!", [[maybe_unused]] const {inputParameterType}& input")
    let recordState ← if ownRecord then do
      let storage ← fieldShape (← inferType state) numberType (nativeStorage := generic)
      let .record _ := storage | throwError "C++ codegen: state export expects record storage"
      pure (some storage)
    else do
      let .record _ := shape | throwError "C++ codegen: state export expects a record input"
      pure none
    let resultType ← whnf resultType
    unless resultType.isAppOfArity ``Prod 2 &&
        (← isDefEq resultType.getAppArgs[1]! (← inferType state)) do
      throwError "C++ codegen: state export expects a StateM action"
    let result ← whnf resultType.getAppArgs[0]!
    let noResult := result.isConstOf ``PUnit
    let outputShape ← fieldShape result numberType
    let returnType ← if noResult then pure "void" else
      match outputShape with
      | .record _ => pure "Output"
      | _ => cppElementType outputShape
    let env ← getEnv
    let stateCalls := (stateExports.getState env).toList.toArray.filter fun (source, target) =>
      source != function && target.borrowed == !ownRecord
    let (_, emission) ← (do
      let destination := recordState.elim Destination.value (Destination.action · "state")
      let result ← evaluate (mkAppN (mkConst function) parameters)
        [(input.fvarId!, inputValue), (state.fvarId!, .borrowed "state" 0)] destination
      let .record fields := result | throwError "C++ codegen: state action did not return a value and state"
      let .borrowed "state" version := fields[1]!
        | throwError "C++ codegen: action replaced its borrowed storage"
      unless version == (← get).stateVersion do
        throwError "C++ codegen: action returned an old state snapshot"
      unless noResult do
        let value ← selectField result 0
        match outputShape with
        | .scalar _ =>
          let (_, text) ← scalar value
          emit s!"return {text};"
        | _ =>
          emit s!"{returnType} output;"
          commitValue outputShape value "output"
          emit "return output;").run {
          stateDialect := true, boundedIndices := true, checkedIndices := true, recordState, stateCalls,
          numberType, arithmetic, foldVectorConstants := generic && ownRecord}
    let body := (pruneTemporaries emission.lines).map fun line =>
      if line.isEmpty then "" else "        " ++ line
    let inputDeclaration ← if !(ownRecord || callable) || noInput || scalarInputs then pure #[] else
      match shape with
      | .record _ => renderRecord "Input" shape
      | _ => pure #[s!"    using Input = {← cppElementType shape};", ""]
    let outputDeclaration ← match outputShape with
      | .record _ => if noResult then pure #[] else renderRecord "Output" outputShape
      | _ => pure #[]
    let parameters := "State& state" ++ inputParameters
    let template := if viewInput then "    template<typename State, typename InputView>"
      else if ownRecord || callable || scalarInputs then "    template<typename State>"
      else "    template<typename State, typename Input>"
    let inlineAttribute := if outlined then "[[gnu::noinline]] " else ""
    let includes := withPackedNumberInclude
      (#["#include \"CodegenInteger.hpp\"", "#include <tuple>"] ++
        emissionIncludes emission (recordState.elim #[] storageHeaders))
      (#[shape, outputShape] ++ recordState.toArray)
    let stateDeclaration ← if declareState then
      renderRecord "State" recordState.get!
    else pure #[]
    let sizeAssert := if emission.usesWideIndices then
      #["    static_assert(std::numeric_limits<std::size_t>::digits >= 64);", ""] else #[]
    return String.intercalate "\n" (renderHeader function cppName generic includes ++
      sizeAssert ++ stateDeclaration ++ inputDeclaration ++ outputDeclaration ++
      #[template, s!"    {inlineAttribute}static {returnType} Process({parameters})", "    {"] ++
      body ++ #["    }", "", "};", ""]).toList

/-- Assemble processing and response definitions in one enclosing filter.
They share the same scalar/vector target; each retains its own Lean source. -/
private partial def firstScalarField : Shape → String
  | .record fields => fields[0]!.1 ++ (match fields[0]!.2 with
      | .scalar _ => ""
      | rest => "." ++ firstScalarField rest)
  | _ => ""

def generateFilterResponses (step : Name) (responses : Array (Name × String))
    (cppName : String) (generic : Bool) (tuning : Option Name := none)
    (capture : Bool := false) (preparedTuning : Option Name := none)
    (reset : Option Name := none) (preparedBank : Option Name := none)
    (preparation : Option Name := none) (externalResponse : Bool := false) : MetaM String := do
  if preparedBank.isSome && !(generic && capture && tuning.isSome) then
    throwError "C++ codegen: prepared bank requires a generic sampled filter"
  if preparation.isSome && preparedBank.isNone then
    throwError "C++ codegen: preparation requires a prepared bank installer"
  if preparedTuning.isSome && !(capture && tuning.isSome) then
    throwError "C++ codegen: prepared updater requires a sampled filter"
  if #["Snapshot", "Frequency", "LogicalSpec", "PreparedSpec", "Preparation"].contains cppName then
    throwError "C++ codegen: filter name conflicts with a response member"
  if responses.isEmpty && !externalResponse then throwError "C++ codegen: filter needs at least one response"
  let (stepIncludes, stepBody) ← generateParts step cppName generic none "Input"
  let mut includes := stepIncludes
  let mut body := stepBody
  let mut methods := #["Process", "Reset", "State", "Output", "LogicalSpec", "ComputeCoefficients", "ComputeCoefficientsFromSnapshot", "MakeSnapshot", "UIState", "PreparedSpec", "InstallPrepared", "Preparation"]
  let mut tuningMembers := #[]
  if let some tuning := tuning then
    let info ← getConstInfo step
    let expected ← forallTelescopeReducing info.type fun parameters _ => do
      let type ← inferType parameters[parameters.size - 2]!
      recordSignature type (if generic then some parameters[0]! else none)
    let snapshotSignature ← if externalResponse then do
      forallTelescopeReducing (← getConstInfo tuning).type fun parameters result => do
        let result ← whnf result
        unless result.isAppOfArity ``Prod 2 do
          throwError "C++ codegen: external response requires tuning to return State and Snapshot"
        return some (← recordSignature result.getAppArgs[1]! (if generic then some parameters[0]! else none))
    else if capture then do
      let responseType := (← getConstInfo responses[0]!.1).type
      forallTelescopeReducing responseType fun parameters _ => do
        let type ← inferType parameters[parameters.size - 2]!
        return some (← recordSignature type (if generic then some parameters[0]! else none))
      else pure none
    let (extraIncludes, tuningBody) ← generateParts tuning cppName generic none "LogicalSpec" true
      (some expected) none snapshotSignature
    includes := includes ++ extraIncludes
    if externalResponse then
      let fields ← forallTelescopeReducing (← getConstInfo tuning).type fun parameters result => do
        let result ← whnf result
        fieldShape result.getAppArgs[1]! (if generic then some parameters[0]! else none)
      tuningMembers ← renderRecord "Snapshot" fields
    tuningMembers := tuningMembers ++ #[s!"    // Coefficients generated from {tuning}.", "    //", ""] ++ tuningBody
    if let some prepared := preparedTuning then
      let (extraIncludes, preparedBody) ← generateParts prepared cppName generic none "Snapshot" false
        (some expected) none snapshotSignature "ComputeCoefficientsFromSnapshot"
      includes := includes ++ extraIncludes
      tuningMembers := tuningMembers ++ #[s!"    // Prepared coefficients generated from {prepared}.", "    //", ""] ++ preparedBody
    if let some prepared := preparedBank then
      let (extraIncludes, preparedBody) ← generateParts prepared cppName generic none "PreparedSpec" true
        (some expected) none snapshotSignature "InstallPrepared" (declareCache := false) (copySpec := true)
      includes := includes ++ extraIncludes
      tuningMembers := tuningMembers ++ preparedBody
  if let some prepare := preparation then
    let (extraIncludes, preparationBody) ← generateParts prepare cppName true none "Input" (preparation := true)
    for line in extraIncludes do
      unless includes.contains line do includes := includes.push line
    body := body ++ #["    template<size_t Width>", "    struct Preparation", "    {",
      "        using Vector = MultiChannelNumber<Number, Width>;", ""] ++
      preparationBody.map (fun line => if line.isEmpty then "" else "    " ++ line) ++ #["    };", ""]
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
  if let some reset := reset then
    let (resetIncludes, resetBody) ← generateResetParts step reset cppName generic
    includes := includes ++ resetIncludes
    body := body ++ resetBody
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
  return String.intercalate "\n" (renderHeader step cppName generic includes ++ body ++ #["};", ""]).toList

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

def generateGeneric (function : Name) (cppName : String) (reset : Option Name := none) : MetaM String :=
  generateWithTarget function cppName true none reset

/-- Export the same pure preparation used by filter banks as a standalone API. -/
def generatePreparation (function : Name) (cppName : String) (generic : Bool) : MetaM String := do
  if #["Prepared", "Prepare"].contains cppName then
    throwError "C++ codegen: preparation name conflicts with a nested member"
  let (includes, body) ← generateParts function cppName generic none "Input" (preparation := true)
  return String.intercalate "\n" (renderHeader function cppName generic includes ++ body ++ #["};", ""]).toList

def generateFunction (function : Name) (cppName method : String) (generic : Bool) : MetaM String :=
  generateWithTarget function cppName generic (some method)

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
  let exports := availableRecords (← getEnv)
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

syntax cppReset := "resetWith " ident

elab "#generate_cpp_generic " function:ident resetSyntax:(cppReset)?
    " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateGeneric function.getId cppName.getId.toString
    (resetSyntax.map (fun p => p.raw[1].getId))
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

elab "#generate_cpp_bounded " function:ident " as " cppName:ident " to " path:str : command => do
  let source := function.getId
  let name := cppName.getId.toString
  let (includes, body) ← liftTermElabM <|
    generateParts source name false none "Input" (boundedIndices := true)
  let code := String.intercalate "\n" (renderHeader source name false includes ++ body ++ #["};", ""]).toList
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {source} with bounded indices"

elab "#generate_cpp_view " function:ident " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateView function.getId cppName.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId} as a read-only view"

elab "#generate_cpp_view_into " function:ident " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateView function.getId cppName.getId.toString (into := true)
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  modifyEnv fun env => compiledActions.addEntry env function.getId
  logInfo m!"Generated {output} from {function.getId} into a result view"

private def checkStateExport (source : Name) (cppName : String) (output : System.FilePath) :
    CommandElabM String := do
  let header := cppName ++ ".hpp"
  unless output.fileName == some header do
    throwError "C++ codegen: state header must be named {header}"
  let exports := stateExports.getState (← getEnv)
  if exports.contains source || exports.toList.any (fun entry => entry.2.cppName == cppName) then
    throwError "C++ codegen: state action is already exported or its C++ name is in use"
  return header

elab "#generate_cpp_effect " function:ident callable:(&"callable")? contract:(&"with_contract")?
    scalarInputs:(&"scalar_inputs")? " as " cppName:ident
    " to " path:str : command => do
  let source := function.getId
  let target := cppName.getId.toString
  let output : System.FilePath := path.getString
  let binding ← if callable.isSome then do
    let header ← checkStateExport source target output
    pure (some ({ cppName := target, header, borrowed := true, scalarInputs := scalarInputs.isSome } : StateExport))
  else pure none
  let code ← liftTermElabM <| generateEffect function.getId cppName.getId.toString
    (entryContract := contract.isSome) (scalarInputs := scalarInputs.isSome) (callable := callable.isSome)
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  modifyEnv fun env => compiledActions.addEntry env function.getId
  if let some binding := binding then
    modifyEnv fun env => stateExports.addEntry env (source, binding)
  logInfo m!"Generated {output} from {function.getId} as a state action"

elab "#generate_cpp_effect_generic " function:ident " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateEffect function.getId cppName.getId.toString (generic := true)
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  modifyEnv fun env => compiledActions.addEntry env function.getId
  logInfo m!"Generated {output} from {function.getId} as a packed state action"

private def exportStateAction (source : Name) (cppName path : String)
    (outlined : Bool) (declareState : Bool := false) : CommandElabM Unit := do
  let output : System.FilePath := path
  let header ← checkStateExport source cppName output
  let code ← liftTermElabM <| generateEffect source cppName
    (ownRecord := true) (outlined := outlined) (declareState := declareState)
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  modifyEnv fun env => stateExports.addEntry env (source, { cppName, header })
  logInfo m!"Generated {output} from {source} as an in-place record state action"

elab "#generate_cpp_state " function:ident " as " cppName:ident " to " path:str : command =>
  exportStateAction function.getId cppName.getId.toString path.getString false

elab "#generate_cpp_state " function:ident " outlined " "as " cppName:ident " to " path:str : command =>
  exportStateAction function.getId cppName.getId.toString path.getString true

elab "#generate_cpp_state " function:ident " owning " "as " cppName:ident " to " path:str : command =>
  exportStateAction function.getId cppName.getId.toString path.getString false true

private def exportPackedStateAction (function : Name) (cppName path : String)
    (declareState : Bool) : CommandElabM Unit := do
  let code ← liftTermElabM <| generateEffect function cppName
    (ownRecord := true) (generic := true) (declareState := declareState)
  let output : System.FilePath := path
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function} as an in-place packed record state action"

elab "#generate_cpp_state_generic " function:ident " as " cppName:ident " to " path:str : command =>
  exportPackedStateAction function.getId cppName.getId.toString path.getString false

elab "#generate_cpp_state_generic " function:ident " owning " "as " cppName:ident " to " path:str : command =>
  exportPackedStateAction function.getId cppName.getId.toString path.getString true

private def exportPreparation (function : Name) (cppName path : String) (generic : Bool) :
    CommandElabM Unit := do
  let code ← liftTermElabM <| generatePreparation function cppName generic
  let output : System.FilePath := path
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function} as pure coefficient preparation"

elab "#generate_cpp_preparation " function:ident " as " cppName:ident
    " to " path:str : command =>
  exportPreparation function.getId cppName.getId.toString path.getString false

elab "#generate_cpp_preparation_generic " function:ident " as " cppName:ident
    " to " path:str : command =>
  exportPreparation function.getId cppName.getId.toString path.getString true

elab "#generate_cpp_function_generic " function:ident " as " cppName:ident " method " methodName:ident
    " to " path:str : command => do
  let code ← liftTermElabM <| generateFunction function.getId cppName.getId.toString
    methodName.getId.toString true
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId} as a pure function with MultiChannelNumber arithmetic"

syntax cppBank := "preparedBank " ident " prepareWith " ident

elab "#generate_cpp_sampled_filter " step:ident tuning:ident
    preparedSyntax:(cppPrepared)? resetSyntax:(cppReset)?
    "[" entries:cppResponse,* "]" " as " cppName:ident " to " path:str : command => do
  let pairs ← entries.getElems.mapM fun entry => do
    match entry with
    | `(cppResponse| $r:ident as $m:ident) => pure (r.getId, m.getId.toString)
    | _ => throwError "C++ codegen: invalid response entry"
  let code ← liftTermElabM <| generateFilterResponses step.getId pairs cppName.getId.toString false (some tuning.getId) true
    (preparedSyntax.map (fun p => p.raw[1].getId))
    (resetSyntax.map (fun p => p.raw[1].getId))
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with captured response and sampled UI"

elab "#generate_cpp_sampled_filter_generic " step:ident tuning:ident
    preparedSyntax:(cppPrepared)? resetSyntax:(cppReset)? bankSyntax:(cppBank)?
    "[" entries:cppResponse,* "]" " as " cppName:ident " to " path:str : command => do
  let pairs ← entries.getElems.mapM fun entry => do
    match entry with
    | `(cppResponse| $r:ident as $m:ident) => pure (r.getId, m.getId.toString)
    | _ => throwError "C++ codegen: invalid response entry"
  let code ← liftTermElabM <| generateFilterResponses step.getId pairs cppName.getId.toString true (some tuning.getId) true
    (preparedSyntax.map (fun p => p.raw[1].getId))
    (resetSyntax.map (fun p => p.raw[1].getId))
    (bankSyntax.map (fun p => p.raw[1].getId))
    (bankSyntax.map (fun p => p.raw[3].getId))
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with captured response and sampled UI"

/-- Generate the same sampled processor and snapshot for a response evaluated by
an external reader, such as an audited response table. -/
elab "#generate_cpp_sampled_processor_generic " step:ident tuning:ident "resetWith" reset:ident
    " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateFilterResponses step.getId #[] cppName.getId.toString true
    (some tuning.getId) (reset := some reset.getId) (externalResponse := true)
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} with captured response coordinates"

end DSP.Codegen
