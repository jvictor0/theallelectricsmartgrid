import Lean
import DSP.IfLess
import DSP.FMA
import DSP.Transcendental
import DSP.Nonlinear
import DSP.ChannelMean
import DSP.DelayMath
import DSP.DelayBuffer.Basic
import DSP.Codegen.Control
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
  deriving Inhabited

inductive Value where
  | scalar (type text : String)
  | record (fields : Array Value)
  | vector (elements : Array Value)
  | indexed (type : String) (element : Shape) (text : String)
  | indexedWrite (type : String) (element : Shape) (base index : String) (value : Value)
  | optional (present : String) (payload : Value)
  | borrowed (text : String) (version : Nat)
  deriving Inhabited

/-- A method or function supplied by a storage backend for a typed Lean
operation. Mutating methods return their receiver as the new state value. -/
structure CallBinding where
  source : Name
  target : String
  arguments : Nat
  result : String
  method : Bool := false
  mutates : Bool := false

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
  calls : Array CallBinding := #[]
  stateDialect : Bool := false
  stateVersion : Nat := 0
  foldVectorConstants : Bool := false

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
    let temporary := match tokens with
      | "const" :: _ :: name :: _ =>
        if name.startsWith "v" && (name.drop 1).toString.toList.all Char.isDigit then some name else none
      | _ => none
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

def scalarType (type : Expr) (numberType : Option Expr) : MetaM String := do
  let type ← whnf type
  if numberType == some type then return "Vector"
  if type.isAppOfArity ``Fin 1 then return "size_t"
  match type with
  | .const ``Float _ => return "double"
  | .const ``Float32 _ => return "float"
  | .const ``Bool _ => return "bool"
  | .const ``Int _ => return "std::int64_t"
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
  | .scalar type | .indexed type _ => return type
  | .vector n element => return s!"std::array<{← cppElementType element packed}, {n}>"
  | .record fields =>
    unless packed do throwError "C++ codegen: an array element needs a nameable C++ type"
    let types ← fields.toList.mapM fun (_, field) => cppElementType field true
    return s!"std::tuple<{String.intercalate ", " types}>"

partial def fieldShape (type : Expr) (numberType : Option Expr)
    (numberName : String := "Vector") : MetaM Shape := do
  let type ← whnf type
  if numberType == some type then return .scalar numberName
  if type.isConstOf ``PUnit then return .record #[]
  if type.isAppOfArity ``Vector 2 then
    let args := type.getAppArgs
    let .lit (.natVal count) ← whnf args[1]!
      | throwError "C++ codegen: indexed array capacity must be a literal: {args[1]!}"
    let element ← fieldShape args[0]! numberType numberName
    return .indexed s!"std::array<{← cppElementType element true}, {count}>" element
  if let .forallE _ domain body _ := type then
    unless !body.hasLooseBVars do throwError "C++ codegen: dependent vector elements are unsupported"
    return .vector (← finSize domain) (← fieldShape body numberType numberName)
  if type.isConstOf ``Float || type.isConstOf ``Float32 || type.isConstOf ``Bool || type.isConstOf ``Int ||
      type.isAppOfArity ``Fin 1 then
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
      fields := fields.push (member, ← fieldShape fieldType numberType numberName)
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

partial def zeroValue : Shape → Value
  | .scalar type => .scalar type (type ++ "{}")
  | .record fields => .record (fields.map fun (_, shape) => zeroValue shape)
  | .vector n element => .vector (Array.replicate n (zeroValue element))
  | .indexed type element => .indexed type element (type ++ "{}")

