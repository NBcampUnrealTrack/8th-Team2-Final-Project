"""Sequential, fresh-receipt v004 PIE verification. Does nothing without --execute."""
import argparse
import datetime
import json
import re
import sys
import time
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parents[1]))
from routes import ROUTE_ORDER, ROUTES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute', action='store_true', help='Run only after the root agent has saved the compact map and rebuilt/reloaded C++.')
    parser.add_argument('--reuse-pie', action='store_true', help='Use an already-running fresh PIE session instead of starting one.')
    parser.add_argument('--routes', nargs='*', choices=ROUTE_ORDER, help='Run a selected movement subset. States/archive/epilogue/camera still run unless --movement-only.')
    parser.add_argument('--movement-only', action='store_true', help='Useful for an isolated movement retest; its receipt is not a full-suite pass.')
    parser.add_argument('--run-id', help='Optional new receipt folder name; existing folders are rejected.')
    args = parser.parse_args()
    modes = ROUTE_ORDER if args.routes is None else args.routes
    if not args.execute:
        print('NOT EXECUTED. Save/reload v004 and rebuild C++ before invoking with --execute.')
        print(json.dumps(dict(routes=modes, total_waypoints=sum(len(ROUTES[m]) for m in modes),
                              receipts=str(ROOT/'receipts/<fresh-run-id>')), indent=2))
        return 0
    from call import call
    run_id = args.run_id or datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    assert re.fullmatch(r'[A-Za-z0-9_-]+', run_id), 'Unsafe run folder name'
    out = ROOT/'receipts'/run_id
    assert not out.exists(), 'Never overwrite or reuse a QA run directory'
    out.mkdir(parents=True)
    state = dict(passed=False, status='running', run_id=run_id, started=datetime.datetime.now().isoformat(),
                 full_suite=not args.movement_only and modes == ROUTE_ORDER, routes=[], completed_stages=[])

    def persist():
        (out/'runtime_suite.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
        (ROOT/'latest_run.json').write_text(json.dumps(dict(path=str(out), status=state['status'],
             passed=state['passed'], full_suite=state['full_suite']), indent=2), encoding='utf-8')

    def execute(code, receipt):
        path = out/(receipt+'_execution.json')
        assert not path.exists()
        result = call('execute_python_code', {'code': 'QA_OUT='+repr(str(out))+'\n'+code})
        path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        assert not isinstance(result, dict) or not result.get('error_message'), result
        if isinstance(result, dict):
            print(receipt, result.get('output', ''), flush=True)
        return result

    def stage(filename, stage_name, receipt):
        return execute('STAGE='+repr(stage_name)+'\n'+(ROOT/filename).read_text(encoding='utf-8'), receipt)

    def wait_receipt(filename, timeout=145):
        path = out/filename
        started = time.monotonic()
        next_notice = 0
        while not path.exists():
            elapsed = time.monotonic()-started
            assert elapsed < timeout, filename+' timed out; inspect the still-running PIE session'
            if elapsed >= next_notice:
                print('Waiting for', filename, round(elapsed), 'seconds', flush=True)
                next_notice = elapsed+10
            time.sleep(1)
        result = json.loads(path.read_text(encoding='utf-8'))
        assert result.get('passed') is True, result
        print(filename, 'PASS', flush=True)
        return result

    def checked(filename, stage_name, receipt, evidence):
        stage(filename, stage_name, receipt)
        result = wait_receipt(evidence, 10)
        state['completed_stages'].append(receipt)
        persist()
        return result

    persist()
    try:
        if args.reuse_pie:
            execute("import unreal\nassert unreal.WidgetService.is_pie_running()", 'pie_reused')
        else:
            execute("import unreal\nassert not unreal.WidgetService.is_pie_running()\nassert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Cindehearth'\nassert unreal.WidgetService.start_pie()", 'pie_start')
            time.sleep(3)
        if not args.movement_only:
            checked('qa_runtime.py', 'states', 'states', 'state_qa.json')
        for mode in modes:
            stage('qa_runtime.py', 'walk:'+mode, 'start_'+mode)
            result = wait_receipt('walk_'+mode+'.json')
            state['routes'].append(dict(route=mode, passed=True, waypoints=result['waypoints_reached']))
            persist()
        if not args.movement_only:
            # Adapt only the receipt destination, keeping the runtime agent's
            # exact five-case source and its original file intact.
            anchor_source = (ROOT.parent/'Runtime/qa_archive_anchors.py').read_text(encoding='utf-8')
            old = "OUT = Path(unreal.Paths.project_dir()) / 'Tools/Cindehearth/Dense_v004/Runtime'"
            assert anchor_source.count(old) == 1, 'Inspect changed runtime anchor QA before integrating it'
            execute(anchor_source.replace(old, 'OUT = Path(QA_OUT)'), 'archive_anchors')
            wait_receipt('archive_anchor_qa.json', 10)
            state['completed_stages'].append('archive_anchors')
            checked('qa_runtime.py', 'archive_enter', 'archive_enter', 'archive_enter.json')
            time.sleep(2)
            checked('qa_runtime.py', 'archive_lower_floor_exit', 'archive_lower_floor_exit', 'archive_lower_floor_exit.json')
            time.sleep(2)
            checked('qa_runtime.py', 'archive_upper_floor', 'archive_upper_floor', 'archive_upper_floor.json')
            begin = checked('qa_runtime.py', 'epilogue', 'epilogue_start', 'epilogue_start.json')
            remaining = float(begin['duration'])+3
            while remaining > 0:
                pause = min(5, remaining)
                time.sleep(pause)
                remaining -= pause
                print('Epilogue settling;', round(remaining, 1), 'seconds remaining', flush=True)
            checked('qa_runtime.py', 'epilogue_verify', 'epilogue_end', 'epilogue_end.json')
            stage('qa_camera.py', 'prepare', 'camera_prepare')
            time.sleep(2)
            checked('qa_camera.py', 'verify', 'camera_verify', 'player_camera.json')
        state['status'] = 'complete'
        state['passed'] = True
        state['finished'] = datetime.datetime.now().isoformat()
        persist()
        print('PASS', 'full suite' if state['full_suite'] else 'selected checks only', str(out), flush=True)
        print('PIE remains running for visual review; no editor-level state was saved by this runner.', flush=True)
        return 0
    except Exception:
        state['status'] = 'failed'
        state['error'] = traceback.format_exc()
        state['finished'] = datetime.datetime.now().isoformat()
        persist()
        print(state['error'], file=sys.stderr, flush=True)
        print('FAILED; inspect', str(out), 'and the current PIE state before any retest.', file=sys.stderr, flush=True)
        return 1


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    raise SystemExit(main())
