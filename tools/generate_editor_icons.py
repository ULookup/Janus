"""Generate original Janus UI vectors and native geometry; Python standard library only."""
from pathlib import Path
import argparse
import html

ROOT = Path(__file__).resolve().parents[1]
BLUE, GREEN, GREY, PURPLE, AMBER, RED = '#65b7f3', '#43d9ac', '#b9c8d8', '#bca0f5', '#f2c566', '#f17b82'

def path(*points): return ('path', points)
def closed(*points): return ('closed', points)
def fill(*points): return ('fill', points)
def rect(x, y, w, h, radius=1): return ('rect', (x, y, w, h, radius))
def circle(x, y, r): return ('circle', (x, y, r))

ICONS = {
    'Save': (BLUE, [closed((4,3),(17,3),(21,7),(21,21),(3,21),(3,3)), rect(7,3,9,6,0), rect(7,14,10,7,0)]),
    'Undo': (BLUE, [path((9,4),(3,10),(9,16)), path((3,10),(14,10),(19,13),(20,18))]),
    'Redo': (BLUE, [path((15,4),(21,10),(15,16)), path((21,10),(10,10),(5,13),(4,18))]),
    'Play': (GREEN, [fill((6,3),(21,12),(6,21))]),
    'Pause': (AMBER, [rect(5,4,4,16,0.5), rect(15,4,4,16,0.5)]),
    'Step': (BLUE, [fill((4,4),(15,12),(4,20)), rect(18,4,3,16,0.5)]),
    'Stop': (GREY, [rect(5,5,14,14,1)]),
    'Select': (GREY, [closed((5,3),(19,13),(13,14),(10,21),(5,3))]),
    'Move': (BLUE, [path((12,3),(12,21)), path((3,12),(21,12)), path((9,6),(12,3),(15,6)), path((9,18),(12,21),(15,18)), path((6,9),(3,12),(6,15)), path((18,9),(21,12),(18,15))]),
    'Rotate': (BLUE, [path((19,7),(15,3),(8,3),(3,8),(3,16),(8,21),(16,21),(21,16)), path((19,2),(19,7),(14,7))]),
    'Scale': (BLUE, [rect(3,13,8,8,0), path((11,13),(21,3)), path((14,3),(21,3),(21,10))]),
    'Frame': (GREY, [path((3,9),(3,3),(9,3)), path((15,3),(21,3),(21,9)), path((21,15),(21,21),(15,21)), path((9,21),(3,21),(3,15)), rect(8,8,8,8,0)]),
    'Focus': (BLUE, [circle(12,12,7), path((12,2),(12,7)), path((12,17),(12,22)), path((2,12),(7,12)), path((17,12),(22,12))]),
    'Grid': (GREY, [rect(3,3,18,18,0), path((9,3),(9,21)), path((15,3),(15,21)), path((3,9),(21,9)), path((3,15),(21,15))]),
    'List': (GREY, [path((9,5),(21,5)), path((9,12),(21,12)), path((9,19),(21,19)), rect(3,4,2,2,0), rect(3,11,2,2,0), rect(3,18,2,2,0)]),
    'Entity': (BLUE, [closed((12,2),(21,7),(21,17),(12,22),(3,17),(3,7)), path((3,7),(12,12),(21,7)), path((12,12),(12,22))]),
    'Hierarchy': (GREY, [rect(8,2,8,5), rect(2,17,7,5), rect(15,17,7,5), path((12,7),(12,12),(5,12),(5,17)), path((12,12),(19,12),(19,17))]),
    'Inspector': (BLUE, [path((3,5),(21,5)), path((3,12),(21,12)), path((3,19),(21,19)), rect(7,2,4,6), rect(15,9,4,6), rect(6,16,4,6)]),
    'Transform': (BLUE, [path((5,20),(5,3)), path((5,20),(21,20)), path((2,6),(5,3),(8,6)), path((18,17),(21,20),(18,23)), path((5,20),(15,10)), path((10,10),(15,10),(15,15))]),
    'Texture': (BLUE, [rect(3,3,18,18), circle(8,8,2), path((3,18),(9,12),(13,16),(17,10),(21,15))]),
    'Script': (GREEN, [path((8,5),(2,12),(8,19)), path((16,5),(22,12),(16,19)), path((14,3),(10,21))]),
    'Animation': (PURPLE, [rect(3,5,18,16), path((3,10),(21,10)), path((7,5),(10,10)), path((13,5),(16,10)), fill((9,13),(15,16),(9,19))]),
    'Audio': (AMBER, [path((9,17),(9,5),(20,3),(20,15)), path((9,9),(20,7)), circle(6,18,3), circle(17,16,3)]),
    'Font': (GREY, [path((3,6),(3,3),(21,3),(21,6)), path((12,3),(12,21)), path((7,21),(17,21))]),
    'Camera': (GREY, [rect(2,6,14,13), closed((16,10),(22,6),(22,19),(16,15))]),
    'Canvas': (BLUE, [rect(2,3,20,16), path((8,22),(16,22)), path((12,19),(12,22)), rect(5,6,5,9,0), path((13,7),(19,7)), path((13,11),(19,11))]),
    'Collider': (GREEN, [path((8,3),(3,3),(3,8)), path((16,3),(21,3),(21,8)), path((21,16),(21,21),(16,21)), path((8,21),(3,21),(3,16)), rect(7,7,10,10,0)]),
    'Physics': (AMBER, [circle(12,13,7), path((12,3),(12,13),(16,16)), path((3,22),(21,22)), path((9,2),(15,2))]),
    'Prefab': (BLUE, [closed((12,2),(21,7),(21,17),(12,22),(3,17),(3,7)), path((3,7),(12,12),(21,7)), path((12,12),(12,22)), closed((12,6),(16,8),(12,10),(8,8))]),
    'Folder': (AMBER, [closed((3,5),(9,5),(12,8),(21,8),(21,21),(3,21)), path((3,11),(21,11))]),
    'Settings': (GREY, [closed((9,2),(15,2),(16,6),(20,6),(23,12),(20,18),(16,18),(15,22),(9,22),(8,18),(4,18),(1,12),(4,6),(8,6)), circle(12,12,4)]),
    'Search': (GREY, [circle(10,10,6), path((15,15),(21,21))]),
    'Add': (BLUE, [path((12,4),(12,20)), path((4,12),(20,12))]),
    'Delete': (GREY, [path((3,6),(21,6)), path((5,6),(6,21),(18,21),(19,6)), path((8,6),(8,3),(16,3),(16,6)), path((10,10),(10,17)), path((14,10),(14,17))]),
    'Console': (GREY, [rect(2,3,20,18), path((6,8),(10,12),(6,16)), path((13,16),(18,16))]),
    'Info': (BLUE, [circle(12,12,10), circle(12,7,0.7), path((12,11),(12,17))]),
    'Warning': (AMBER, [closed((12,2),(23,21),(1,21)), path((12,8),(12,13)), circle(12,17,0.7)]),
    'Error': (RED, [circle(12,12,10), path((8,8),(16,16)), path((16,8),(8,16))]),
    'Agent': (PURPLE, [rect(3,7,18,14,3), path((12,3),(12,7)), circle(12,2,1), circle(8,13,1), circle(16,13,1), path((8,17),(16,17))]),
    'Profiler': (GREEN, [path((3,3),(3,21),(22,21)), path((6,16),(10,11),(14,14),(21,5))]),
}