def primitive (name : Name) : Bool :=
  #[``Vector.ofFn, ``Vector.get, ``Vector.set, ``Fin.foldl, ``Fin.foldlM, ``DSP.foldUntil, ``DSP.foldUntilM,
    ``Trig.sinFloat, ``Trig.cosFloat, ``Trig.tanFloat,
    ``Trig.sinFloat32, ``Trig.cosFloat32, ``Trig.tanFloat32,
    ``ExpDecay.float, ``ExpDecay.float32, ``FMA.float, ``FMA.float32, ``Float.add, ``Float.sub, ``Float.mul, ``Float.div, ``Float.neg,
    ``Float32.add, ``Float32.sub, ``Float32.mul, ``Float32.div, ``Float32.neg,
    ``Float32.toFloat, ``Float.toFloat32, ``Int.ofNat, ``Int.toNat,
    ``Int.add, ``Int.sub, ``Int.lt, ``Int.le, ``Int.decLt, ``Int.decLe, ``Int.decEq,
    ``Float.ofNat, ``Float32.ofNat, ``Float.ofScientific, ``Float32.ofScientific,
    ``Float.sqrt, ``Float32.sqrt, ``Float.floor, ``Float.lt, ``Float.le, ``Float32.lt, ``Float32.le,
    ``Float.beq, ``Float32.beq, ``ite, ``Bool.true, ``Bool.false, ``Eq,
    ``Bool.decEq, ``Float.decLt, ``Float.decLe, ``Float32.decLt, ``Float32.decLe,
    ``dite, ``instDecidableOr, ``instDecidableAnd,
    ``Fin.mk, ``Fin.val, ``Nat.lt, ``Nat.le, ``Nat.decLt, ``Nat.decLe,
    ``DelayMath.exp2Float, ``DelayMath.floorFloat, ``DelayMath.phaseFloat, ``DelayMath.decayFloat,
    ``DelayMath.exp2Float32, ``DelayMath.floorFloat32, ``DelayMath.phaseFloat32, ``DelayMath.decayFloat32].contains name

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
      if primitive name then return e
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

def emitDelayMath (index : Nat) (type : String) (values : Array String) : GenM Value := do
  modify fun s => { s with usesMath := true }
  let vector := type == "Vector"
  let literal (single double : String) := if vector then
    s!"Vector::Splat(static_cast<Number>(std::is_same_v<Number, float> ? {single} : {double}))"
    else if type == "float" then single else double
  let x := values[0]!
  if index == 0 then
    return ← temporary type (if vector then s!"{x}.Exp2()" else s!"MultiChannelNumber<{type}, 1>::Splat({x}).Exp2()[0]")
  if index == 1 then
    return ← temporary type (if vector then s!"{x}.Floor()" else s!"std::floor({x})")
  if index == 2 then
    return ← temporary type s!"(Math::ATan2({x}, {values[1]!}) / {literal "0xC90FDBp-21f" "0x1921FB54442D18p-50"})"
  if index == 3 then
    let ln1000f ← hexLiteral (6.907755278982137052053974364053 : Float32).toBits.toNat 23 8 127 "f"
    let ln1000 ← hexLiteral (6.907755278982137052053974364053 : Float).toBits.toNat 52 11 1023 ""
    let (_, arg) ← scalar (← temporary type s!"(-({literal ln1000f ln1000} * {x}))")
    return ← temporary type (if vector then s!"{arg}.Exp()" else s!"MultiChannelNumber<{type}, 1>::Splat({arg}).Exp()[0]")
  throwError "C++ codegen: unsupported delay preparation operation"

private partial def callArgument : Value → GenM String
  | .scalar _ text => pure text
  | .record fields => return "{" ++ String.intercalate ", " (← fields.toList.mapM callArgument) ++ "}"
  | .vector _ => throwError "C++ codegen: a bound call requires packed vectors"
  | .indexed _ _ text => pure text
  | value@(.indexedWrite ..) => do callArgument (← materializeArray value)
  | .optional .. => throwError "C++ codegen: a bound call needs an explicit option representation"
  | .borrowed .. => throwError "C++ codegen: a value binding cannot consume borrowed state"

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

