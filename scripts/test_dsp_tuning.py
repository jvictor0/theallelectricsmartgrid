"""Coefficient lowering: direct Math4096 oracle, native Lean error, state preservation."""
import math
import re
import struct
import subprocess

LP = ['G', 's', 'y']
SVF = ['k', 'a1', 'a2', 'a3', 'ic1', 'ic2', 'lowPass', 'bandPass', 'highPass', 'notch', 'allPass']
def nest(prefix, fields):
    return [prefix + '.' + f for f in fields]

# Independent schemas also make accidental field/layout changes visible here.
SCHEMAS = {
    'ConservativeReuppingLadderCore': (['cutoff', 'resonance', 'gain'],
        ['alpha', 'saturation.gain', 'saturation.tanhGain', 'k',
         'stage1', 'stage2', 'stage3', 'stage4', 'input', 'output']),
    'OnePoleZOHLowPass': (['cutoff'], ['alpha', 'y']),
    'OnePoleTPTLowPass': (['cutoff'], LP),
    'OnePoleTPTHighPass': (['cutoff'], nest('lowPass', LP) + ['y']),
    'TPTBaseWidth': (['highPass.cutoff', 'lowPass.cutoff'],
                     nest('highPass.lowPass', LP) + ['highPass.y'] + nest('lowPass', LP)),
    'TPTSVF': (['cutoff', 'q'], SVF),
    'MorphingTPTSVF': (['svf.cutoff', 'svf.q', 'blend'], nest('svf', SVF) + ['cosine', 'sine', 'blend', 'output']),
    'TPTLadder': (['cutoff', 'feedback'], ['k', 'drive'] + sum((nest('stage'+str(i), LP) for i in range(1, 5)), [])),
}
for mode in ['LowPass', 'BandPass', 'HighPass']:
    SCHEMAS['TPTSVF4Pole'+mode] = (['stage1.cutoff', 'stage1.q', 'stage2.cutoff', 'stage2.q'],
                                 nest('stage1', SVF)+nest('stage2', SVF))

SPLITTER = 'LinkwitzRileySplitter4Band'
CONSERVATIVE = 'ConservativeReuppingLadderCore'
def phase_fields(prefix):
    return nest(prefix+'.svf', SVF) + nest(prefix, ['low.x', 'high.x'])
def two_band_fields(prefix):
    return nest(prefix+'.first', SVF) + nest(prefix+'.second', SVF) + nest(prefix,
        ['lowPhase.x', 'highPhase.x', 'low.x', 'high.x'])
SCHEMAS[SPLITTER] = (['tree.value', 'tree.low.value', 'tree.high.value'],
    nest('tree.first', SVF) + nest('tree.second', SVF) + phase_fields('tree.lowPhase') +
    phase_fields('tree.highPhase') + two_band_fields('tree.low') + two_band_fields('tree.high') +
    [f'bands[{i}]' for i in range(4)])

def member(path):
    return '.'.join('m_'+p for p in path.split('.'))

