#!/usr/bin/env python3
"""Measure AI strength using real headless fights, not mocked combat or replays.

Example:
  python3 port/tests/battle/ai_strength.py --binary port/build_release/zenkaipower_64 \
      --data release/linux-x64/gamedata
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
PREFIX = 'AI_BENCH_RESULT '


def sign_tail(wins, losses):
    """Exact one-sided sign-test p, treating an entire side-swapped pair as a unit."""
    n = wins + losses
    return sum(math.comb(n, k) for k in range(wins, n + 1)) / 2**n if n else 1.0


def summarize(matches, candidate, baseline, alpha):
    grouped = {}
    wins = losses = draws = 0
    margins = []
    for match in matches:
        side = match['candidate_side']
        winner = match['winner_flags'] & 7
        score = 0.5 if winner == 4 else float(winner == (1 << side))
        wins += score == 1
        losses += score == 0
        draws += score == 0.5
        key = (match['characters'][0], match['stage'], match['seed'])
        if side in grouped.setdefault(key, {}):
            raise ValueError('Duplicate side in a paired trial')
        grouped[key][side] = score
        margins.append(match['health'][side] - match['health'][1 - side])
    if not grouped or any(set(pair) != {0, 1} for pair in grouped.values()):
        raise ValueError('Every trial must have both side-swapped matches')
    scores = [sum(pair.values()) / 2 for pair in grouped.values()]
    positive = sum(score > 0.5 for score in scores)
    negative = sum(score < 0.5 for score in scores)
    mean = sum(scores) / len(scores)
    up, down = sign_tail(positive, negative), sign_tail(negative, positive)
    verdict = 'inconclusive'
    if mean > 0.5 and up <= alpha:
        verdict = 'improved'
    elif mean < 0.5 and down <= alpha:
        verdict = 'regressed'
    return dict(candidate=candidate, baseline=baseline, verdict=verdict,
                wins=wins, losses=losses, draws=draws, score=mean,
                pairs=len(scores), favorable_pairs=positive, unfavorable_pairs=negative,
                tied_pairs=len(scores)-positive-negative, improvement_p=up, regression_p=down,
                alpha_per_direction=alpha, mean_health_margin=sum(margins)/len(margins),
                knockouts=sum(bool(m['reason'] & 1) for m in matches),
                time_limits=sum(bool(m['reason'] & 2) for m in matches),
                mean_ticks=sum(m.get('ticks', 0) for m in matches)/len(matches))


def run_match(binary, data, config, timeout):
    chara, level0, level1, stage, seed, seconds = config
    env = {key: value for key, value in os.environ.items() if not key.startswith('ZENKAIPOWER_')}
    with tempfile.TemporaryDirectory(prefix='ai-strength-save-') as saves:
        env.update(ZENKAIPOWER_AI_BENCH=f'{chara},{chara},{level0},{level1},{stage},{seed},{seconds}',
                   ZENKAIPOWER_GS='none', ZENKAIPOWER_DATA=str(data), ZENKAIPOWER_SAVES=saves)
        try:
            process = subprocess.run([str(binary)], cwd=ROOT, env=env, capture_output=True,
                                     text=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f'Fight timed out: {config}') from error
    lines = [line[len(PREFIX):] for line in process.stdout.splitlines() if line.startswith(PREFIX)]
    if process.returncode or len(lines) != 1:
        raise RuntimeError(f'Invalid fight {config}, exit {process.returncode}:\n'
                           f'{process.stdout[-1500:]}\n{process.stderr[-1500:]}')
    result = json.loads(lines[0])
    if not all(max_hp > 0 and 0 <= hp <= max_hp for hp, max_hp in zip(result['hp'], result['max_hp'])):
        raise RuntimeError(f'Invalid health measurements: {result}')
    result['health'] = [100 * hp / max_hp for hp, max_hp in zip(result['hp'], result['max_hp'])]
    expected = dict(characters=[chara, chara], levels=[level0, level1], stage=stage,
                    seed=seed, seconds=seconds)
    if any(result.get(key) != value for key, value in expected.items()):
        raise RuntimeError(f'Fight fixture mismatch: {result}')
    if (result['winner_flags'] & 7) not in (1, 2, 4) or not result['reason'] & 3:
        raise RuntimeError(f'Fight was not completed normally: {result}')
    if result['native_winner_flags'] & 7 != result['winner_flags']:
        raise RuntimeError(f'Live health and the native game winner disagree: {result}')
    if (result['ticks'] <= 0 or not all(math.isfinite(hp) for hp in result['health'])
        or (result['reason'] & 2 and result['ticks'] < seconds * 30)):
        raise RuntimeError(f'Invalid fight measurements: {result}')
    return result


def write_report(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(report, indent=2) + '\n')
    temporary.replace(path)


def summary_table(report):
    headers = ('Levels', 'W', 'L', 'D', 'Score', 'HP lead', 'p(improve)', 'Verdict')
    rows = [(f"{s['candidate']} vs {s['baseline']}", str(s['wins']), str(s['losses']),
             str(s['draws']), f"{s['score']:.1%}", f"{s['mean_health_margin']:+.1f} pp",
             f"{s['improvement_p']:.3g}", s['verdict'].capitalize()) for s in report['summary']]
    return headers, rows


def print_summary_table(report):
    headers, rows = summary_table(report)
    widths = [max(len(row[i]) for row in [headers, *rows]) for i in range(len(headers))]
    def line(row):
        return ' | '.join(cell.ljust(widths[i]) if i in (0, 7) else cell.rjust(widths[i])
                          for i, cell in enumerate(row))
    print(line(headers))
    print('-+-'.join('-' * width for width in widths))
    for row in rows:
        print(line(row))
    print(f"Score: win=1, draw=0.5. HP lead: percentage points. "
          f"Corrected per-direction threshold: {report['alpha_per_direction']:.3g}.")


def write_summary_csv(report, path):
    fields = ('candidate', 'baseline', 'wins', 'losses', 'draws', 'score',
              'mean_health_margin', 'improvement_p', 'regression_p', 'alpha_per_direction', 'verdict')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=fields, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(report['summary'])


def plotting_library():
    try:
        import matplotlib
        matplotlib.use('Agg')  # Export files without a window/display server.
        import matplotlib.pyplot as plt
        return plt
    except ImportError as error:
        raise RuntimeError('Plotting requires matplotlib; install it in your Python environment '
                           '(python -m pip install matplotlib). Combat tests do not require it.') from error


def completed_report(report):
    if report.get('status') != 'complete' or not report.get('summary'):
        raise ValueError('A complete benchmark report is required')
    # Derive every displayed/exported table from the same completed trials.
    normalized = dict(report)
    normalized['summary'] = [summarize([m for m in report['matches'] if
                                       (m['candidate'], m['baseline']) == (candidate, baseline)],
                                      candidate, baseline, report['alpha_per_direction'])
                             for candidate, baseline in report['comparisons']]
    return normalized


def plot_results(report, prefix):
    """Descriptive charts and the exact verdict table; no independence-based error bars."""
    plt = plotting_library()
    report = completed_report(report)
    summaries = report['summary']
    headers, rows = summary_table(report)
    prefix = Path(prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    colors = dict(win='#13866e', draw='#c5cbd4', loss='#d97762')
    with plt.rc_context({'font.family': 'DejaVu Sans', 'font.size': 11,
                         'axes.spines.top': False, 'axes.spines.right': False,
                         'axes.titleweight': 'bold', 'svg.fonttype': 'none'}):
        fig = plt.figure(figsize=(14, max(7.8, 4.2 + len(rows)*.62)), facecolor='#f6f8fb')
        grid = fig.add_gridspec(2, 2, height_ratios=(2.3, 1.4), hspace=.4, wspace=.32,
                               left=.10, right=.97, top=.80, bottom=.13)
        outcomes, health, table_ax = fig.add_subplot(grid[0, 0]), fig.add_subplot(grid[0, 1]), fig.add_subplot(grid[1, :])
        positions = list(range(len(summaries)))
        labels = [f"{s['candidate']} vs {s['baseline']}" for s in summaries]
        totals = [s['wins'] + s['losses'] + s['draws'] for s in summaries]
        left = [0.0] * len(summaries)
        for field, label, color in [('wins', 'Win', colors['win']), ('draws', 'Draw', colors['draw']),
                                    ('losses', 'Loss', colors['loss'])]:
            values = [100*s[field]/total for s, total in zip(summaries, totals)]
            outcomes.barh(positions, values, left=left, color=color, height=.65, label=label)
            if field == 'wins':
                for y, value in enumerate(values):
                    if value >= 15:
                        outcomes.text(value/2, y, f'{value:.1f}%', ha='center', va='center',
                                      color='white', weight='bold')
            left = [a+b for a, b in zip(left, values)]
        outcomes.axvline(50, color='#46556b', linestyle=':', linewidth=1)
        outcomes.set(yticks=positions, yticklabels=labels, xlim=(0, 100),
                     xlabel='Percentage of fights', title='New AI outcomes against original levels')
        outcomes.invert_yaxis()
        outcomes.legend(loc='upper left', bbox_to_anchor=(0, -.19), ncol=3, frameon=False)
        margins = [s['mean_health_margin'] for s in summaries]
        health.barh(positions, margins, color=[colors['win'] if x >= 0 else colors['loss'] for x in margins], height=.65)
        health.axvline(0, color='#46556b', linewidth=1)
        for y, margin in enumerate(margins):
            health.text(margin + (2 if margin >= 0 else -2), y, f'{margin:+.1f}',
                        ha='left' if margin >= 0 else 'right', va='center')
        health.set(yticks=positions, yticklabels=labels, xlim=(-115, 115),
                   xticks=(-100, -50, 0, 50, 100), xlabel='Remaining-health lead (percentage points)',
                   title='Mean health advantage of the new AI')
        health.invert_yaxis()
        table_ax.axis('off')
        table = table_ax.table(cellText=rows, colLabels=headers, cellLoc='center', loc='center',
                               colWidths=[.13, .065, .065, .065, .11, .15, .15, .19])
        table.auto_set_font_size(False)
        table.set_fontsize(11)
        table.scale(1, 1.65)
        verdict_colors = dict(improved='#08745f', regressed='#b34036', inconclusive='#946c18')
        for (row, column), cell in table.get_celld().items():
            cell.set_edgecolor('#dce2eb')
            cell.set_facecolor('#e4eaf2' if row == 0 else '#ffffff' if row % 2 else '#edf2f7')
            if row == 0:
                cell.set_text_props(weight='bold', color='#24354c')
            elif column == 7:
                cell.set_text_props(weight='bold', color=verdict_colors[summaries[row-1]['verdict']])
        fig.suptitle('ZENKAI AI — combat benchmark', x=.10, y=.96, ha='left', fontsize=23, weight='bold', color='#24354c')
        fig.text(.10, .895, f"{sum(totals)} scored fights  •  {len(report['characters'])} mirror-match characters  •  "
                 f"{len(report['stages'])} stage(s)  •  up to {report['seconds']} seconds of game time",
                 fontsize=12, color='#46556b')
        fig.text(.10, .052, f"Sides swapped for each seed. Verdicts: paired sign test with multiple-comparison correction "
                 f"(threshold {report['alpha_per_direction']:.3g}).\n"
                 "Score = wins + half of draws, divided by fights. Results apply to the tested scenarios.",
                 fontsize=10, color='#46556b', linespacing=1.6)
        paths = [Path(str(prefix) + extension) for extension in ('.png', '.svg')]
        for path in paths:
            fig.savefig(path, dpi=170, facecolor=fig.get_facecolor())
        plt.close(fig)
    csv_path = Path(str(prefix) + '_summary.csv')
    write_summary_csv(report, csv_path)
    for path in [*paths, csv_path]:
        print(f'Exported: {path}')


def self_test():
    def fixture(outcomes):
        matches = []
        for seed, outcome in enumerate(outcomes):
            for side in (0, 1):
                winner = 4 if outcome == 'draw' else 1 << (side if outcome == 'win' else 1-side)
                matches.append(dict(candidate_side=side, winner_flags=winner,
                                    characters=[0, 0], stage=0, seed=seed, health=[50, 50],
                                    reason=2, ticks=2700))
        return matches
    assert summarize(fixture(['win']*12), 49, 29, .005)['verdict'] == 'improved'
    assert summarize(fixture(['loss']*12), 49, 29, .005)['verdict'] == 'regressed'
    assert summarize(fixture(['draw']*12), 49, 29, .005)['verdict'] == 'inconclusive'
    assert summarize(fixture(['win']), 49, 29, .005)['verdict'] == 'inconclusive'
    assert summarize(fixture(['win', 'loss']*6), 49, 29, .005)['verdict'] == 'inconclusive'
    # A side-0 advantage must not masquerade as a stronger candidate.
    biased = fixture(['win']*12)
    for match in biased:
        match['winner_flags'] = 1
    assert summarize(biased, 49, 29, .005)['verdict'] == 'inconclusive'
    try:
        summarize(biased[:-1], 49, 29, .005)
    except ValueError:
        pass
    else:
        raise AssertionError('An incomplete pair was accepted')
    print('Statistical verdict checks passed (improvement, regression, ties, small samples, side bias).')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--binary', type=Path, default=ROOT / 'port/build_release/zenkaipower_64')
    parser.add_argument('--data', type=Path, default=ROOT / 'release/linux-x64/gamedata')
    parser.add_argument('--comparisons', default='20:0,26:6,33:13,41:21,49:29',
                        help='candidate:baseline effective CPU levels')
    parser.add_argument('--characters', default='0,5,30', help='mirror-match character IDs')
    parser.add_argument('--stages', default='0')
    parser.add_argument('--seeds', type=int, default=12, help='number of seeds, starting at 1001')
    parser.add_argument('--seconds', type=int, choices=(60, 90, 180, 240), default=90)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--timeout', type=float, default=60)
    parser.add_argument('--alpha', type=float, default=.05)
    parser.add_argument('--report', type=Path, default=ROOT / 'port/build/ai_strength.json')
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--plot', action='store_true', help='export PNG/SVG plots and a summary CSV after the benchmark')
    parser.add_argument('--plot-from', type=Path, metavar='REPORT', help='plot a saved JSON report without running fights')
    parser.add_argument('--plot-output', type=Path, metavar='PREFIX', help='output file prefix; defaults to the report path without .json')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.plot_output and not (args.plot or args.plot_from):
        parser.error('--plot-output requires --plot or --plot-from')
    if args.plot_from:
        try:
            report = completed_report(json.loads(args.plot_from.read_text()))
            print_summary_table(report)
            plot_results(report, args.plot_output or args.plot_from.with_suffix(''))
        except (OSError, ValueError, KeyError, RuntimeError) as error:
            parser.exit(3, f'{error}\n')
        return
    if args.plot:
        try:
            plotting_library()  # Check before spending time running fights.
        except RuntimeError as error:
            parser.exit(3, f'{error}\n')
    comparisons = [tuple(map(int, pair.split(':'))) for pair in args.comparisons.split(',')]
    characters = list(map(int, args.characters.split(',')))
    stages = list(map(int, args.stages.split(',')))
    if (not comparisons or any(len(pair) != 2 or not all(0 <= x <= 49 for x in pair) for pair in comparisons)
        or len(set(comparisons)) != len(comparisons) or len(set(characters)) != len(characters)
        or len(set(stages)) != len(stages) or any(not 0 <= x < 161 for x in characters)
        or any(not 0 <= x < 35 for x in stages) or args.seeds < 1 or args.jobs < 1
        or args.timeout <= 0 or not 0 < args.alpha < 1):
        parser.error('Invalid levels, characters, stages, seeds, jobs, timeout or alpha')
    binary, data = args.binary.resolve(), args.data.resolve()
    if not binary.is_file() or not data.is_dir():
        parser.error('A built game binary and extracted game data are required')
    report = dict(status='running', binary=str(binary), data=str(data),
                  binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  comparisons=comparisons, characters=characters, stages=stages, seeds=args.seeds,
                  seconds=args.seconds, alpha=args.alpha, matches=[], controls=[],
                  metric='Native game winner, checked against live normalized HP at combat end',
                  statistical_test='Paired sign test; one side-swapped pair per character/stage/seed; '
                                   'Bonferroni correction over comparisons and both directions')
    # Bonferroni covers all comparisons and both possible directional verdicts.
    alpha = args.alpha / (2 * len(comparisons))
    report['alpha_per_direction'] = alpha
    start = time.monotonic()
    write_report(args.report, report)
    try:
        probe = (characters[0], comparisons[-1][0], comparisons[-1][1], stages[0], 1001, args.seconds)
        first = run_match(binary, data, probe, args.timeout)
        second = run_match(binary, data, probe, args.timeout)
        report['repeatability'] = dict(first=first, second=second, equal=first == second)
        if first != second:
            raise RuntimeError('Identical seeded fixtures produced different results; benchmark reproducibility failed')
        print('Seeded fight repeatability verified.', flush=True)
        # Equal-level mirrors reveal any positional advantage in the fixture.
        for chara in characters:
            control = (chara, 29, 29, stages[0], 1001, args.seconds)
            report['controls'].append(run_match(binary, data, control, args.timeout))
        tasks = []
        for candidate, baseline in comparisons:
            for char_index, chara in enumerate(characters):
                for stage_index, stage in enumerate(stages):
                    seed_start = 1001 + (char_index * len(stages) + stage_index) * args.seeds
                    for seed in range(seed_start, seed_start + args.seeds):
                        for side in (0, 1):
                            levels = (candidate, baseline) if side == 0 else (baseline, candidate)
                            tasks.append(((chara, *levels, stage, seed, args.seconds), candidate, baseline, side))
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {pool.submit(run_match, binary, data, config, args.timeout): (candidate, baseline, side)
                       for config, candidate, baseline, side in tasks}
            for future in as_completed(futures):
                candidate, baseline, side = futures[future]
                try:
                    result = future.result()
                except Exception:
                    for pending in futures:
                        pending.cancel()
                    raise
                result.update(candidate=candidate, baseline=baseline, candidate_side=side)
                report['matches'].append(result)
                done = len(report['matches'])
                if done % 12 == 0 or done == len(tasks):
                    print(f'{done}/{len(tasks)} fights completed ({time.monotonic()-start:.0f}s)', flush=True)
                    write_report(args.report, report)
        report['summary'] = [summarize([m for m in report['matches'] if
                                       (m['candidate'], m['baseline']) == (candidate, baseline)],
                                      candidate, baseline, alpha) for candidate, baseline in comparisons]
        report['status'] = 'complete'
        report['elapsed_seconds'] = time.monotonic() - start
        write_report(args.report, report)
        print_summary_table(report)
        print(f'Report: {args.report}')
    except Exception as error:
        report.update(status='invalid', error=str(error), elapsed_seconds=time.monotonic()-start)
        write_report(args.report, report)
        print(str(error), file=sys.stderr)
        raise SystemExit(3) from error
    if args.plot:
        try:
            plot_results(report, args.plot_output or args.report.with_suffix(''))
        except (OSError, ValueError, RuntimeError) as error:
            parser.exit(3, f'Benchmark report saved, but plotting failed: {error}\n')
    # Inconclusive results are explicitly not a pass. 2 denotes measured regression.
    verdicts = [result['verdict'] for result in report['summary']]
    raise SystemExit(2 if 'regressed' in verdicts else 0 if all(v == 'improved' for v in verdicts) else 1)


if __name__ == '__main__':
    main()