/-- Collect changed leaves, capturing projected reads before committing them.
An identity projection is omitted, including unchanged process coefficients. -/
partial def changedAssignments (shape : Shape) (value : Value) (target : String)
    (owned : Bool := false) (packed : Bool := false) : GenM (Array String) := do
  match shape, value with
  | .scalar type, .scalar type' text =>
    unless type == type' do throwError "C++ codegen: updated field type mismatch"
    if text == target then return #[]
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
    if text == target then return #[]
    let (_, saved) ← scalar (← savedScalar type text)
    return #[s!"{target} = {saved};"]
  | .indexed type element, .indexedWrite type' _ base index next =>
    unless type == type' do throwError "C++ codegen: updated array type mismatch"
    if owned && base == target then
      let (_, index) ← scalar (← savedScalar "size_t" index)
      return ← changedAssignments element next s!"{target}[{index}]" owned true
    changedAssignments shape (← materializeArray value) target owned packed
  | _, _ => throwError "C++ codegen: updated result shape mismatch"

/-- Capture projected leaves while reusing existing saved expression values. -/
partial def snapshot (value : Value) : GenM Value := do
  match value with
  | .scalar type text => savedScalar type text
  | .record fields => return .record (← fields.mapM snapshot)
  | .vector fields => return .vector (← fields.mapM snapshot)
  | .indexed type element text =>
    let (_, saved) ← scalar (← savedScalar type text)
    return .indexed type element saved
  | value@(.indexedWrite ..) => snapshot (← materializeArray value)
  | .optional present payload =>
    let (_, present) ← scalar (← savedScalar "bool" present)
    return .optional present (← snapshot payload)
  | .borrowed .. => throwError "C++ codegen: borrowed state cannot be saved by value"

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