def coefficient_oracle(name):
    result = {}
    def lp(prefix, fc):
        result[prefix+'G'] = f'Gain(p[{fc}])'
    def svf(prefix, fc, q):
        for i, field in enumerate(SVF[:4]):
            result[prefix+field] = f'TuneSVF(p[{fc}], {q})[{i}]'
    if name == CONSERVATIVE:
        for i, field in enumerate(['alpha', 'saturation.gain', 'saturation.tanhGain', 'k']):
            result[field] = f'TuneConservative(p[0], p[1], p[2])[{i}]'
    elif name == 'OnePoleZOHLowPass':
        result['alpha'] = 'Number(1) - MultiChannelNumber<Number,1>::Splat(-(Number(6.283185307179586476925286766559) * p[0])).Exp()[0]'
    elif name == 'OnePoleTPTLowPass': lp('', 0)
    elif name == 'OnePoleTPTHighPass': lp('lowPass.', 0)
    elif name == 'TPTBaseWidth':
        lp('highPass.lowPass.', 0)
        lp('lowPass.', 1)
    elif name == 'TPTSVF': svf('', 0, 'p[1]')
    elif name == 'MorphingTPTSVF':
        svf('svf.', 0, 'p[1]')
        result.update(cosine='Cos(Phase(p[2]))', sine='Sin(Phase(p[2]))', blend='p[2]')
    elif name.startswith('TPTSVF4Pole'):
        svf('stage1.', 0, 'p[1]')
        svf('stage2.', 2, 'p[3]')
    elif name == 'TPTLadder':
        for i in range(1, 5): lp(f'stage{i}.', 0)
        result['k'] = 'p[1]'
    elif name == SPLITTER:
        for prefix, cutoff in [('tree.first.', 0), ('tree.second.', 0),
                ('tree.lowPhase.svf.', 2), ('tree.highPhase.svf.', 1),
                ('tree.low.first.', 1), ('tree.low.second.', 1),
                ('tree.high.first.', 2), ('tree.high.second.', 2)]:
            svf(prefix, cutoff, 'Cos(Number(0.125))')
    return result

def snapshot_oracle(name):
    if name == CONSERVATIVE:
        t = 'TuneConservative(p[0], p[1], p[2])'
        return {'memoryRatio': f'(Number(1)-{t}[0])/{t}[0]',
                'slope': f'{t}[1]/{t}[2]', 'k': f'{t}[3]'}
    g = lambda n: f'Math4096::TanPi(MultiChannelNumber<Number,1>::Splat(p[{n}]))[0]'
    if name == 'OnePoleZOHLowPass': return {'alpha': coefficient_oracle(name)['alpha']}
    if name in ('OnePoleTPTLowPass', 'OnePoleTPTHighPass'): return {'g': g(0)}
    if name == 'TPTBaseWidth': return {'highPass.g': g(0), 'lowPass.g': g(1)}
    if name == 'TPTSVF': return {'g': g(0), 'k': 'Number(1)/p[1]'}
    if name == 'MorphingTPTSVF':
        return {'svf.g': g(0), 'svf.k': 'Number(1)/p[1]',
                'low': 'p[2] > Number(0.5) ? Number(0) : Cos(Phase(p[2]))',
                'band': 'p[2] > Number(0.5) ? Cos(Phase(p[2])) : Sin(Phase(p[2]))',
                'high': 'p[2] > Number(0.5) ? Sin(Phase(p[2])) : Number(0)'}
    if name.startswith('TPTSVF4Pole'):
        return {'stage1.g': g(0), 'stage1.k': 'Number(1)/p[1]',
                'stage2.g': g(2), 'stage2.k': 'Number(1)/p[3]'}
    if name == 'TPTLadder': return dict([(f'g{i}', g(0)) for i in range(1,5)] + [('k','p[1]')])
    raise AssertionError(name)

