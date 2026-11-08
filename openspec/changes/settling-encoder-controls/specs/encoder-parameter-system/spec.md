## MODIFIED Requirements

### Requirement: Interpolated Per-Sample Output Caches with Source-Side Smoothing
The matrix SHALL write root outputs through `SetOutput(output, hostSample)`, stamped with `SampleTimer::GetHostSample()`, preserving the output from before the first write of each host frame in `m_outputPrev`. Every host pass SHALL publish `x_oversample` caches. For an output written in the current host frame, slot `s` SHALL contain `ToValue(m_output + (m_outputPrev - m_output) × (x_oversample - 1 - s) / x_oversample)`. Other outputs SHALL publish their latest value in every slot. Thus interpolation reaches the latest output on the last engine sample, and ratio 1 publishes the latest output directly.

`SmartGridOneEncoders::Process` SHALL select the current sub-sample cache before DSP parameter reads. Its `GetValue` overloads SHALL read that cache, mapping Voice parameters to trio `voice / 3` and local channel `voice % 3`, with direct channels for Quad and Global parameters. The cell-level `GetValue(channel)` helper SHALL provide equivalent interpolation for direct callers. Bipolar conversion SHALL occur once at publication or direct read. Matrix calculations and UI publication SHALL use latest values without output interpolation; deferred visible child values SHALL be synchronized before UI publication.

Smoothing SHALL happen where values enter the encoder system:
- Stored edits: `SetTarget` SHALL retain a per-scene target and preserve the ongoing filter state. Moving scene values SHALL advance at host rate through a one-pole with a 20 ms time constant, using `alpha = 1-exp(-1/(0.020*hostSampleRate))`. Internal filter state SHALL use double precision. After a filter step, a normalized residual at most `1e-6` SHALL snap both the internal and published scene value exactly to the target. New edits SHALL accumulate against targets without changing the coefficient or restarting filter state. Patch JSON SHALL save targets. Settled scene values SHALL perform no further pole arithmetic; after the final output/dependency refresh, a tree with no moving descendants SHALL return to the cached evaluation path. Live modulation SHALL bypass this control filter.
- Scene blend and faders: blend and fader parameter-set messages SHALL set `SettlingSlew` targets that `ProcessInputSlews` advances every sample with a 500 Hz one-pole low-pass, before the scene manager and encoders run, stopping once the value settles on its target. Restoring a blend factor or saved fader positions SHALL snap the corresponding slew to the restored value.

#### Scenario: Encoder edit follows a fixed response
- **WHEN** a settled scene changes from 0.25 to 0.35
- **THEN** after 20 ms its value is approximately `0.35 - 0.10/e`
- **AND** it lands exactly on 0.35 once the residual reaches the settling threshold

#### Scenario: Burst edits preserve filter state
- **WHEN** a small correction arrives 1 ms after a larger edit
- **THEN** the target incorporates the correction and the filter keeps its current state and fixed coefficient
- **AND** the correction does not compress unfinished movement into a new linear ramp

#### Scenario: Settled controls stop smoothing
- **WHEN** all moving values in a control tree reach their targets
- **THEN** the final values are published and the tree becomes eligible for cached evaluation
- **AND** subsequent host frames perform no pole arithmetic for those settled values

#### Scenario: Modulation retains its bandwidth
- **WHEN** an audio-rate source modulates a parameter with settled manual controls
- **THEN** the source continues through the existing matrix and output interpolation without the manual-control one-pole

#### Scenario: DSP reads follow the computed output
- **WHEN** a cell's output changes during a matrix pass at ratio 1
- **THEN** a `GetValue` call later in the same engine sample returns the new output, without an exponential approach from the previous output

#### Scenario: DSP reads interpolate across a host frame
- **WHEN** the oversampling ratio is 4 and a matrix pass moves a cell's output from 0.2 to 1.0
- **THEN** `GetValue` returns 0.4, 0.6, 0.8, and 1.0 on the four engine samples of that host frame

