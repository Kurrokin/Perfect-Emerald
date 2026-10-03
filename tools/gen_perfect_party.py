# Perfect UI party screen: build tiles, tilemaps and palettes from the design sheet.
import numpy as np, struct, os
from PIL import Image
SHEET='/mnt/user-data/uploads/Yes_You_Party_Screen.png'
S=np.array(Image.open(SHEET).convert('RGBA')).astype(int)
def hx(p): return None if p[3]==0 else '#%02x%02x%02x'%tuple(p[:3])
def crop(x0,y0,x1,y1): return [[hx(S[y,x]) for x in range(x0,x1)] for y in range(y0,y1)]
BG_A,BG_B='#484e6f','#3c4664'
def pattern(x,y): return BG_B if (x%4==3 or y%4==3) else BG_A
# ---------------- static background (240x160) ----------------
M=crop(4,3,244,163)
B=[row[:] for row in M]
def fill_pattern(x0,y0,x1,y1):
    for y in range(y0,y1):
        for x in range(x0,x1): B[y][x]=pattern(x,y)
fill_pattern(0,24,90,80)       # under the main slot (the window draws it)
fill_pattern(86,8,240,130)     # under the five wide slots
# keep the mock-up's top band (y<8) and the cancel panel as they are
# ---------------- palettes ----------------
def rgb15(h):
    r,g,b=int(h[1:3],16),int(h[3:5],16),int(h[5:7],16); return (r>>3)|((g>>3)<<5)|((b>>3)<<10)
# palette 0: background
P0=['#3c4664','#484e6f','#373c60','#61c8ef','#6fdff7','#b3eefb','#ffffff','#588ade','#6077d7','#89b3f8','#8c3956','#8ea7e6','#3c4664','#3c4664','#3c4664','#3c4664']
# cancel panel palettes: pal1 normal, pal2 selected (indices 1,11,12 of pal1 are also read for empty slots)
ARROW_N=hx(S[272,104]); ARROW_S=hx(S[272,121])
print('arrow colours normal/selected sample',ARROW_N,ARROW_S)
# ---------------- tiles for the background ----------------
cols=set(c for row in B for c in row if c)
print('background colours',sorted(cols))
cmap0={c:i for i,c in enumerate(P0) if i not in (12,13,14,15)}
cmap0['#3c4664']=0
def tile_of(img,tx,ty,cmap):
    t=[]
    for y in range(8):
        for x in range(8):
            c=img[ty*8+y][tx*8+x]
            t.append(0 if c is None else cmap[c])
    return tuple(t)
def flips(t):
    a=np.array(t).reshape(8,8)
    return {(0,0):tuple(a.flatten()),(1,0):tuple(a[:,::-1].flatten()),(0,1):tuple(a[::-1,:].flatten()),(1,1):tuple(a[::-1,::-1].flatten())}
tiles=[tuple([0]*64)]; index={tiles[0]:(0,0,0)}
bgmap=[[0]*32 for _ in range(32)]
CANCEL=set((x,y) for x in range(23,30) for y in range(17,19))
for ty in range(20):
    for tx in range(30):
        t=tile_of(B,tx,ty,cmap0)
        found=None
        for (h,v),ft in flips(t).items():
            if ft in index:
                base,bh,bv=index[ft]; found=(base,h,v); break
        if found is None:
            index[t]=(len(tiles),0,0); tiles.append(t); found=(len(tiles)-1,0,0)
        pal=1 if (tx,ty) in CANCEL else 0
        bgmap[ty][tx]=found[0] | (found[1]<<10) | (found[2]<<11) | (pal<<12)
NBG=len(tiles)
print('background tiles',NBG)
# ---------------- confirm button (for the choose-several modes): the cancel panel with a check mark ----------------
cancel_tm=[bgmap[ty][tx] for ty in (17,18) for tx in range(23,30)]
# rebuild the 7x2 region image, then draw a check over the arrow
reg=[[B[y][x] for x in range(184,240)] for y in range(136,152)]
# find the arrow (white) and paint it with the panel cyan, then draw a check mark in white
for y in range(16):
    for x in range(56):
        if reg[y][x] in ('#ffffff','#b3eefb'): reg[y][x]='#61c8ef'
check=[(0,4),(1,5),(2,6),(3,5),(4,4),(5,3),(6,2),(7,1),(8,0)]
for dx,dy in check:
    for t in (0,1):
        x,y=30+dx,5+dy+t
        if 0<=y<16: reg[y][x]='#ffffff'
confirm_tm=[]
for ty in range(2):
    for tx in range(7):
        t=tuple(0 if reg[ty*8+y][tx*8+x] is None else cmap0[reg[ty*8+y][tx*8+x]] for y in range(8) for x in range(8))
        if t in index: confirm_tm.append(index[t][0] | (1<<12))
        else:
            index[t]=(len(tiles),0,0); tiles.append(t); confirm_tm.append((len(tiles)-1) | (1<<12))