PRELUDE = r'''
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <type_traits>
#include "Math.hpp"

template<typename Number>
std::array<Number,4> TuneConservative(Number cutoff, Number resonance, Number gain)
{
    using Mono = MultiChannelNumber<Number,1>;
    const Number sine = Math4096::Sin2pi(Mono::Splat(std::min(Number(0.5),cutoff)*Number(0.5)))[0];
    const Number root = std::sqrt(Number(1)+sine*sine);
    const Number alpha = sine*Number(2)*(root-sine);
    const Number ratio = sine*Number(2)*(root+sine);
    const Number crossing = ratio+Number(2);
    const Number nyquist = Number(1)+Number(2)/std::max(ratio,Number(2));
    const Number nyquistSquared = nyquist*nyquist;
    const Number critical = ratio < Number(2) ? crossing*crossing : nyquistSquared*nyquistSquared;
    const Number g2 = gain*gain;
    const Number saturation = std::min(Number(1),std::max(Number(-1),
        gain*(g2+Number(27))/std::fma(gain*Number(9),gain,Number(27))));
    const Number slope = gain/saturation;
    return {alpha,gain,saturation,resonance*Number(0.975)*critical/(slope*slope*slope)};
}

template<typename Number>
using Bits = std::conditional_t<std::is_same_v<Number,float>, uint32_t, uint64_t>;

template<typename Number>
Bits<Number> Raw(Number value)
{
    Bits<Number> bits;
    std::memcpy(&bits, &value, sizeof(value));
    return bits;
}

template<typename Number>
Number Read()
{
    Bits<Number> bits;
    std::cin >> bits;
    Number value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

template<typename Number>
Number Cos(Number x)
{
    return Math4096::Cos2pi(MultiChannelNumber<Number,1>::Splat(x))[0];
}

template<typename Number>
Number Sin(Number x)
{
    return Math4096::Sin2pi(MultiChannelNumber<Number,1>::Splat(x))[0];
}

template<typename Number>
Number Gain(Number cutoff)
{
    const Number g = Math4096::TanPi(MultiChannelNumber<Number,1>::Splat(cutoff))[0];
    assert(g > 0);
    const Number gain = g/(Number(1)+g);
    assert(std::abs(Number(1)-Number(2)*gain) < Number(1));
    return gain;
}

template<typename Number>
Number Phase(Number blend)
{
    return (blend > Number(0.5)) ? (blend-Number(0.5))/Number(2) : blend/Number(2);
}

template<typename Number>
std::array<Number,4> TuneSVF(Number cutoff, Number q)
{
    const Number g = Math4096::TanPi(MultiChannelNumber<Number,1>::Splat(cutoff))[0];
    const Number k = Number(1)/q;
    const Number a1 = Number(1)/std::fma(g,g+k,Number(1));
    const Number a2 = g*a1;
    const Number a3 = g*a2;
    // Check the poles of the rounded integrator-state update, independently
    // of the real-number pole proof.
    //
    const double a = 2.0*double(a1)-1.0;
    const double d = 1.0-2.0*double(a3);
    const double trace = a+d;
    const double determinant = a*d+4.0*double(a2)*double(a2);
    const auto root = std::sqrt(std::complex<double>(trace*trace-4.0*determinant,0));
    assert(std::abs((trace+root)/2.0) < 1.0);
    assert(std::abs((trace-root)/2.0) < 1.0);
    return {k,a1,a2,a3};
}
'''