partial def evaluate (expression : Expr) (locals : Locals) : GenM Value := do
  let calls := (← get).calls
  let effects := if (← get).stateDialect then stateBindings else #[]
  let opaqueCalls := calls.map (·.source) ++ effects.map (·.source) ++
    (if (← get).stateDialect then #[``StateT.get] else #[]) ++
    (if (← get).boundedIndices then #[``Nat.decEq, ``Nat.beq] else #[])
  let e ← expose expression opaqueCalls
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
      if name == ``DSP.SquareRoot && values.size == 1 && index == 0 then
        return ← temporary "Vector" s!"{values[0]!}.Sqrt()"
      if name == ``DSP.ChannelMean && values.size == 1 && index == 0 then
        return ← temporary "Vector" s!"{values[0]!}.MeanBroadcast()"
      if name == ``DSP.DelayMath then
        return ← emitDelayMath index "Vector" values
      if name == ``DSP.Tanh && values.size == 1 && index == 0 then
        -- Lower the executable interpretation, Tanh.rational, with exactly its
        -- multiplication/FMA order and ordered clamp. Native tests cover both.
        let x := values[0]!
        let (_, x2) ← scalar (← temporary "Vector" s!"({x} * {x})")
        let (_, numerator) ← scalar (← temporary "Vector" s!"({x} * ({x2} + Vector::Splat(27)))")
        let (_, denominator) ← scalar (← temporary "Vector"
          s!"Vector::FusedMultiplyAdd({x} * Vector::Splat(9), {x}, Vector::Splat(27))")
        let (_, y) ← scalar (← temporary "Vector" s!"({numerator} / {denominator})")
        return ← temporary "Vector" s!"Vector::Min(Vector::Splat(1), Vector::Max(Vector::Splat(-1), {y}))"
      if name == ``DSP.IfLess && values.size == 4 then
        return ← temporary "Vector"
          s!"Vector::IfLess({values[0]!}, {values[1]!}, {values[2]!}, {values[3]!})"
      if name == ``DSP.FMA && values.size == 3 then
        return ← temporary "Vector"
          s!"Vector::FusedMultiplyAdd({values[0]!}, {values[1]!}, {values[2]!})"
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
    if ((name == ``Fin.foldl || name == ``DSP.foldUntil) && args.size == 4) ||
        ((name == ``Fin.foldlM || name == ``DSP.foldUntilM) && args.size == 7) then
      let monadic := name == ``Fin.foldlM || name == ``DSP.foldUntilM
      let earlyExit := name == ``DSP.foldUntilM
      let offset := if monadic then 3 else 1
      let countExpr := args[offset]!
      let count ← if let .lit (.natVal count) ← expose countExpr (calls.map (·.source)) then
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
                let result ← evaluate (mkApp step stateVar)
                  ((stateVar.fvarId!, stateValue) :: locals)
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
      let index ← if let .lit (.natVal index) ← expose indexValue (calls.map (·.source)) then
        pure (toString index)
      else do
        let (type, index) ← scalar (← evaluate indexExpr locals)
        unless type == "size_t" do throwError "C++ codegen: expected a bounded array index"
        pure index
      if name == ``Vector.get then return readValue element s!"{array}[{index}]" true
      let value ← evaluate args[4]! locals
      return .indexedWrite type element array index value
    if let some call := calls.find? (·.source == name) then
      unless call.arguments ≤ args.size do
        throwError "C++ codegen: missing bound call arguments for {name}"
      let values ← (args.extract (args.size - call.arguments) args.size).mapM (evaluate · locals)
      let arguments ← values.toList.mapM callArgument
      let (callee, arguments) ← if call.method then do
          let (_, receiver) ← scalar values[0]!
          pure (s!"{receiver}.{call.target}", arguments.drop 1)
        else pure (call.target, arguments)
      let code := callee ++ "(" ++ String.intercalate ", " arguments ++ ")"
      if call.mutates then
        emit (code ++ ";")
        return values[0]!
      return ← temporary call.result code
    if let some effect := effects.find? (·.source == name) then
      if let some (single, double) := effect.generated then
        let exported := compiledActions.getState (← getEnv)
        unless exported.contains single && exported.contains double do
          throwError "C++ codegen: generated procedures for {name} must be exported first"
      unless effect.arguments + 1 ≤ args.size do
        throwError "C++ codegen: a state operation must be applied to its current state"
      let state ← evaluate args.back! locals
      let .borrowed receiver version := state
        | throwError "C++ codegen: a state binding needs borrowed storage"
      unless version == (← get).stateVersion do throwError "C++ codegen: effect used an old state snapshot"
      let arguments ← (args.extract (args.size - effect.arguments - 1) (args.size - 1)).mapM fun arg => do
        callArgument (← evaluate arg locals)
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
    if (name == ``ForInStep.done || name == ``ForInStep.yield) && args.size == 2 then
      return .optional (if name == ``ForInStep.done then "true" else "false") (← evaluate args[1]! locals)
    if (name == ``ite || name == ``dite) && args.size ≥ 5 then
      let rest := args.extract 5 args.size
      let decision ← expose args[2]! opaqueCalls
      let .const decisionName _ := decision.getAppFn
        | throwError "C++ codegen: unsupported conditional decision {decision}"
      let decisionArgs := decision.getAppArgs
      let yes := if name == ``dite then
          withLocal `proof args[1]! fun proof => evaluate (mkAppN (mkApp args[3]! proof) rest) locals
        else evaluate (mkAppN args[3]! rest) locals
      let no := if name == ``dite then
          withLocal `proof (mkApp (mkConst ``Not) args[1]!) fun proof =>
            evaluate (mkAppN (mkApp args[4]! proof) rest) locals
        else evaluate (mkAppN args[4]! rest) locals
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
        return ← evaluate (mkAppN outer rest) locals
      if name == ``ite && decisionName == ``instDecidableAnd && decisionArgs.size == 4 then
        let inner ← mkAppOptM ``ite #[some args[0]!, some decisionArgs[1]!, some decisionArgs[3]!,
          some args[3]!, some args[4]!]
        let outer ← mkAppOptM ``ite #[some args[0]!, some decisionArgs[0]!, some decisionArgs[2]!,
          some inner, some args[4]!]
        return ← evaluate (mkAppN outer rest) locals
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
        else if decisionName == ``Int.decEq then
          pure (mkApp3 (mkConst ``Eq [1]) (mkConst ``Int) decisionArgs[0]! decisionArgs[1]!)
        else throwError "C++ codegen: unsupported conditional decision {decision}"
      let (_, condition) ← scalar (← evaluate conditionExpr locals)
      return ← emitBranch condition yes no
    if name == ``Eq && args.size == 3 && (args[0]!.isConstOf ``Bool ||
        (args[0]!.isConstOf ``Int && (← get).boundedIndices)) then
      let (_, a) ← scalar (← evaluate args[1]! locals)
      let (_, b) ← scalar (← evaluate args[2]! locals)
      if (a == "true" || a == "false") && (b == "true" || b == "false") then
        return .scalar "bool" (if a == b then "true" else "false")
      return ← temporary "bool" s!"({a} == {b})"
    if name == ``Bool.true then return .scalar "bool" "true"
    if name == ``Bool.false then return .scalar "bool" "false"
    if name == ``PUnit.unit then return .record #[]
    if name == ``Fin.mk || name == ``Fin.val then
      let value ← evaluate args[1]! locals
      let (type, _) ← scalar value
      unless type == "size_t" do throwError "C++ codegen: expected a bounded index"
      return value
    if (name == ``Float32.toFloat || name == ``Float.toFloat32) && args.size == 1 then
      let (_, value) ← scalar (← evaluate args[0]! locals)
      let type := if name == ``Float32.toFloat then "double" else "float"
      return ← temporary type s!"static_cast<{type}>({value})"
    if name == ``Float.floor && args.size == 1 then
      let (_, value) ← scalar (← evaluate args[0]! locals)
      return ← temporary "double" s!"std::floor({value})"
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
    if let some index := #[``DelayMath.exp2Float, ``DelayMath.floorFloat,
        ``DelayMath.phaseFloat, ``DelayMath.decayFloat].idxOf? name then
      let pairs ← args.mapM fun arg => do scalar (← evaluate arg locals)
      return ← emitDelayMath index pairs[0]!.1 (pairs.map Prod.snd)
    if let some index := #[``DelayMath.exp2Float32, ``DelayMath.floorFloat32,
        ``DelayMath.phaseFloat32, ``DelayMath.decayFloat32].idxOf? name then
      let pairs ← args.mapM fun arg => do scalar (← evaluate arg locals)
      return ← emitDelayMath index pairs[0]!.1 (pairs.map Prod.snd)
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
    if name == ``Float.sqrt || name == ``Float32.sqrt then
      unless args.size == 1 do throwError "C++ codegen: invalid square root application"
      let (type, value) ← scalar (← evaluate args[0]! locals)
      return ← temporary type s!"std::sqrt({value})"
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
  let mut body := pruneTemporaries emission.lines
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
    (pruneTemporaries body).map (fun line => if line.isEmpty then "" else "        " ++ line) ++ #["    }", ""])