NBG=len(tiles)
print('background + button tiles',NBG); assert NBG<99
# ---------------- slot art ----------------
def cut_cols(art, start, n):
    return [row[:start]+row[start+n:] for row in art]
MAIN_SRC=crop(2,165,89,212)   # 87x47 normal main slot
WIDE_SRC=crop(91,165,244,187) # 153x22 normal wide slot
main=cut_cols(MAIN_SRC,40,7)  # 80 wide
wide=cut_cols(WIDE_SRC,40,9)  # 144 wide
def find_bar(art):
    # the HP bar: a run of white under-line pixels at the right of the cyan strip
    best=None
    for r,row in enumerate(art):
        runs=[];s=None
        for x,c in enumerate(row+[None]):
            if c=='#ffffff' and s is None: s=x
            if c!='#ffffff' and s is not None: runs.append((s,x)); s=None
        for a,b in runs:
            if b-a>=30 and (best is None or b-a>best[2]-best[1]): best=(r,a,b)
    return best
def slot_indices(art, W, H, oy):
    """window-sized index image: art placed at row oy, 1px ring where the art is transparent"""
    bar=find_bar(art)
    r_u,x0,x1=bar
    bar_rows=range(r_u-4,r_u)            # top line, 3 bar rows
    img=[[0]*W for _ in range(H)]
    for y,row in enumerate(art):
        for x,c in enumerate(row):
            if c is None: continue
            inbar=(y in bar_rows and x0<=x<x1)
            if c=='#588ade': v=15 if inbar else 4
            elif c=='#6077d7': v=5
            elif c=='#373c60': v=6
            elif c=='#3c4664': v=13 if inbar else 6
            elif c=='#484e6f': v=14
            elif c=='#61c8ef': v=7
            elif c=='#6fdff7': v=8
            elif c=='#ffffff': v=3
            elif c=='#8ea7e6': v=5
            else: raise SystemExit('unmapped colour %s'%c)
            img[y+oy][x]=v
    h=len(art)
    for x in range(W):
        for y in (oy-1, oy+h):
            if 0<=y<H and img[y][x]==0: img[y][x]=1
    for y in range(oy-1, oy+h+1):
        for x in (0, W-1):
            if 0<=y<H and img[y][x]==0: img[y][x]=1
    return img, (x0, r_u-3+oy, x1-x0)   # bar x, bar y (first of the 3 rows), width
main_img, main_bar = slot_indices(main, 80, 56, 1)
wide_img, wide_bar = slot_indices(wide, 144, 24, 1)
print('main bar',main_bar,'wide bar',wide_bar)
def strip_rows(img):
    return [y for y in range(len(img)) if img[y][0]==7 or img[y][1]==7]
def no_hp(img, bar):
    out=[r[:] for r in img]; bx,by,bw=bar
    # label + bar -> plain strip
    rows=range(by-1, by+3)
    for y in range(by-3, by+4):
        for x in range(0, len(img[0])):
            if out[y][x] in (3,13,14,15) and (x>=bx-20): out[y][x]=7
    return out
main_nohp=no_hp(main_img, main_bar); wide_nohp=no_hp(wide_img, wide_bar)
# empty slot: dark box
empty=[[0]*144 for _ in range(24)]
for y in range(24):
    for x in range(144):
        if y in (0,23) or x in (0,143): empty[y][x]=1
        elif y in (1,22) or x in (1,142): empty[y][x]=12
        else: empty[y][x]=11
# ---------------- slot tiles ----------------
slot_tiles={}
def add_slot_tiles(img, w, h):
    tm=[]
    for ty in range(h):
        for tx in range(w):
            t=tuple(img[ty*8+y][tx*8+x] for y in range(8) for x in range(8))
            if t==tiles[0]: tm.append(0); continue
            if t not in slot_tiles:
                slot_tiles[t]=len(tiles); tiles.append(t)
            tm.append(slot_tiles[t])
    return tm
