# MSM5232 VST3 (Core + CLI)

This project simulates MSM5232-like tones using a procedurally generated wavetable (based on note.md’s tanh curve and weights). It provides a lightweight synth core, a small CLI renderer, and an optional VST3 wrapper.

Language / 言語: [English](#english) | [日本語](#日本語)

---

## English

### Overview
MSM5232-inspired poly synth with anti-aliased, bandlimited wavetables and optional per-voice oversampling. Ships as a small cross‑platform core, a CLI offline renderer, and an optional VST3 plug‑in.

### Features
- 32-voice polyphony and ADSR envelope.
- Global tone selection (15 combinations of wav1 | wav2 | wav4 | wav8).
- Bandlimited wavetable mipmaps (1..8 tables per octave) with smooth crossfades.
- HQ oversampling per voice: Auto2x / Force2x / Force4x / Force8x.
- PreHighCut modes: Off / Fixed (~65% Nyquist, soft LP) / ByMaxNote.
- MIDI: Pitch Bend (±2 st), CC#1 Detune, CC#24 Depth, CC#25 Rate.
- CLI offline renderer (48 kHz, 24‑bit WAV). Optional VST3 plug‑in.

### Build (CLI)
```
cmake -S . -B build
cmake --build build -j
./build/cli/msm5232_render [toneMask 1..15]
```
Outputs `render.wav` (stereo, 48 kHz, 24‑bit PCM) rendering an 8‑note chord with a 2 s gate.

### Build (VST3, optional)
- Download Steinberg VST3 SDK and set `VST3_SDK_DIR` to its root (`pluginterfaces/` inside).
- Configure and build:
```
cmake -S . -B build -DBUILD_VST3=ON -DVST3_SDK_DIR=/path/to/VST3_SDK
cmake --build build -j
```
Produces `msm5232_vst3.vst3` under `build/vst3/` (or your generator’s default path).

### Submodules (VST3 SDK)
- Optional, for building the VST3 plug‑in within this repo:
```
git submodule add https://github.com/steinbergmedia/vst3sdk VST_SDK/vst3sdk
git submodule update --init --recursive
```
- With the submodule present, CMake auto‑detects it. You can still set `-DVST3_SDK_DIR=/your/path` to override.
- For reproducible builds, pin the submodule to a known tag/commit:
```
git -C VST_SDK/vst3sdk checkout <tag-or-commit>
git add VST_SDK/vst3sdk && git commit -m "Pin vst3sdk to <tag-or-commit>"
```

### Windows Example (single-file .vst3)
From Developer PowerShell for VS 2022:
```
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 \
  -DBUILD_VST3=ON -DVST3_SDK_DIR="%cd%/VST_SDK/vst3sdk" \
  -DSMTG_CREATE_BUNDLE_FOR_WINDOWS=OFF \
  -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF \
  -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF \
  -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF
cmake --build build-win --config Release --target msm5232_vst3
```
Output: `build-win/bin/Release/msm5232_vst3.vst3` (single file). If a `.dll` is produced, rename to `.vst3`.

### Parameters (VST3)
- Tone: 1..15 (wav1/2/4/8 combinations)
- Attack / Decay / Sustain / Release
- Gain
- TableSize: 64 / 128 / 256 (no interpolation; direct lookup)
- Quantize4bit: ON/OFF (≈4‑bit equivalent)
- Detune: −0.50 .. +0.50 st
- VibratoDepth: 0.00 .. 0.50 st
- VibratoRate: 0.00 .. 16.00 Hz
- Bandlimit: Off / 1/Oct .. 8/Oct
- HQMode: Off / Auto2x / Force2x / Force4x / Force8x
- PreHighCutMode: Off / Fixed / ByMaxNote
  - Fixed: soft LP around ~65% Nyquist (12‑bin taper)
  - ByMaxNote: computes safe maximum harmonic H from `PreHighCutMaxNote` and `VibratoDepth`
- PreHighCutMaxNote: 0..127 (default 64)

### Recommended Settings
- General: Bandlimit = 3/Oct or 4/Oct, HQMode = Auto2x.
- Very high/bright patches: PreHighCutMode = ByMaxNote and set `PreHighCutMaxNote` near the highest note (e.g., 64–84). If needed, HQMode = Force2x.

### Performance Notes
- Bandlimited tables are (re)built on parameter changes only; the audio callback never rebuilds them.
- 8/Oct roughly doubles BL tables vs. 4/Oct; memory remains small (~56 × 512 floats per waveform).
- HQMode Auto2x engages selectively; Force2x/4x/8x cost about 2×/4×/8× DSP for active voices.
- Oversampling decimator (OS=2/4/8) uses a small Hamming‑windowed‑sinc FIR (DC‑normalized, linear phase).

### NoiseAdd (extended)
- UI range 0..10000% (internally 0..100). Mapping of normalized 0..1:
  - 0..0.5 → 0..10% linear (fine)
  - 0.5..1 → 10%..10000% logarithmic (coarse)
- Effective signal: `s + d·|s|·noise` (d=0..100), scaled by `1/(1+d)` to avoid clipping.

### MIDI Control (VST3)
- Pitch Bend: ±2 semitones (center 0.5 normalized)
- CC#1 (Mod Wheel): Detune (−0.5..+0.5 st)
- CC#24: Vibrato Depth (0..0.5 st)
- CC#25: Vibrato Rate (0..16 Hz)
  - CC input mirrors to GUI parameters via `outputParameterChanges`.

### Key DSP Details
- Base waveform length: 512 samples/cycle.
- Effective table sizes: 64 / 128 / 256 (direct lookup, no interpolation)
  - 64:  x = −0.375   + 0.25·n
  - 128: x = −0.4375  + 0.125·n
  - 256: x = −0.46875 + 0.0625·n
- Quantization option: ≈4‑bit equivalent (normalized to [−1,1], rounded to ~1/7 steps, symmetric −7..7).
- Tone mix weights: wav1=1.0, wav2=0.6, wav4=0.5, wav8=0.45.

### Notes
- Processing is sample‑rate agnostic (CLI renders at 48 kHz; VST3 uses host rate).
- Output is stereo (dual‑mono) by default.
- For MSVC/Windows builds, sources compile with `/utf-8` to avoid codepage warnings.

### Compatibility
- Projects that had Bandlimit at max (previously 4/Oct) now map the top step to 8/Oct. Likewise, HQMode’s top step maps to Force8x. If you need prior CPU profile, select 4/Oct and/or Force2x manually after loading.

### Layout
- `src/dsp/`: wavetable generation, ADSR, voices, synth
- `src/app/`: CLI offline renderer
- `src/vst3/`: minimal VST3 processor/controller (compiled only when enabled)

### License
- The source code in this repository is licensed under the terms in `LICENSE`.
- The Steinberg VST 3 SDK is licensed separately by Steinberg/Yamaha. If you add it as a submodule under `VST_SDK/vst3sdk`, you do so under the SDK’s license; read the SDK’s `LICENSE.txt` for redistribution/usage terms.

---

## 日本語

### 概要
MSM5232 に着想を得たポリ・シンセ。エイリアスを抑制する帯域制限ウェーブテーブルと、必要に応じたボイス単位のオーバーサンプリングを備えます。軽量コア／CLI オフラインレンダラ／任意の VST3 プラグインを提供します。

### 特長
- 32ボイス・ポリフォニック、ADSR エンベロープ。
- グローバルトーン（wav1 | wav2 | wav4 | wav8 の 15 組み合わせ）。
- 帯域制限ウェーブテーブルのミップマップ（1..8 テーブル/オクターブ）。
- HQ オーバーサンプリング：Auto2x / Force2x / Force4x / Force8x。
- PreHighCut モード：Off / Fixed（Nyquist の約 65% でソフト LP）/ ByMaxNote。
- MIDI：ピッチベンド（±2 半音）、CC#1 Detune、CC#24 Depth、CC#25 Rate。
- CLI オフラインレンダ（48 kHz / 24-bit WAV）。VST3 は任意。

### ビルド（CLI）
```
cmake -S . -B build
cmake --build build -j
./build/cli/msm5232_render [toneMask 1..15]
```
8 音の和音（ゲート 2 秒）を `render.wav`（ステレオ、48 kHz、24‑bit PCM）に出力します。

### ビルド（VST3, 任意）
- Steinberg VST3 SDK を取得し、`VST3_SDK_DIR` をそのルート（`pluginterfaces/` を含む）に設定します。
- 構成とビルド：
```
cmake -S . -B build -DBUILD_VST3=ON -DVST3_SDK_DIR=/path/to/VST3_SDK
cmake --build build -j
```
`build/vst3/`（または使用ジェネレータ既定の場所）に `msm5232_vst3.vst3` が生成されます。

### サブモジュール（VST3 SDK）
- 本リポ内で VST3 をビルドしたい場合の任意設定：
```
git submodule add https://github.com/steinbergmedia/vst3sdk VST_SDK/vst3sdk
git submodule update --init --recursive
```
- サブモジュールが存在すれば CMake が自動検出します。明示的に指定したい場合は `-DVST3_SDK_DIR=/your/path` を使用してください。
- 再現性のため、既知のタグ/コミットに固定（ピン留め）することを推奨：
```
git -C VST_SDK/vst3sdk checkout <tag-or-commit>
git add VST_SDK/vst3sdk && git commit -m "Pin vst3sdk to <tag-or-commit>"
```

### Windows 例（単一ファイル .vst3）
Developer PowerShell for VS 2022 から：
```
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 \
  -DBUILD_VST3=ON -DVST3_SDK_DIR="%cd%/VST_SDK/vst3sdk" \
  -DSMTG_CREATE_BUNDLE_FOR_WINDOWS=OFF \
  -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF \
  -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF \
  -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF
cmake --build build-win --config Release --target msm5232_vst3
```
出力：`build-win/bin/Release/msm5232_vst3.vst3`（単一ファイル）。`.dll` ができた場合は `.vst3` に改名してください。

### パラメータ（VST3）
- Tone：1..15（wav1/2/4/8 の組み合わせ）
- Attack / Decay / Sustain / Release
- Gain
- TableSize：64 / 128 / 256（補間なしの直接参照）
- Quantize4bit：ON/OFF（≈4ビット相当）
- Detune：−0.50 .. +0.50 半音
- VibratoDepth：0.00 .. 0.50 半音
- VibratoRate：0.00 .. 16.00 Hz
- Bandlimit：Off / 1/Oct .. 8/Oct
- HQMode：Off / Auto2x / Force2x / Force4x / Force8x
- PreHighCutMode：Off / Fixed / ByMaxNote
  - Fixed：Nyquist の約 65% にソフト LP（12bin テーパー）
  - ByMaxNote：`PreHighCutMaxNote` と `VibratoDepth` から安全な最大倍音数 H を算出
- PreHighCutMaxNote：0..127（既定 64）

### 推奨設定
- 一般用途：Bandlimit = 3/Oct または 4/Oct、HQMode = Auto2x。
- 超高域・明るい音色：PreHighCutMode = ByMaxNote、`PreHighCutMaxNote` を実際の最高音付近（例：64–84）に設定。必要なら HQMode = Force2x。

### パフォーマンスメモ
- 帯域制限テーブルの構築はパラメータ変更時のみ。オーディオコールバックでは再構築しません。
- 8/Oct は 4/Oct の約 2 倍のテーブル数ですが、メモリは小規模（波形あたり ≈56 × 512 float）。
- HQMode の Auto2x は選択的に動作。Force2x/4x/8x は有効ボイスでそれぞれ約 2×/4×/8× の負荷。
- デシメータ（OS=2/4/8）は小さなハミング窓シンク FIR（直線位相、DC 正規化）。

### NoiseAdd（拡張）
- UI 表示 0..10000%（内部 0..100）。正規化 0..1 のマッピング：
  - 0..0.5 → 0..10% を線形（細かい制御）
  - 0.5..1 → 10%..10000% を対数（大まかな制御）
- 実効式：`s + d·|s|·noise`（d=0..100）を `1/(1+d)` でスケーリングしクリップを回避。

### MIDI コントロール（VST3）
- ピッチベンド：±2 半音（ノーマライズ中心 0.5）
- CC#1（モジュレーションホイール）：Detune（−0.5..+0.5 半音）
- CC#24：Vibrato Depth（0..0.5 半音）
- CC#25：Vibrato Rate（0..16 Hz）
  - `outputParameterChanges` により GUI パラメータへ反映。

### 主要 DSP 仕様
- 基本波形長：512 サンプル/周期。
- 有効テーブルサイズ：64 / 128 / 256（補間なしの直接参照）
  - 64：  x = −0.375   + 0.25·n
  - 128： x = −0.4375  + 0.125·n
  - 256： x = −0.46875 + 0.0625·n
- 量子化オプション：≈4ビット相当（[−1,1] に正規化後、約 1/7 刻み、対称 −7..7）。
- トーン混合比：wav1=1.0, wav2=0.6, wav4=0.5, wav8=0.45。

### 備考
- サンプルレート非依存（CLI は 48 kHz 固定、VST3 はホストに追従）。
- 既定はステレオ（デュアルモノ）。
- MSVC/Windows では `/utf-8` でコンパイルして警告を回避。

### 互換性について
- 以前の最大設定（4/Oct）を使用していたプロジェクトは、最大ステップが 8/Oct に対応します。HQMode の最大も Force8x に対応。過去の CPU プロファイルを維持したい場合は、読み込み後に 4/Oct や Force2x を手動選択してください。

### ディレクトリ構成
- `src/dsp/`：ウェーブテーブル生成、ADSR、ボイス、シンセ
- `src/app/`：CLI オフラインレンダラ
- `src/vst3/`：最小限の VST3 プロセッサ／コントローラ（有効化時のみ）

### ライセンス
- 本リポジトリのソースコードは `LICENSE` の条件に従います。
- Steinberg VST 3 SDK は別ライセンスです。`VST_SDK/vst3sdk` にサブモジュールとして追加する場合は、SDK 同梱の `LICENSE.txt` を必ず確認し、その条件に従ってください（再配布条件等）。

---

## Changelog / 変更履歴

### v2.3
- EN: Oversampling decimator upgraded from box average to a small Hamming‑windowed‑sinc FIR (DC‑normalized, linear phase) for OS=2/4/8; cleaner highs and reduced fold‑back.
- JP: オーバーサンプリングのデシメータを「箱平均」から小さなハミング窓シンク FIR（直線位相、DC 正規化）へ。高域がよりクリーンになり、折り返しを低減。

### v2.2
- EN: Bandlimit density extended to 8/Oct (1..8 tables/octave) for smoother high‑pitch motion; HQMode adds Force4x/Force8x.
- JP: 帯域制限の密度を 8/Oct まで拡張（高音域の変化でのにじみを低減）。HQMode に Force4x/Force8x を追加。

### v2.1
- EN: Added PreHighCutMode (Off / Fixed / ByMaxNote). Fixed = soft LP ~65% Nyquist with 12‑bin taper. ByMaxNote computes safe maximum harmonic H. Default `PreHighCutMaxNote` = 64.
- JP: PreHighCutMode（Off / Fixed / ByMaxNote）を追加。Fixed は Nyquist 約 65% のソフト LP（12bin テーパー）。ByMaxNote は安全な最大倍音数 H を算出。`PreHighCutMaxNote` 既定は 64。

### v2.0
- EN: Fixed Force2x pitch‑doubling bug; refined Auto2x engagement near highest band/harmonic limit.
- JP: Force2x で音程が倍化する不具合を修正。最高帯域付近や倍音限界近傍での Auto2x の動作条件を調整。

### v1.9 (HQ mode)
- EN: Added HQMode with Auto2x / Force2x. Auto2x engages per‑voice near the top band; Force2x always on. Implementation uses two half‑phase steps per sample.
- JP: HQMode を追加（Auto2x / Force2x）。Auto2x は高域帯で必要なボイスのみ動作。Force2x は常時。各サンプルを 2 つの半位相ステップに分割。

### v1.8
- EN: Soft spectral taper near cutoff (~6 bins) to reduce leakage and vibrato‑sideband alias; conservative table selection with vibrato guard; added 4/Oct quality.
- JP: カットオフ近傍にソフトなスペクトルテーパー（約 6 ビン）で漏れとビブラート側帯のエイリアスを低減。ビブラートを見越した保守的なテーブル選択。4/Oct 品質を追加。

### v1.5
- EN: Bandlimited wavetable mipmaps with selectable quality (Off / 1..4 per octave initially); crossfade between tables by f0 to keep harmonics under Nyquist and remove high‑pitch aliasing.
- JP: 帯域制限ウェーブテーブルのミップマップ（当初 Off / 1..4/Oct）。f0 に応じて隣接テーブルをクロスフェードし、倍音を Nyquist 未満に保って高音域のエイリアスを解消。

### v1.4
- EN: Pitch Bend (±2 st); Detune parameter (CC#1); Vibrato Depth (CC#24); Vibrato Rate (CC#25) with real‑time GUI updates.
- JP: ピッチベンド（±2 半音）、Detune（CC#1）、Vibrato Depth（CC#24）、Vibrato Rate（CC#25）を追加。GUI をリアルタイム更新。

---

### Limitations
- No custom GUI (host generic editor is used).