def runner_source():
    code = PRELUDE + '#include "OnePoleTPTLowPass32.hpp"\n'
    for name in SCHEMAS:
        if name != SPLITTER:
            code += f'#include "{name}.hpp"\n'
        code += f'#include "{name}Generic.hpp"\n'
    for name, (spec, fields) in SCHEMAS.items():
        oracle = coefficient_oracle(name)
        code += f'''
template<typename Number, size_t Size>
void Test{name}()
{{
    {name}Generic<Number,Size> filter{{}};
    typename {name}Generic<Number,Size>::LogicalSpec spec;
    std::array<std::array<Number,{len(spec)}>,Size> inputs;
    std::array<std::array<Number,{len(fields)}>,Size> before;
    for (size_t lane = 0; lane < Size; ++lane)
    {{
'''
        for i, f in enumerate(spec):
            code += f'        inputs[lane][{i}] = Read<Number>();\n        spec.{member(f)}[lane] = inputs[lane][{i}];\n'
        for i, f in enumerate(fields):
            code += f'        before[lane][{i}] = Read<Number>();\n        filter.m_state.{member(f)}[lane] = before[lane][{i}];\n'
        code += '    }\n\n'
        if name == 'TPTSVF':
            code += '    auto fromSnapshot = filter;\n'
        code += '''    filter.ComputeCoefficients(spec);
    const auto first = filter.m_state;
'''
        if name != SPLITTER:
            code += '    const auto captured = filter.MakeSnapshot();\n'
        if name == 'TPTSVF':
            code += '''    fromSnapshot.ComputeCoefficientsFromSnapshot(captured);
    fromSnapshot.ComputeCoefficientsFromSnapshot(captured);
'''
        code += '''    filter.ComputeCoefficients(spec);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        const auto& p = inputs[lane];
'''
        for i, f in enumerate(fields):
            expr = oracle.get(f, f'before[lane][{i}]')
            code += f'''        assert(Raw(filter.m_state.{member(f)}[lane]) == Raw({expr}));
        assert(Raw(filter.m_state.{member(f)}[lane]) == Raw(first.{member(f)}[lane]));
        std::cout << Raw(filter.m_state.{member(f)}[lane]) << ' ';
'''
            if name == 'TPTSVF':
                code += f'        assert(Raw(fromSnapshot.m_state.{member(f)}[lane]) == Raw(first.{member(f)}[lane]));\n'
        if name != SPLITTER:
            for f, expr in snapshot_oracle(name).items():
                code += f'''        assert(Raw(captured.{member(f)}[lane]) == Raw({expr}));
        assert(Raw(filter.MakeSnapshot().{member(f)}[lane]) == Raw(captured.{member(f)}[lane]));
        std::cout << Raw(captured.{member(f)}[lane]) << ' ';
'''
                if name == 'TPTSVF':
                    code += f'        assert(Raw(fromSnapshot.MakeSnapshot().{member(f)}[lane]) == Raw(captured.{member(f)}[lane]));\n'
        if name == SPLITTER:
            code += '        const auto snapshot = decltype(filter)::MakeSnapshot(spec);\n'
            for index, node in enumerate(['tree', 'tree.low', 'tree.high']):
                code += f'        assert(Raw(snapshot.{member(node)}.m_value.m_g[lane]) == Raw(Math4096::TanPi(MultiChannelNumber<Number,1>::Splat(p[{index}]))[0]));\n'
                code += f'        assert(Raw(snapshot.{member(node)}.m_value.m_k[lane]) == Raw(Number(1) / Cos(Number(0.125))));\n'
        if name != SPLITTER:
            code += f'''        if constexpr (std::is_same_v<Number,double>)
        {{
            {name} scalar{{}};
            {name}::LogicalSpec scalarSpec;
'''
            for i, f in enumerate(spec):
                code += f'            scalarSpec.{member(f)} = p[{i}];\n'
            for i, f in enumerate(fields):
                code += f'            scalar.m_state.{member(f)} = before[lane][{i}];\n'
            if name == 'TPTSVF':
                code += '            auto scalarFromSnapshot = scalar;\n'
            code += '            scalar.ComputeCoefficients(scalarSpec);\n'
            if name == 'TPTSVF':
                code += '            scalarFromSnapshot.ComputeCoefficientsFromSnapshot(scalar.MakeSnapshot());\n'
            for f in fields:
                code += f'            assert(Raw(scalar.m_state.{member(f)}) == Raw(filter.m_state.{member(f)}[lane]));\n'
                if name == 'TPTSVF':
                    code += f'            assert(Raw(scalarFromSnapshot.m_state.{member(f)}) == Raw(scalar.m_state.{member(f)}));\n'
            for f in snapshot_oracle(name):
                code += f'            assert(Raw(scalar.MakeSnapshot().{member(f)}) == Raw(captured.{member(f)}[lane]));\n'
            code += '        }\n'
        if name == 'OnePoleTPTLowPass':
            code += '''        if constexpr (std::is_same_v<Number,float>)
        {
            OnePoleTPTLowPass32 scalar{};
            OnePoleTPTLowPass32::LogicalSpec scalarSpec{p[0]};
            scalar.ComputeCoefficients(scalarSpec);
            assert(Raw(scalar.m_state.m_G) == Raw(filter.m_state.m_G[lane]));
        }
'''
        code += '    }\n\n    std::cout << "\\n";\n}\n\n'

    code += 'int main()\n{\n    std::string name;\n    int precision, width;\n    while (std::cin >> name >> precision >> width)\n    {\n'
    for name in SCHEMAS:
        for precision in (32,64):
            for width in (1,2,3,4):
                code += f'''        if (name == "{name}" && precision == {precision} && width == {width})
        {{
            Test{name}<{"float" if precision == 32 else "double"},{width}>();
            continue;
        }}

'''
    code += '        return 2;\n    }\n\n    return 0;\n}\n'
    return code

