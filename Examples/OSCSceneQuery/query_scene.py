"""No pip packages. From a complete repository: python Examples/OSCSceneQuery/query_scene.py.
Use --port 9000 for a running Playback receiver, or the default spatial input 8100.
"""
import argparse
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'Apps/OSCController'))
from osc_controller.scene_query import query_scene

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--host',default='127.0.0.1');parser.add_argument('--port',type=int,default=8100)
    parser.add_argument('--page-size',type=int,default=16)
    args=parser.parse_args()
    try:scene=query_scene(args.host,args.port,args.page_size)
    except (ValueError,OSError) as error:parser.exit(1,str(error)+'\n')
    if hasattr(sys.stdout,'reconfigure'):sys.stdout.reconfigure(encoding='utf-8')
    print(json.dumps(scene,indent=2,ensure_ascii=False,allow_nan=False))