def generate():
    out = ROOT / 'Editor/Icons'
    files = {}
    enum = '// Generated by tools/generate_editor_icons.py.\n#pragma once\nnamespace Janus::Editor {\nenum class Icon {\n'
    enum += ''.join(f'    {name},\n' for name in ICONS) + '};\n}\n'
    files[out / 'EditorIconCatalog.h'] = enum
    native = '// Generated by tools/generate_editor_icons.py.\n'
    native += 'ImVec4 IconColor(Icon icon) { switch(icon) {\n'
    for name, (color, shapes) in ICONS.items():
        rgb = [int(color[i:i+2], 16) / 255 for i in (1,3,5)]
        native += f'case Icon::{name}: return ImVec4{{{rgb[0]:.6f}f,{rgb[1]:.6f}f,{rgb[2]:.6f}f,1.0f}};\n'
    native += '} return ImVec4{1,1,1,1}; }\n'
    native += 'void DrawIcon(Icon icon, ImVec2 origin, float size) {\n'
    native += 'if(size <= 0) return;\nauto* draw = ImGui::GetWindowDrawList();\nconst float scale=size/24.0f;\nconst float stroke=2.0f*scale;\nconst ImU32 color=ImGui::GetColorU32(IconColor(icon));\nauto P=[&](float x,float y){return ImVec2{origin.x+x*scale,origin.y+y*scale};};\nswitch(icon) {\n'
    gallery = []
    for name, (color, shapes) in ICONS.items():
        native += f'case Icon::{name}: {{\n'
        svg = []
        for kind, data in shapes:
            if kind in ('path','closed','fill'):
                points = ' '.join(f'{x},{y}' for x,y in data)
                tag = 'polyline' if kind == 'path' else 'polygon'
                svg.append(f'<{tag} points="{points}"' + (' fill="currentColor" stroke="none"' if kind=='fill' else '') + '/>')
                if kind == 'fill':
                    native += '{const ImVec2 points[]{'+','.join(f'P({x},{y})' for x,y in data)+'};draw->AddConvexPolyFilled(points,IM_ARRAYSIZE(points),color);}\n'
                else:
                    pairs = list(zip(data, data[1:]))
                    if kind == 'closed': pairs.append((data[-1],data[0]))
                    for (x,y),(u,v) in pairs:
                        native += f'draw->AddLine(P({x},{y}),P({u},{v}),color,stroke);\n'
                    for x,y in data:
                        native += f'draw->AddCircleFilled(P({x},{y}),stroke/2,color,8);\n'
            elif kind == 'rect':
                x,y,w,h,r = data
                svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}"/>')
                native += f'draw->AddRect(P({x},{y}),P({x+w},{y+h}),color,{float(r)}f*scale,0,stroke);\n'
            else:
                x,y,r = data
                svg.append(f'<circle cx="{x}" cy="{y}" r="{r}"/>')
                native += f'draw->AddCircle(P({x},{y}),{float(r)}f*scale,color,0,stroke);\n'
        native += 'break;}\n'
        content = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="color:{color}">' + ''.join(svg) + '</svg>\n'
        files[out / 'svg' / f'{name.lower()}.svg'] = content
        gallery.append(f'<article><div class="sizes">'+''.join(f'<img src="svg/{name.lower()}.svg" width="{s}" height="{s}" alt="">' for s in (16,24,40))+f'</div><span>{html.escape(name)}</span></article>')
    native += '}}\n'
    files[out / 'EditorIconGeometry.inl'] = native
    files[out / 'preview.html'] = '<!doctype html><html lang="en"><meta charset="utf-8"><title>Janus Editor Icons</title><style>body{margin:40px;background:#171e27;color:#e1ebf5;font:15px system-ui}h1{font-size:28px}p{color:#9dafc2}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}article{background:#222d3a;border:1px solid #374657;border-radius:8px;padding:20px}.sizes{display:flex;align-items:center;gap:20px;height:52px}span{display:block;margin-top:12px}</style><h1>Janus / Editor icons</h1><p>Original vectors · 24 × 24 grid · 2 px stroke · 16 / 24 / 40 px previews</p><div class="grid">'+''.join(gallery)+'</div></html>\n'
    return files

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args=parser.parse_args()
    files=generate()
    if args.check:
        stale=[str(p.relative_to(ROOT)) for p,text in files.items() if not p.exists() or p.read_text(encoding='utf-8')!=text]
        if stale: raise SystemExit('Stale icon outputs: '+', '.join(stale))
    else:
        for p,text in files.items():
            p.parent.mkdir(parents=True,exist_ok=True)
            p.write_text(text,encoding='utf-8',newline='\n')
    print(f'{len(ICONS)} icons; {len(files)} outputs '+('verified' if args.check else 'generated'))