def test_tuning(cases, compiler, output, root):
    source = output / 'tuning-runner.cpp'
    source.write_text(runner_source())
    runner = output / 'tuning-runner'
    subprocess.run(compiler + ['-std=c++17','-O2','-fno-fast-math','-ffp-contract=off',
        '-Wall','-Wextra','-Werror','-I',str(root/'build/lean/generated'),
        '-I',str(root/'private/src'),str(source),'-o',str(runner)],check=True)
    inputs, expected, labels = [], [], []
    for name, (spec, fields) in SCHEMAS.items():
        # A coefficient update must not even store a history field, including
        # an apparently harmless self-assignment that can quiet signaling NaNs.
        #
        header = (root/'build/lean/generated'/f'{name}Generic.hpp').read_text()
        method = header.split('void ComputeCoefficients(',1)[1].split('\n    }',1)[0]
        stores = set(re.findall(r'm_state\.(\w+(?:\.\w+)*) =', method))
        assert stores == {member(f) for f in coefficient_oracle(name)}, (name, stores)
        if name == 'TPTSVF':
            prepared_method = header.split('void ComputeCoefficientsFromSnapshot(',1)[1].split('\n    }',1)[0]
            prepared_stores = set(re.findall(r'm_state\.(\w+(?:\.\w+)*) =', prepared_method))
            assert prepared_stores == stores
            assert 'Math4096' not in prepared_method
        for precision in (32,64):
            rows = [c.split()[2:] for c in cases if c.startswith(f'tuning{precision} {name} ')]
            assert rows
            for width in (1,2,3,4):
                for start in range(0,len(rows),width):
                    lanes = rows[start:start+width]
                    lanes += [lanes[-1]]*(width-len(lanes))
                    inputs.append(f'{name} {precision} {width}')
                    count = len(spec)+len(fields)
                    inputs.append(' '.join(x for row in lanes for x in row[:count]))
                    expected.append([x for row in lanes for x in row[count:]])
                    labels.append((name,precision,width,start))
    result = subprocess.run([str(runner)],input='\n'.join(inputs)+'\n',
                            text=True,capture_output=True,timeout=60)
    actual = result.stdout.splitlines()
    assert result.returncode == 0, (result.stderr, labels[min(len(actual), len(labels)-1)])
    assert len(actual) == len(expected)
    max_error = {32:0.0,64:0.0}
    for label, line, wanted in zip(labels, actual, expected):
        found = line.split()
        assert len(found) == len(wanted)
        precision = label[1]
        fmt = ('I','f') if precision == 32 else ('Q','d')
        for got, ref in zip(found,wanted):
            if got == ref: continue
            a = struct.unpack(fmt[1],struct.pack(fmt[0],int(got)))[0]
            b = struct.unpack(fmt[1],struct.pack(fmt[0],int(ref)))[0]
            assert math.isfinite(a) and math.isfinite(b), (label,a,b)
            error = abs(a-b)/(1+abs(b))
            max_error[precision] = max(max_error[precision],error)
            assert error < 2e-5, (label,a,b,error)
    print(f'Tuning: {len(actual)} batches; all {len(SCHEMAS)} exports, both precisions, widths 1–4. '
          f'Math4096 bit checks, scalar agreement, snapshot capture, history, idempotence and sampled poles pass. '
          f'Max native-Lean error |delta|/(1+|reference|): {max_error}')
