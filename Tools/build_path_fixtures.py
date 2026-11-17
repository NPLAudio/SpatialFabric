"""Write Fixtures/path-v1.json.

Expected positions below are hand-computed from Specs/Path-v1.md (not produced by the
reference implementation), so implementations are tested against independent numbers.
"""
import copy
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def document(keys, **track):
    base = {'id': 't', 'target': {'kind': 'object', 'key': 'fixture/object'}, 'keyframes': keys}
    base.update(track)
    return {'schema': 'org.nplaudio.spatialfabric.path', 'schema_version': 1, 'space': 'canonical', 'tracks': [base]}


def key(t, position, **extra):
    return dict(t=t, position=position, **extra)


def case(name, doc, samples, note):
    return {'name': name, 'note': note, 'document': doc,
            'samples': [{'track': 't', 'time': t, 'position': p, **({'parameters': q} if q else {})} for t, p, q in samples]}


cases = [
    case('linear_midpoint', document([key(0, [0, 0, 0]), key(2, [2, 4, 0])]),
         [(0, [0, 0, 0], None), (0.5, [0.5, 1, 0], None), (1, [1, 2, 0], None), (2, [2, 4, 0], None), (9, [2, 4, 0], None), (-1, [0, 0, 0], None)],
         'Linear interpolation; clamped before the first and after the last keyframe.'),
    case('hold_steps', document([key(0, [0, 0, 0], interp='hold'), key(1, [1, 0, 0])]),
         [(0.99, [0, 0, 0], None), (1, [1, 0, 0], None)], 'Hold keeps the previous position until the next key time.'),
    case('ease_in_out_quarter', document([key(0, [0, 0, 0], ease='in_out'), key(4, [8, 0, 0])]),
         [(1, [1.25, 0, 0], None), (2, [4, 0, 0], None)], 'u=0.25 -> u^2(3-2u)=0.15625, times 8.'),
    case('ease_in_and_out_half', document([key(0, [0, 0, 0], ease='in'), key(2, [4, 0, 0], ease='out'), key(4, [4, 0, 0])]),
         [(1, [1, 0, 0], None), (3, [4, 0, 0], None)], 'In at u=0.5 -> 0.25 (x4=1). The second segment has equal ends.'),
    case('loop_wraps_without_closing_segment', document([key(0, [0, 0, 0]), key(2, [2, 0, 0])], loop='loop'),
         [(1, [1, 0, 0], None), (2, [0, 0, 0], None), (3, [1, 0, 0], None), (4.5, [0.5, 0, 0], None)],
         'At exactly the last key time the result is the first keyframe.'),
    case('ping_pong_reflects', document([key(0, [0, 0, 0]), key(2, [2, 0, 0])], loop='ping_pong'),
         [(1, [1, 0, 0], None), (2, [2, 0, 0], None), (2.5, [1.5, 0, 0], None), (4, [0, 0, 0], None), (5, [1, 0, 0], None)],
         'Period is twice the span.'),
    case('speed_scales_time', document([key(0, [0, 0, 0]), key(2, [2, 0, 0])], speed=2),
         [(0.5, [1, 0, 0], None), (1, [2, 0, 0], None), (3, [2, 0, 0], None)], 'Effective time is time * speed.'),
    case('catmull_rom_through_and_between', document([key(0, [0, 0, 0], interp='catmull_rom'), key(1, [1, 0, 0], interp='catmull_rom'),
                                                         key(2, [1, 0, 0], interp='catmull_rom'), key(3, [0, 0, 0])]),
         [(0, [0, 0, 0], None), (0.5, [0.5, 0, 0], None), (1, [1, 0, 0], None), (1.5, [1.125, 0, 0], None), (2, [1, 0, 0], None)],
         'Endpoint neighbors are clamped; x values hand-computed with the uniform Catmull-Rom basis.'),
    case('catmull_rom_interior_reproduces_line', document([key(i, [i, 0, 0], interp='catmull_rom') for i in range(5)]),
         [(1.5, [1.5, 0, 0], None), (2.5, [2.5, 0, 0], None), (0.5, [0.4375, 0, 0], None)],
         'Interior segments of evenly spaced collinear data stay straight; the clamped first segment does not (hand value 0.4375).'),
    case('bezier_midpoint', document([key(0, [0, 0, 0], interp='bezier', tangent_out=[0, 4, 0]), key(1, [4, 0, 0], tangent_in=[0, 4, 0])]),
         [(0, [0, 0, 0], None), (0.5, [2, 3, 0], None), (1, [4, 0, 0], None)], '(P0+3C1+3C2+P1)/8 with C1=[0,4,0], C2=[4,4,0].'),
    case('gain_curve_follows_time_map', document([key(0, [0, 0, 0]), key(2, [2, 0, 0])], parameters={'gain': [{'t': 0, 'value': 0}, {'t': 2, 'value': 1}]}),
         [(1, [1, 0, 0], {'gain': 0.5}), (3, [2, 0, 0], {'gain': 1})], 'Parameters interpolate linearly with the same time map.'),
]
waypoint_doc = document([key(0, [0, 0, 0]), {'t': 2, 'waypoint': 'door'}])
waypoint_doc['waypoints'] = [{'id': 'door', 'name': 'Door', 'position': [0, 2, 0]}]
labelled = document([key(0, [0, 0, 0]), key(1, [1, 0, 0])])
labelled['tracks'][0]['target'].update(label='Moving object', object_id=3)
cases.append(case('target_label_and_object_id_are_informational', labelled, [(0.5, [0.5, 0, 0], None)], 'Optional label/object_id do not affect sampling.'))
cases.append(case('waypoint_reference_resolves', waypoint_doc, [(1, [0, 1, 0], None), (2, [0, 2, 0], None)], 'Waypoint ids resolve at sampling time.'))