tm_main=add_slot_tiles(main_img,10,7); tm_main_nohp=add_slot_tiles(main_nohp,10,7)
tm_wide=add_slot_tiles(wide_img,18,3); tm_wide_nohp=add_slot_tiles(wide_nohp,18,3); tm_empty=add_slot_tiles(empty,18,3)
print('total tiles',len(tiles),'(slot tiles',len(tiles)-NBG,')'); assert len(tiles)<=256
# ---------------- palettes (16 x 16) ----------------
pal=[[ '#000000']*16 for _ in range(16)]
pal[0]=P0[:]
pal[1]=P0[:]; pal[2]=P0[:]
pal[2][3]='#8fe0f8'; pal[2][4]='#a8ecfb'; pal[2][6]='#fff2f5'; pal[2][5]='#f5dbe4'
# empty-slot colours are read from palette 1 entries 1, 11, 12
pal[1][1]='#484e6f'; pal[1][11]='#343a58'; pal[1][12]='#4f5780'
COMMON={0:'#3c4664',2:'#484e6f',3:'#ffffff',13:'#3c4664',14:'#484e6f',15:'#588ade'}
def slotpal(): 
    p=['#000000']*16
    for k,v in COMMON.items(): p[k]=v
    return p
for i in range(3,11): pal[i]=slotpal()
def put(ids, colours):
    for i,c in zip(ids,colours): pal[i//16][i%16]=c
N1=['#588ade','#6077d7','#373c60']; N2=['#3c4664','#61c8ef','#6fdff7']          # normal
S1=['#89b3f8','#588ade','#588ade']; S2=['#8c3956','#61c8ef','#6fdff7']          # current selection (red ring)
F1=['#7d819e','#6a6e8a','#373c60']; F2=['#3c4664','#9aa3c4','#b5bdd8']          # fainted
SF1=['#a8adc6','#7d819e','#7d819e']                                              # current selection, fainted
MU1=['#9a86e0','#8270cf','#373c60']; MU2=['#3c4664','#61c8ef','#6fdff7']        # multi-battle partner
SMU1=['#c0b2f4','#9a86e0','#9a86e0']                                             # current selection, partner
A1=['#62c9a6','#4aa98c','#2f6f60']; A2=['#f8d850','#8fe8c8','#b8f4de']          # selected to switch / use on
put([52,53,54],N1); put([49,55,56],N2)
put([116,117,118],S1); put([97,103,104],S2)
put([84,85,86],F1); put([81,87,88],F2); put([148,149,150],SF1)
put([68,69,70],MU1); put([65,71,72],MU2); put([132,133,134],SMU1)
put([100,101,102],A1); put([161,167,168],A2)
# HP bar colours from the design (top row, main row)
def strip(y): return hx(S[y,100]), hx(S[y+1,100])
g=strip(253); yl=strip(257); rd=strip(261)
print('hp colours',g,yl,rd)
put([58,57],list(g)); put([74,73],list(yl)); put([90,89],list(rd))
# gender symbol colours (fg, shadow)
put([59,60],['#8ecbff','#3c4664']); put([75,76],['#ffa0c0','#3c4664'])
# every slot palette starts in the normal state
for i in range(3,11):
    for off,c in zip((4,5,6),N1): 
        if pal[i][off]=='#000000': pal[i][off]=c
    for off,c in zip((1,7,8),N2):
        if pal[i][off]=='#000000': pal[i][off]=c
# ---------------- write files ----------------
OUT='/home/claude/pokeemerald_modern_hack_source/graphics/party_menu/perfect/'
os.makedirs(OUT,exist_ok=True)
with open(OUT+'bg_pal.gbapal','wb') as f:
    for p in pal:
        for c in p: f.write(struct.pack('<H',rgb15(c)))
W=16; Hn=(len(tiles)+W-1)//W
img=Image.new('P',(W*8,Hn*8)); img.putpalette([v for c in P0 for v in (int(c[1:3],16),int(c[3:5],16),int(c[5:7],16))])
px=img.load()
for i,t in enumerate(tiles):
    for y in range(8):
        for x in range(8): px[(i%W)*8+x,(i//W)*8+y]=t[y*8+x]
img.save(OUT+'bg.png')
with open(OUT+'bg.bin','wb') as f:
    for row in bgmap:
        for v in row: f.write(struct.pack('<H',v))
for name,tm in (('slot_main',tm_main),('slot_main_no_hp',tm_main_nohp),('slot_wide',tm_wide),('slot_wide_no_hp',tm_wide_nohp),('slot_wide_empty',tm_empty)):
    open(OUT+name+'.bin','wb').write(bytes(tm))
open(OUT+'cancel_button.bin','wb').write(b''.join(struct.pack('<H',v) for v in cancel_tm))
open(OUT+'confirm_button.bin','wb').write(b''.join(struct.pack('<H',v) for v in confirm_tm))
import json
json.dump({'main_bar':main_bar,'wide_bar':wide_bar,'main_strip':strip_rows(main_img),'wide_strip':strip_rows(wide_img)},open('/home/claude/pui/layout.json','w'))
print('written; main strip rows',strip_rows(main_img)[:1],strip_rows(main_img)[-1:],'wide strip rows',strip_rows(wide_img)[:1],strip_rows(wide_img)[-1:])
# ---------------- preview: the whole screen in the normal state, slot 0 selected ----------------
def c2rgb(c): return (int(c[1:3],16),int(c[3:5],16),int(c[5:7],16))
scr=Image.new('RGB',(240,160)); sp=scr.load()
for ty in range(20):
    for tx in range(30):
        e=bgmap[ty][tx]; t=tiles[e&0x3ff]; h=(e>>10)&1; v=(e>>11)&1; p=pal[e>>12]
        for y in range(8):
            for x in range(8):
                sx=7-x if h else x; sy=7-y if v else y
                i=t[sy*8+sx]; sp[tx*8+x,ty*8+y]=c2rgb(p[i])
def blit(img,wx,wy,state):
    p=slotpal()
    for k,v in enumerate(N1): p[4+k]=v
    p[1],p[7],p[8]=N2
    if state=='sel':
        for k,v in enumerate(S1): p[4+k]=v
        p[1],p[7],p[8]=S2
    p[9],p[10]=g[1],g[0]
    for y,row in enumerate(img):
        for x,i in enumerate(row):
            if i: sp[wx+x,wy+y]=c2rgb(p[i])
blit(main_img,8,24,'sel')
for k in range(5): blit(wide_img,96,8+24*k,'normal')
scr.resize((720,480),Image.NEAREST).save('/home/claude/pui/preview.png')
# ---------------- status icons (32x64: PSN PAR SLP FRZ BRN PKRS FNT blank) ----------------
new={'PAR':(97,244),'BRN':(118,244),'PSN':(139,244),'SLP':(160,244),'FRZ':(181,244)}
order=['PSN','PAR','SLP','FRZ','BRN','PKRS','FNT',None]
van=Image.open('/home/claude/pokeemerald_modern_hack_source/graphics/interface/status_icons.png').convert('RGBA')
vp=np.array(van).astype(int)
spal=['#000000']; 
frames=[]
for name in order:
    fr=[[None]*32 for _ in range(8)]
    if name in new:
        x0,y0=new[name]
        for y in range(8):
            for x in range(20): fr[y][x]=hx(S[y0+y,x0+x])
    elif name in ('PKRS','FNT'):
        k=order.index(name)
        for y in range(8):
            for x in range(32):
                p=vp[k*8+y,x]; fr[y][x]=None if p[3]==0 or tuple(p[:3])==tuple(vp[0,0][:3]) and False else '#%02x%02x%02x'%tuple(p[:3])
    frames.append(fr)
# palette: colours of the new icons first, then fit the two old icons to them
cset=[]
for fr in frames[:5]:
    for row in fr:
        for c in row:
            if c and c not in cset: cset.append(c)
print('status icon colours (new)',len(cset))
def nearest(c):
    r,g,b=c2rgb(c); return min(cset,key=lambda d:(c2rgb(d)[0]-r)**2+(c2rgb(d)[1]-g)**2+(c2rgb(d)[2]-b)**2)
bgc='#%02x%02x%02x'%tuple(vp[0,0][:3])
for k in (5,6):
    for y in range(8):
        for x in range(32):
            c=frames[k][y][x]
            if c==bgc: frames[k][y][x]=None
            elif c: frames[k][y][x]=nearest(c)
assert len(cset)<=15, len(cset)
spal=['#000000']+cset+['#000000']*(15-len(cset))
simg=Image.new('P',(32,64)); simg.putpalette([v for c in spal for v in c2rgb(c)])
spx=simg.load()
for k,fr in enumerate(frames):
    for y in range(8):
        for x in range(32):
            c=fr[y][x]; spx[x,k*8+y]=0 if c is None else spal.index(c)
simg.save(OUT+'status_icons.png')
# ---------------- constants for the code ----------------
pattern_tile=bgmap[12][4]&0x3ff
open('/home/claude/pokeemerald_modern_hack_source/include/perfect_party_ui.h','w').write('''// GENERATED by gen_party.py (Perfect UI party screen) - layout of the new slot art
#ifndef GUARD_PERFECT_PARTY_UI_H
#define GUARD_PERFECT_PARTY_UI_H
#define PERFECT_PARTY_PATTERN_TILE %d   // plain background tile (under the cancel button in the showcase)
#define PERFECT_MAIN_BAR_X %d
#define PERFECT_MAIN_BAR_Y %d
#define PERFECT_MAIN_BAR_W %d
#define PERFECT_WIDE_BAR_X %d
#define PERFECT_WIDE_BAR_Y %d
#define PERFECT_WIDE_BAR_W %d
#endif
'''%(pattern_tile,main_bar[0],main_bar[1],main_bar[2],wide_bar[0],wide_bar[1],wide_bar[2]))
print('pattern tile',pattern_tile)