def generateParts (function : Name) (cppName : String) (generic : Bool)
    (method : Option String) (inputName : String) (declareRecords : Bool := true)
    (tuningState : Option Expr := none) (captureSpec : Option Expr := none)
    (captureSnapshot : Option Expr := none)
    (tuningMethod : String := "ComputeCoefficients")
    (resetState : Option Expr := none) (preparation : Bool := false)
    (declareCache : Bool := true) (copySpec : Bool := false)
    (resetOutput : Option Expr := none) : MetaM (Array String × Array String) := do
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
            #[``Add, ``Sub, ``Mul, ``Div, ``Neg, ``OfNat, ``OfScientific, ``DSP.IfLess, ``DSP.FMA, ``DSP.Trig, ``DSP.ExpDecay, ``DSP.Tanh, ``DSP.SquareRoot, ``DSP.ChannelMean, ``DSP.DelayMath].any
              (fun supported => name == some supported) do
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
      return (mathIncludes emission.usesMath, inputDeclaration ++ (← renderRecord "Prepared" outputShape) ++
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
      return (mathIncludes emission.usesMath,
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
          commitValue outputFields output "m_output").run { numberType, arithmetic }
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
      return (mathIncludes emission.usesMath, lines)
    let outputLines ← match outputFields with
      | none => pure #["    State m_state;", ""]
      | some outputFields => do
        let record ← renderRecord "Output" outputFields
        pure (record ++ #["    State m_state;", "    Output m_output;", ""])
    let lines := (← renderRecord "State" stateFields) ++ (← renderRecord "Input" inputFields) ++
      outputLines ++ #["    void Process(Input& input)", "    {"] ++
      (scheduleStores (pruneTemporaries body)).map (fun line => if line.isEmpty then "" else "        " ++ line) ++
      #["    }", ""]
    return (mathIncludes emission.usesMath, lines)

def generateWithTarget (function : Name) (cppName : String) (generic : Bool)
    (method : Option String) : MetaM String := do
  let (includes, body) ← generateParts function cppName generic method "Input"
  return String.intercalate "\n" (renderHeader function cppName generic includes ++ body ++ #["};", ""]).toList

/-- Export a read-only computation over any compatible storage view. A view
provides the record members and indexing interface derived from the Lean type. -/
def generateView (function : Name) (cppName : String) : MetaM String := do
  unless validIdentifier cppName do throwError "C++ codegen: invalid view export name"
  let info ← getConstInfo function
  unless info.levelParams.isEmpty do throwError "C++ codegen: view export needs a specialized function"
  forallTelescopeReducing info.type fun parameters resultType => do
    unless parameters.size == 1 do throwError "C++ codegen: view export expects one record input"
    let input := parameters[0]!
    let shape ← fieldShape (← inferType input) none
    let .record _ := shape | throwError "C++ codegen: view export expects one record input"
    let outputShape ← fieldShape resultType none
    let result ← match outputShape with
      | .scalar type => pure type
      | .record _ => pure "Output"
      | _ => cppElementType outputShape
    let declarations ← if let .record _ := outputShape then renderRecord result outputShape else pure #[]
    let (_, emission) ← (do
      let value ← evaluate (mkApp (mkConst function) input) [(input.fvarId!, readValue shape "input")]
      match outputShape with
      | .scalar type =>
        let (found, text) ← scalar value
        unless type == found do throwError "C++ codegen: view result type mismatch"
        emit s!"return {text};"
      | _ =>
        emit s!"{result} output;"
        commitValue outputShape value "output"
        emit "return output;").run {boundedIndices := true, checkedIndices := true}
    let body := (pruneTemporaries emission.lines).map fun line =>
      if line.isEmpty then "" else "        " ++ line
    return String.intercalate "\n" (renderHeader function cppName false
        (#["#include \"CodegenInteger.hpp\""] ++ mathIncludes emission.usesMath) ++ declarations ++
      #["    template<typename Input>", s!"    static {result} Evaluate([[maybe_unused]] const Input& input)",
        "    {"] ++ body ++ #["    }", "", "};", ""]).toList

/-- Export a StateM action over an exclusive native storage view. The source
can observe the storage only through registered state primitives. -/
def generateEffect (function : Name) (cppName : String) : MetaM String := do
  unless validIdentifier cppName do throwError "C++ codegen: invalid state view export name"
  let info ← getConstInfo function
  unless info.levelParams.isEmpty do throwError "C++ codegen: state export needs a specialized action"
  forallTelescopeReducing info.type fun parameters resultType => do
    unless parameters.size == 2 do throwError "C++ codegen: state export expects one input and one state"
    let input := parameters[0]!
    let state := parameters[1]!
    let shape ← fieldShape (← inferType input) none
    let .record _ := shape | throwError "C++ codegen: state export expects a record input"
    let resultType ← whnf resultType
    unless resultType.isAppOfArity ``Prod 2 && resultType.getAppArgs[0]!.isConstOf ``PUnit do
      throwError "C++ codegen: state export expects a StateM action returning PUnit"
    let (_, emission) ← (do
      let result ← evaluate (mkAppN (mkConst function) parameters)
        [(input.fvarId!, readValue shape "input"), (state.fvarId!, .borrowed "state" 0)]
      let .borrowed "state" version ← selectField result 1
        | throwError "C++ codegen: action replaced its borrowed storage"
      unless version == (← get).stateVersion do
        throwError "C++ codegen: action returned an old state snapshot").run {
          stateDialect := true, boundedIndices := true, checkedIndices := true}
    let body := (pruneTemporaries emission.lines).map fun line =>
      if line.isEmpty then "" else "        " ++ line
    return String.intercalate "\n" (renderHeader function cppName false
        (#["#include \"CodegenInteger.hpp\"", "#include <tuple>"] ++ mathIncludes emission.usesMath) ++
      #["    template<typename State, typename Input>",
        "    static void Process(State& state, [[maybe_unused]] const Input& input)", "    {"] ++
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
    (preparation : Option Name := none) : MetaM String := do
  if preparedBank.isSome && !(generic && capture && tuning.isSome) then
    throwError "C++ codegen: prepared bank requires a generic sampled filter"
  if preparation.isSome && preparedBank.isNone then
    throwError "C++ codegen: preparation requires a prepared bank installer"
  if preparedTuning.isSome && !(capture && tuning.isSome) then
    throwError "C++ codegen: prepared updater requires a sampled filter"
  if #["Snapshot", "Frequency", "LogicalSpec", "PreparedSpec", "Preparation"].contains cppName then
    throwError "C++ codegen: filter name conflicts with a response member"
  if responses.isEmpty then throwError "C++ codegen: filter needs at least one response"
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
    let info ← getConstInfo step
    let expected ← forallTelescopeReducing info.type fun parameters _ => do
      let type ← inferType parameters[parameters.size - 2]!
      recordSignature type (if generic then some parameters[0]! else none)
    let expectedOutput ← forallTelescopeReducing info.type fun parameters result => do
      let result ← whnf result
      if result.isAppOfArity ``Prod 2 then
        return some (← recordSignature result.getAppArgs[1]! (if generic then some parameters[0]! else none))
      return none
    let (resetIncludes, resetBody) ← generateParts reset cppName generic none "Input"
      (resetState := some expected) (resetOutput := expectedOutput)
    includes := includes ++ resetIncludes
    body := body ++ #[s!"    // Reset generated from {reset}.", "    //", ""] ++ resetBody
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

def generateGeneric (function : Name) (cppName : String) : MetaM String :=
  generateWithTarget function cppName true none

/-- Export the same pure preparation used by filter banks as a standalone API. -/
def generatePreparationGeneric (function : Name) (cppName : String) : MetaM String := do
  if #["Prepared", "Prepare"].contains cppName then
    throwError "C++ codegen: preparation name conflicts with a nested member"
  let (includes, body) ← generateParts function cppName true none "Input" (preparation := true)
  return String.intercalate "\n" (renderHeader function cppName true includes ++ body ++ #["};", ""]).toList

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

elab "#generate_cpp_view " function:ident " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateView function.getId cppName.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId} as a read-only view"

elab "#generate_cpp_effect " function:ident " as " cppName:ident " to " path:str : command => do
  let code ← liftTermElabM <| generateEffect function.getId cppName.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  modifyEnv fun env => compiledActions.addEntry env function.getId
  logInfo m!"Generated {output} from {function.getId} as a state action"

elab "#generate_cpp_preparation_generic " function:ident " as " cppName:ident
    " to " path:str : command => do
  let code ← liftTermElabM <| generatePreparationGeneric function.getId cppName.getId.toString
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId} as pure coefficient preparation"

elab "#generate_cpp_function_generic " function:ident " as " cppName:ident " method " methodName:ident
    " to " path:str : command => do
  let code ← liftTermElabM <| generateFunction function.getId cppName.getId.toString
    methodName.getId.toString true
  let output : System.FilePath := path.getString
  if let some parent := output.parent then IO.FS.createDirAll parent
  IO.FS.writeFile output code
  logInfo m!"Generated {output} from {function.getId} as a pure function with MultiChannelNumber arithmetic"

syntax cppReset := "resetWith " ident
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

end DSP.Codegen
