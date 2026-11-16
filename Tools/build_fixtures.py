"""Generate deterministic OSC golden vectors with only the Python standard library."""
import json
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]
def string(s):
    b=s.encode('utf-8')+b'\0';return b+b'\0'*(-len(b)%4)
def packet(address,args):
    result=string(address)+string(','+''.join(t for t,_ in args))
    for tag,value in args:
        result+=string(value) if tag=='s' else struct.pack('>i' if tag=='i' else '>f',value)
    return result
vectors=[]
for name,address,args in [
 ('address_only_stop','/demo/playback/mono/stop',[]),
 ('numeric_play','/demo/playback/mono/play',[('f',1.)]),
 ('scene_query','/spatialfabric/scene/query',[('s','fixture_001'),('i',9011),('i',0),('i',16)]),
 ('adm_front','/adm/obj/1/xyz',[('f',0.),('f',1.),('f',0.)]),
 ('unicode_name','/adm/obj/1/name',[('s','日本語 🎧')])]:
 vectors.append({'name':name,'address':address,'tags':','+''.join(t for t,_ in args),'hex':packet(address,args).hex()})
invalid=[{'name':'bad_padding','hex':'2f7366002c6600013f800000'}, {'name':'nan_float','hex':(string('/sf')+string(',f')+struct.pack('>I',0x7fc00000)).hex()}, {'name':'unknown_tag','hex':(string('/sf')+string(',q')).hex()}]
(ROOT/'Fixtures/osc-v1.json').write_text(json.dumps({'schema_version':1,'valid':vectors,'invalid':invalid},indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
coords=[{'name':'front','canonical':[1,0,0],'unity_m':[0,0,1],'unreal_cm':[100,0,0],'adm_xyz':[0,1,0],'adm_aed':[0,0,1]}, {'name':'right','canonical':[0,1,0],'unity_m':[1,0,0],'unreal_cm':[0,100,0],'adm_xyz':[1,0,0],'adm_aed':[-90,0,1]}, {'name':'up','canonical':[0,0,1],'unity_m':[0,1,0],'unreal_cm':[0,0,100],'adm_xyz':[0,0,1],'adm_aed':[0,90,1]}]
(ROOT/'Fixtures/coordinates-v1.json').write_text(json.dumps({'schema_version':1,'unit_stage_half_extent_m':1,'vectors':coords},indent=2)+'\n',encoding='utf-8')
custom=[]
def custom_case(name,config,address,state,expected):
    custom.append({'name':name,'config':config,'address':address,'state':state,'expected':[{'address':a,'args':v,'hex':packet(a,[('f',n) for n in v]).hex()} for a,v in expected]})
custom_case('packed_xyz',{},'/custom/{id}/xyz',{'id':7,'front':.5,'right':-.5,'up':1},[('/custom/7/xyz',[.5,-.5,1])])
custom_case('aed_clamped_ranges',{'coordinates':1,'positionMin':0,'positionMax':1,'argumentTemplate':'a e d gain width id'},'/custom/{id}/aed',{'id':9,'front':0,'right':1,'up':0,'gain':1.5,'width':-.1},[('/custom/9/aed',[.25,.5,1,1,0,9])])
custom_case('discrete_aed_aliases',{'coordinates':1,'sendMode':1,'argumentTemplate':'a e d'},'/object/{id}/{axis}',{'id':2,'front':1,'right':0,'up':0},[('/object/2/azimuth',[0]),('/object/2/elevation',[0]),('/object/2/distance',[1])])
custom_case('address_fields_and_numeric_format',{'positionMin':0,'positionMax':100,'gainMin':0,'gainMax':10,'widthMin':0,'widthMax':100,'argumentTemplate':'id'},'/{room}/{label}/{id}/{x}/{gain}/{width}',{'id':11,'label':'Tap','front':1,'right':0,'up':0,'gain':.5,'width':0,'fields':[{'key':'room','value':'Hall_A'}]},[('/Hall_A/Tap/11/100.0/5.0/0.0',[11])])
custom_case('reverse_ranges_case_insensitive_args',{'positionMin':1,'positionMax':-1,'gainMin':10,'gainMax':0,'widthMin':2,'widthMax':4,'argumentTemplate':'X y Z gain WIDTH'},'/reverse/{id}',{'id':3,'front':-1,'right':1,'up':0,'gain':.25,'width':.5},[('/reverse/3',[1,-1,0,7.5,3])])
(ROOT/'Fixtures/custom-v1.json').write_text(json.dumps({'schema_version':1,'vectors':custom},indent=2)+'\n',encoding='utf-8')
print('Wrote',len(vectors),'valid OSC vectors,',len(invalid),'invalid packets,',len(coords),'coordinate vectors and',len(custom),'Custom contract vectors.')