base = document([key(0, [0, 0, 0]), key(1, [1, 0, 0])])
invalid = []


def bad(name, mutate, codes):
    doc = copy.deepcopy(base)
    mutate(doc)
    invalid.append({'name': name, 'document': doc, 'codes': sorted(codes)})


bad('wrong_schema', lambda d: d.update(schema='x'), ['bad_schema'])
bad('wrong_version', lambda d: d.update(schema_version=2), ['bad_version'])
bad('wrong_space', lambda d: d.update(space='unity'), ['bad_space'])
bad('no_tracks', lambda d: d.update(tracks=[]), ['no_tracks'])
bad('bad_track_id', lambda d: d['tracks'][0].update(id='bad id'), ['bad_id'])
bad('duplicate_track_id', lambda d: d['tracks'].append(copy.deepcopy(d['tracks'][0])), ['duplicate_id'])
bad('bad_target_kind', lambda d: d['tracks'][0]['target'].update(kind='mixer'), ['bad_target'])
bad('bad_target_label', lambda d: d['tracks'][0]['target'].update(label=5), ['bad_target'])
bad('zero_object_id', lambda d: d['tracks'][0]['target'].update(object_id=0), ['bad_target'])
bad('float_object_id', lambda d: d['tracks'][0]['target'].update(object_id=1.5), ['bad_target'])
bad('empty_target_key', lambda d: d['tracks'][0]['target'].update(key=''), ['bad_target'])
bad('bad_loop', lambda d: d['tracks'][0].update(loop='forever'), ['bad_loop'])
bad('zero_speed', lambda d: d['tracks'][0].update(speed=0), ['bad_speed'])
bad('no_keyframes', lambda d: d['tracks'][0].update(keyframes=[]), ['no_keyframes'])
bad('times_equal', lambda d: d['tracks'][0]['keyframes'][1].update(t=0), ['times_not_increasing'])
bad('negative_time', lambda d: d['tracks'][0]['keyframes'][0].update(t=-1), ['bad_time'])
bad('position_wrong_length', lambda d: d['tracks'][0]['keyframes'][0].update(position=[0, 0]), ['bad_position'])
bad('position_out_of_range', lambda d: d['tracks'][0]['keyframes'][0].update(position=[17, 0, 0]), ['out_of_range'])
bad('position_and_waypoint', lambda d: d['tracks'][0]['keyframes'][0].update(waypoint='a'), ['position_conflict'])
bad('missing_position', lambda d: d['tracks'][0]['keyframes'][0].pop('position'), ['position_conflict'])
bad('unknown_waypoint', lambda d: d['tracks'][0]['keyframes'][0].update(waypoint='nowhere', position=None) or d['tracks'][0]['keyframes'][0].pop('position'), ['unknown_waypoint'])
bad('bad_interp', lambda d: d['tracks'][0]['keyframes'][0].update(interp='spline'), ['bad_interp'])
bad('bad_ease', lambda d: d['tracks'][0]['keyframes'][0].update(ease='bounce'), ['bad_ease'])
bad('bad_tangent', lambda d: d['tracks'][0]['keyframes'][0].update(tangent_out=[100, 0, 0]), ['bad_tangent'])
bad('gain_out_of_range', lambda d: d['tracks'][0].update(parameters={'gain': [{'t': 0, 'value': 5}]}), ['bad_parameter'])
bad('parameter_times_not_increasing', lambda d: d['tracks'][0].update(parameters={'spread': [{'t': 1, 'value': 0}, {'t': 1, 'value': 1}]}), ['bad_parameter'])

fixture = {'schema_version': 1, 'tolerance': 1e-9, 'spec': 'Specs/Path-v1.md', 'cases': cases, 'invalid': invalid,
           'engine_conversions': [{'canonical': [1, 2, 3], 'unity_m': [2, 3, 1], 'unreal_cm': [100, 200, 300], 'adm_xyz': [2, 1, 3]}]}
(ROOT / 'Fixtures/path-v1.json').write_text(json.dumps(fixture, indent=2) + '\n', encoding='utf-8', newline='\n')
