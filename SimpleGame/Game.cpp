#include "stdafx.h"
#include "Game.h"
#include "Dependencies/freeglut.h"
#include <cmath>
#include <algorithm>
#include <cwctype>
#include <stdexcept>

namespace {
const Color Ink(0x08151c), White(0xe9eee6), Muted(0x879b9f), Teal(0x70d7c7), Gold(0xe8bd7b);
float Clamp(float x, float lo, float hi) { return (std::max)(lo, (std::min)(hi, x)); }
unsigned Hash(int x, int y) { unsigned v = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u; v ^= v >> 13; return v * 1274126177u; }
}

Game::Game(Renderer& r) : renderer(r) {
    sky = renderer.LoadTexture("Resources/Textures/elysium_sky.png");
    portraits = renderer.LoadTexture("Resources/Textures/crew_portraits.png");
    if (!sky || !portraits || !canvas.Ready()) throw std::runtime_error("Required art texture or Canvas is missing");
    for(GLuint texture : {sky,portraits}) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    SnapCamera();
}
void Game::SnapCamera() { camera = GameWorld::Project(world.player) + Vec2{65,-30}; }
Vec2 Game::P(Vec2 grid, float z) const {
    Vec2 s = (GameWorld::Project(grid) - camera) * zoom;
    return {720 + s.x, 450 + s.y - z * zoom};
}
void Game::ClearInput() { for (bool& k : keys) k = false; for (bool& k : arrows) k = false; running = false; world.walking = false; }
void Game::Tick(float dt) {
    dt = Clamp(dt, 0, .05f);
    Vec2 input = {float((keys['d'] || arrows[3]) - (keys['a'] || arrows[2])), float((keys['s'] || arrows[1]) - (keys['w'] || arrows[0]))};
    world.Move(input, dt, running); world.Update(dt);
    zoom += (desiredZoom - zoom) * (1 - std::exp(-dt * 8));
    if (world.view != View::Title) {
        Vec2 target = GameWorld::Project(world.player) + Vec2{65,-30};
        camera = camera + (target - camera) * (1 - std::exp(-dt * 5));
        if (world.view == View::Explore) arrival += dt;
    }
    if (lastDialogueSize != world.dialogue.size()) { typing = 0; lastDialogueSize = world.dialogue.size(); }
    if (world.view == View::Dialogue) typing += dt * 42;
}
void Game::Key(unsigned char key, bool down) {
    key = (unsigned char)std::tolower(key); keys[key] = down;
    if (!down) return;
    if (world.view == View::Title) {
        if (key == 13 || key == ' ') { world.Start(); arrival = 0; }
        if (key == 27) exitRequested = true;
        return;
    }
    if (world.view == View::Ending) {
        if (key == 13) { world = GameWorld(); SnapCamera(); arrival = 0; ClearInput(); }
        return;
    }
    if (world.view == View::Dialogue) {
        if (world.choicePending && world.dialogue.empty()) {
            if (key == '1' || key == '2') world.Choose(key - '1');
        } else if (key == 'e' || key == ' ' || key == 13) {
            if (!world.dialogue.empty() && typing < world.dialogue.front().text.size()) typing = (float)world.dialogue.front().text.size();
            else world.Advance();
        }
        return;
    }
    if (key == 27) { world.view = world.view == View::Explore ? View::Pause : View::Explore; ClearInput(); return; }
    if (key == 'm') { world.view = world.view == View::Map ? View::Explore : View::Map; ClearInput(); return; }
    if (key == 'j') { world.view = world.view == View::Journal ? View::Explore : View::Journal; ClearInput(); return; }
    if (world.view == View::Pause) {
        if (key == 13) world.view = View::Explore;
        if (key == 't') { world.view = View::Title; ClearInput(); }
        return;
    }
    if (key == 'e') world.Interact();
    if (key == 'q') world.Scan();
}
void Game::Arrow(int key, bool down) {
    if (key == GLUT_KEY_UP) arrows[0] = down;
    if (key == GLUT_KEY_DOWN) arrows[1] = down;
    if (key == GLUT_KEY_LEFT) arrows[2] = down;
    if (key == GLUT_KEY_RIGHT) arrows[3] = down;
}
void Game::Wheel(int direction) { if (world.view == View::Explore) desiredZoom = Clamp(desiredZoom + direction * .10f, .72f, 1.45f); }
void Game::Click(float x, float y) {
    if (world.view == View::Title && x >= 100 && x <= 445 && y >= 581 && y <= 648) { world.Start(); arrival = 0; }
    else if (world.view == View::Dialogue) {
        if (world.choicePending && world.dialogue.empty()) {
            if (x > 120 && x < 1320 && y >= 699 && y < 755) world.Choose(0);
            if (x > 120 && x < 1320 && y >= 765 && y < 821) world.Choose(1);
        } else Key('e', true);
    } else if (world.view == View::Explore && x > 1180 && y < 226) Key('m', true);
}
void Game::Poly(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) { canvas.Quad(a,b,c,d,color.Fade(objectAlpha)); }
void Game::Box(Vec2 p, float w, float d, float z, float h, Color top, Color left, Color right) {
    Vec2 a = P(p + Vec2{-w/2,-d/2},z), b = P(p + Vec2{w/2,-d/2},z),
         c = P(p + Vec2{w/2,d/2},z), e = P(p + Vec2{-w/2,d/2},z);
    Vec2 up = {0,-h * zoom};
    Poly(b,c,c+up,b+up,right); Poly(c,e,e+up,c+up,left); Poly(a+up,b+up,c+up,e+up,top);
    canvas.Line(a+up,b+up,Color(0x9db2b4,.18f*objectAlpha),.7f);
    canvas.Line(b+up,c+up,Color(0xb4cecb,.20f*objectAlpha),.7f);
    canvas.Line(c+up,e+up,Color(0xaec7c6,.12f*objectAlpha),.7f);
}
void Game::Ground() {
    for (int sum = -28; sum <= 29; ++sum) for (int x = -10; x <= 20; ++x) {
        int y = sum - x, tile = world.Tile(x,y); if (tile < 0) continue;
        Vec2 p = {(float)x,(float)y}, s = P(p);
        if (s.x < -160 || s.x > 1600 || s.y < -180 || s.y > 1080) continue;
        const unsigned hash = Hash(x,y);
        Color floor = tile == 2 ? Color((hash%4 == 0) ? 0x344f49 : 0x2b423f) :
            tile == 0 ? Color((hash%5 == 0) ? 0x46535a : 0x3d4a51) :
            tile == 4 ? Color((hash%4 == 0) ? 0x394650 : 0x303e48) : Color((hash%5 == 0) ? 0x3e5055 : 0x34474b);
        if (tile == 5) floor = Color(0x435056);
        Vec2 a = P(p+Vec2{-.5f,-.5f}), b = P(p+Vec2{.5f,-.5f}), c = P(p+Vec2{.5f,.5f}), d = P(p+Vec2{-.5f,.5f});
        Vec2 drop = {0,42*zoom};
        if (world.Tile(x+1,y) < 0) { canvas.Quad(b,c,c+drop,b+drop,Color(0x162a32)); canvas.Line(b+drop,c+drop,Color(0x06171f),3*zoom); }
        if (world.Tile(x,y+1) < 0) { canvas.Quad(c,d,d+drop,c+drop,Color(0x20353e)); canvas.Line(c+drop,d+drop,Color(0x06171f),3*zoom); }
        if((world.Tile(x+1,y)<0 || world.Tile(x,y+1)<0) && hash%3==0) {
            Vec2 low=s+Vec2{0,48*zoom};
            canvas.Quad(low+Vec2{-9*zoom,0},low+Vec2{9*zoom,0},low+Vec2{5*zoom,48*zoom},low+Vec2{-5*zoom,48*zoom},Color(0x1a3039));
            canvas.Line(low+Vec2{0,10*zoom},low+Vec2{0,35*zoom},Teal.Fade(.24f),2*zoom);
        }
        canvas.Quad(a,b,c,d,floor);
        canvas.Line(a,b,Color(0xa8b4b0,.09f),.65f*zoom); canvas.Line(a,d,Color(0x111f26,.55f),.8f*zoom);
        if (tile != 2 && hash%3 == 0) {
            canvas.Line(P(p+Vec2{-.32f,-.32f}),P(p+Vec2{.30f,-.32f}),Color(0x71858a,.25f),.6f*zoom);
            canvas.Line(P(p+Vec2{.30f,-.32f}),P(p+Vec2{.30f,.3f}),Color(0x172c33,.6f),.7f*zoom);
        }
        if(tile!=2 && hash%7==0) {
            for(int i=0;i<5;++i) canvas.Line(P(p+Vec2{-.23f+i*.08f,-.2f}),P(p+Vec2{-.23f+i*.08f,.2f}),Color(0x0b2630,.6f),2*zoom);
            canvas.Ellipse(P(p+Vec2{.32f,.3f}),1.1f*zoom,.7f*zoom,Color(0xa4b9b3,.5f));
        }
        if (tile == 5 && ((x+y)%2 == 0)) {
            canvas.Line(P(p+Vec2{-.28f,-.1f}),P(p+Vec2{.26f,-.1f}),Gold.Fade(.55f),2*zoom);
            canvas.Line(P(p+Vec2{-.28f,.08f}),P(p+Vec2{.26f,.08f}),Gold.Fade(.3f),1.5f*zoom);
        }
        if (tile == 2) {
            for (int i=0;i<5;++i) {
                float dx = ((hash>>(i*3))&15)/20.f-.35f, dy=((hash>>(i*3+2))&15)/20.f-.35f;
                Vec2 v=P(p+Vec2{dx,dy});
                canvas.Line(v,v+Vec2{2*zoom,-(3+i%3)*zoom},Color(i%2 ? 0x789477:0x456e5e,.7f),zoom);
            }
            if (hash%5==0) canvas.Ellipse(s+Vec2{12,-7},1.5f*zoom,1.5f*zoom,Gold.Fade(.7f));
        }
        Color edge = world.quest>=3 ? Gold.Fade(.65f) : Teal.Fade(.40f);
        if (world.Tile(x+1,y)<0) canvas.Line(b,c,edge,1.7f*zoom);
        if (world.Tile(x,y+1)<0) canvas.Line(c,d,edge,1.7f*zoom);
        if (world.Tile(x-1,y)<0) canvas.Line(a,d,Color(0x90adaf,.22f),1.2f*zoom);
        if (world.Tile(x,y-1)<0) canvas.Line(a,b,Color(0x90adaf,.22f),1.2f*zoom);
        // Low perimeter rails and their posts make the elevated colony read as architecture.
        if(tile!=5) {
            Vec2 up={0,-17*zoom};Color rail(0x8aa3a4,.46f);
            if(world.Tile(x+1,y)<0){canvas.Line(b+up,c+up,rail,1.4f*zoom);canvas.Line(b,b+up,rail,1.4f*zoom);}
            if(world.Tile(x,y+1)<0){canvas.Line(c+up,d+up,rail,1.4f*zoom);canvas.Line(d,d+up,rail,1.4f*zoom);}
        }
    }
    // Landing pad rings and inset guide lights provide a readable arrival landmark.
    Vec2 pad = P({-7.2f,.2f});
    canvas.Ellipse(pad,152*zoom,76*zoom,Gold.Fade(.4f),false,1.4f*zoom);
    canvas.Ellipse(pad,145*zoom,72.5f*zoom,Color(0xadc2c6,.18f),false,zoom);
    for (int i=0;i<12;++i) {
        float angle=i*6.283185f/12;
        Vec2 v=pad+Vec2{std::cos(angle)*152*zoom,std::sin(angle)*76*zoom};
        canvas.Ellipse(v,2*zoom,2*zoom,Gold);
    }
    // Ground light pools are drawn below depth-sorted people and structures.
    for (const auto& o:world.props) {
        if (o.kind==PropKind::Lamp || o.kind==PropKind::Relay) {
            Color c = o.kind==PropKind::Relay && !world.restored[o.variant] ? Gold : Teal;
            canvas.Glow(P(o.p),70*zoom,35*zoom,c.Fade(.14f));
        }
        if (o.kind==PropKind::Core) canvas.Glow(P(o.p),190*zoom,95*zoom,Teal.Fade(.18f));
    }
    for (const auto& o:world.props) if (o.kind!=PropKind::Memory && o.kind!=PropKind::Citizen) {
        Vec2 s=P(o.p);
        canvas.Ellipse(s+Vec2{o.h*.24f*zoom,7*zoom},(o.w*26+o.h*.22f)*zoom,(o.d*13+10)*zoom,Color(0x061216,.27f));
    }
}
void Game::Building(const Prop& o) {
    Vec2 p=o.p; float w=o.w,d=o.d,h=o.h;
    Color trim=o.variant==1?Color(0xd19b62):Color(0x6d9fa0);
    Box(p,w+.2f,d+.2f,0,9,Color(0x526164),Color(0x263a42),Color(0x1e3039));
    Box(p,w,d,9,h,Color(0x596970),Color(0x31464d),Color(0x23363e));
    Box(p,w+.18f,d+.18f,h+9,7,Color(0x6c7879),Color(0x3c545b),Color(0x2c424b));
    // Recessed strips, windows, mullions, and an accessible entrance on the front face.
    for(int i=0;i<4;++i) {
        float x=p.x-w*.4f+i*w*.22f;
        Vec2 a=P({x,p.y+d*.5f+.01f},h*.66f),b=P({x+w*.14f,p.y+d*.5f+.01f},h*.66f);
        Vec2 down={0,h*.27f*zoom};
        Poly(a,b,b+down,a+down,world.quest>=3?Color(0xe3b373,.9f):Color(0x7aafa8,.65f));
        canvas.Line(a+down*.5f,b+down*.5f,Color(0x22363d),2*zoom);
        Vec2 a2=P({p.x+w*.5f+.01f,p.y-d*.4f+i*d*.22f},h*.67f), b2=P({p.x+w*.5f+.01f,p.y-d*.4f+i*d*.22f+d*.13f},h*.67f);
        Poly(a2,b2,b2+down,a2+down,Color(0xd6b67d,.5f));
    }
    Vec2 door=P({p.x,p.y+d*.5f+.04f},8);
    Poly(door+Vec2{-13*zoom,0},door+Vec2{11*zoom,12*zoom},door+Vec2{11*zoom,-37*zoom},door+Vec2{-13*zoom,-49*zoom},Color(0x102932));
    canvas.Line(door+Vec2{-14*zoom,-49*zoom},door+Vec2{12*zoom,-36*zoom},trim,3*zoom);
    Box(p+Vec2{-.3f,-.2f},w*.55f,d*.45f,h+16,8,Color(0x344d58),Color(0x243b45),Color(0x172c35));
    for(int i=0;i<6;++i) {
        Vec2 u=P(p+Vec2{-w*.26f+i*w*.07f,-d*.2f},h+24),v=P(p+Vec2{-w*.26f+i*w*.07f,d*.12f},h+24);
        canvas.Line(u,v,Color(0x7a9a9c,.5f*objectAlpha),1.4f*zoom);
    }
    Box(p+Vec2{w*.3f,-d*.25f},.38f,.4f,h+16,14,Color(0x899799),Color(0x485f67),Color(0x304951));
    Vec2 antenna=P(p+Vec2{-w*.3f,-d*.3f},h+16);
    canvas.Line(antenna,antenna+Vec2{0,-35*zoom},Color(0x809aa0,objectAlpha),2*zoom);
    canvas.Glow(antenna+Vec2{0,-35*zoom},12*zoom,12*zoom,Gold.Fade(.5f*objectAlpha));
    if(o.kind==PropKind::Archive) {
        Vec2 sign=P(p,h*.82f);
        canvas.Rect(sign.x-58*zoom,sign.y,116*zoom,17*zoom,Color(0x071e28,.8f*objectAlpha));
        CenterText(sign.x,sign.y, L"A R C H I V E",(int)(10*zoom),Teal.Fade(objectAlpha));
    }
}
void Game::Person(Vec2 foot, int kind, float scale, bool moving, float phase) {
    float walk=moving?std::sin(world.time*10+phase):0;
    float direction=kind<2 && world.facing.x<-.1f ? -1.f:1.f;
    auto v=[&](float x,float y){return foot+Vec2{x*scale*direction,y*scale};};
    Color coat=kind==0?Color(0xc9d5d4):kind==1?Color(0xa78358):Color(0x688983);
    Color shadow=kind==0?Color(0x6e8994):kind==1?Color(0x635448):Color(0x3a555b);
    Color accent=kind==0?Teal:Gold;
    canvas.Ellipse(v(1,0),17*scale,6*scale,Color(0x031419,.4f));
    canvas.Line(v(-5,-19),v(-6+walk*3,-2),Color(0x15242f),6*scale);
    canvas.Line(v(5,-18),v(6-walk*3,-2),Color(0x263a44),6*scale);
    canvas.Line(v(-6+walk*3,-2),v(-2+walk*3,-2),Color(0xabbcbd),2*scale);
    float bob=std::abs(walk)*1.5f;
    canvas.Quad(v(-10,-46-bob),v(9,-46-bob),v(14+walk*2,-16),v(-13-walk*2,-17),shadow);
    canvas.Quad(v(-7,-46-bob),v(8,-46-bob),v(10,-20),v(-6,-24),coat);
    canvas.Triangle(v(-4,-42),v(-7,-20),v(0,-25),Color(0xeff2e4,.65f));
    canvas.Line(v(-12,-42),v(-15-walk*2,-26),shadow,5*scale);
    canvas.Line(v(11,-42),v(14+walk*2,-28),coat,5*scale);
    canvas.Ellipse(v(14+walk*2,-26),2*scale,3*scale,Color(0xc5a694));
    canvas.Rect(foot.x-9*scale,foot.y-30*scale,19*scale,3*scale,Color(0x243842));
    canvas.Rect(foot.x+3*scale,foot.y-31*scale,3*scale,4*scale,accent);
    canvas.Ellipse(v(0,-53-bob),8*scale,10*scale,Color(0xccb8a5));
    Color hair=kind==0?Color(0xe3e9e2):Color(0x26363d);
    canvas.Ellipse(v(-1,-59-bob),9*scale,6*scale,hair);
    canvas.Triangle(v(-9,-58-bob),v(-8,-44-bob),v(-2,-61-bob),hair);
    if(kind==0 || kind==2) {
        canvas.Quad(v(-8,-54),v(-4,-50),v(-6,-34),v(-12,-40),Color(0xa7c0c4));
        canvas.Triangle(v(0,-62-bob),v(8,-59-bob),v(7,-50-bob),hair);
    }
    canvas.Line(v(2,-52-bob),v(5,-52-bob),Color(0x354753),scale);
    canvas.Line(v(-7,-44-bob),v(7,-44-bob),accent,3*scale);
    canvas.Triangle(v(-7,-45-bob),v(-12,-43),v(-21-walk*3,-36),accent.Fade(.85f));
    if(kind==0) { canvas.Glow(v(11,-34),9*scale,9*scale,Teal.Fade(.6f)); canvas.Ellipse(v(10,-35),1.6f*scale,2.3f*scale,White); }
}
void Game::Portrait(float x,float y,float w,float h,int kind) {
    if(kind>2) {
        canvas.Rect(x,y,w,h,Color(0x152f39));
        Vec2 p={x+w/2,y+h*.46f};canvas.Glow(p,w*.45f,h*.4f,Teal.Fade(.2f));
        canvas.Ellipse(p,w*.32f,h*.18f,Teal.Fade(.55f),false);
        Marker(p,Teal,w*.16f);CenterText(p.x,y+h*.78f,L"SIGNAL",10,Teal);return;
    }
    // Atlas thirds match the female wanderer, captain, and colony resident.
    float u0=kind/3.f+.01f,u1=(kind+1)/3.f-.01f;
    float bottom = h/w<1.3f ? .22f : 0;
    canvas.TextureRegion(portraits,x,y,w,h,u0,bottom,u1,1);
}
void Game::DrawProp(const Prop& o) {
    Vec2 s=P(o.p);
    if (s.x<-350 || s.x>1800 || s.y<-300 || s.y>1300) return;
    objectAlpha=1;
    if((o.kind==PropKind::Habitat || o.kind==PropKind::Archive) && GameWorld::Project(o.p).y>GameWorld::Project(world.player).y) {
        Vec2 player=P(world.player,35);
        if(std::abs(player.x-s.x)<(o.w+o.d)*23*zoom && player.y>s.y-(o.h+50)*zoom && player.y<s.y+20*zoom) objectAlpha=.35f;
    }
    if(o.kind==PropKind::Habitat || o.kind==PropKind::Archive) Building(o);
    else if(o.kind==PropKind::Crate) {
        Box(o.p,o.w,o.d,0,o.h,Color(0x84918d),Color(0x52666b),Color(0x344950));
        Box(o.p,o.w+.06f,o.d+.06f,o.h-5,5,Color(0x748b88),Color(0x465d61),Color(0x30454f));
        canvas.Line(P(o.p+Vec2{-.2f,.33f},5),P(o.p+Vec2{-.2f,.33f},22),Gold.Fade(.6f),2*zoom);
    } else if(o.kind==PropKind::Lamp) {
        Box(o.p,.3f,.3f,0,5,Color(0x687e7b),Color(0x293c44),Color(0x1a3039));
        canvas.Line(s,P(o.p,o.h),Color(0x8a9e9b),3.5f*zoom);
        canvas.Line(P(o.p,o.h),P(o.p+Vec2{.21f,.21f},o.h),Color(0x96aaa6),3*zoom);
        Vec2 light=P(o.p+Vec2{.21f,.21f},o.h);
        Color color=o.variant==0?Gold:Teal;
        canvas.Glow(light,27*zoom,27*zoom,color.Fade(.36f)); canvas.Ellipse(light,3*zoom,2*zoom,White);
    } else if(o.kind==PropKind::Tree) {
        Box(o.p,.85f,.85f,0,12,Color(0x425952),Color(0x324347),Color(0x22383d));
        canvas.Line(P(o.p,12),P(o.p,o.h*.83f),Color(0x697a66),4*zoom);
        for(int i=0;i<9;++i) {
            float a=i*2.399f, radius=10+(i%3)*6.f;
            Vec2 p=P(o.p,o.h*.66f)+Vec2{std::cos(a)*radius*zoom,std::sin(a)*radius*zoom*.65f-i*2*zoom};
            Color c=i%2?Color(0x58786c):Color(0x365c52);
            canvas.Quad(p+Vec2{-16*zoom,0},p+Vec2{-3*zoom,-17*zoom},p+Vec2{19*zoom,-3*zoom},p+Vec2{6*zoom,12*zoom},c);
            canvas.Line(p+Vec2{-9*zoom,0},p+Vec2{5*zoom,-5*zoom},Color(0xa0ae86,.24f),zoom);
        }
        canvas.Ellipse(P(o.p,o.h*.78f)+Vec2{6*zoom,-2*zoom},2*zoom,2*zoom,Gold);
    } else if(o.kind==PropKind::Relay) {
        Color light=world.restored[o.variant]?Teal:Gold;
        Box(o.p,.85f,.85f,0,9,Color(0x5c7478),Color(0x344a53),Color(0x1e3944));
        Box(o.p,.42f,.42f,9,51,Color(0x698289),Color(0x49626b),Color(0x2c4956));
        Box(o.p,.7f,.65f,60,6,Color(0x88a3a6),Color(0x587982),Color(0x39596a));
        Vec2 top=P(o.p,81+std::sin(world.time*2+o.variant)*3);
        canvas.Glow(top,38*zoom,40*zoom,light.Fade(.3f)); Marker(top,light,9*zoom);
        canvas.Line(P(o.p,18),P(o.p,48),light,2*zoom);
        canvas.Ellipse(P(o.p,1),33*zoom,16.5f*zoom,light.Fade(.6f),false,zoom);
    } else if(o.kind==PropKind::Core) {
        Color c=world.quest>=3?Gold:Teal;
        for(int i=0;i<3;++i) canvas.Ellipse(P(o.p,i*7.f), (104-i*12)*zoom,(52-i*6)*zoom,Color(0x243d46));
        canvas.Ellipse(P(o.p,15),83*zoom,41.5f*zoom,c.Fade(.75f),false,1.7f*zoom);
        Box(o.p,1.25f,1.25f,18,126,Color(0x789291),Color(0x324f5b),Color(0x203b48));
        Box(o.p,.78f,.78f,144,42,Color(0xa3b7ae),Color(0x54767d),Color(0x385965));
        for(int i=0;i<3;++i) canvas.Line(P(o.p+Vec2{-.4f+i*.4f,.65f},25),P(o.p+Vec2{-.4f+i*.4f,.65f},136),c.Fade(.75f),2*zoom);
        for(int i=0;i<3;++i) canvas.Ellipse(P(o.p,112+i*33.f), (65-i*6)*zoom,(21-i*2)*zoom,c.Fade(.5f),false,1.5f*zoom);
        Vec2 top=P(o.p,218+std::sin(world.time)*5);
        canvas.Glow(top,110*zoom,130*zoom,c.Fade(.3f));
        canvas.Triangle(top+Vec2{0,-29*zoom},top+Vec2{20*zoom,0},top+Vec2{0,31*zoom},c.Fade(.8f));
        canvas.Triangle(top+Vec2{0,-29*zoom},top+Vec2{-20*zoom,0},top+Vec2{0,31*zoom},White.Fade(.7f));
        for(int i=0;i<8;++i) {
            float a=world.time*.25f+i*6.283185f/8;
            canvas.Ellipse(P(o.p,58)+Vec2{std::cos(a)*100*zoom,std::sin(a)*38*zoom},2*zoom,2*zoom,c);
        }
    } else if(o.kind==PropKind::Ship) {
        // Angled hull, inset canopy, paired engine nacelles and landing struts.
        for(float side:{-1.f,1.f}) {
            Box(o.p+Vec2{-.2f,side*.9f},1.7f,.48f,11,22,Color(0x727d80),Color(0x354b56),Color(0x263d4a));
            Vec2 engine=P(o.p+Vec2{-.95f,side*.9f},24);
            canvas.Glow(engine,40*zoom,22*zoom,Teal.Fade(.5f)); canvas.Ellipse(engine,7*zoom,5*zoom,Teal);
            canvas.Line(P(o.p+Vec2{.4f,side*.8f},0),P(o.p+Vec2{.4f,side*.8f},24),Color(0xa1aaa8),4*zoom);
        }
        Box(o.p,2.5f,1.3f,25,28,Color(0x8c9796),Color(0x526771),Color(0x314955));
        Vec2 a=P(o.p+Vec2{1.9f,0},31),b=P(o.p+Vec2{.5f,-.67f},53),d=P(o.p+Vec2{.5f,.67f},53);
        canvas.Triangle(a,b,d,Color(0x9ea8a2));
        canvas.Triangle(a,d,P(o.p+Vec2{.5f,.67f},25),Color(0x566d76));
        Box(o.p+Vec2{.18f,0},1.05f,.8f,53,18,Color(0x739d9e),Color(0x315867),Color(0x1c404f));
        canvas.Line(P(o.p+Vec2{-.25f,.41f},68),P(o.p+Vec2{.55f,.41f},68),Teal.Fade(.8f),2*zoom);
        canvas.Line(P(o.p+Vec2{-.8f,.7f},43),P(o.p+Vec2{-.2f,.7f},43),Gold,3*zoom);
    } else if(o.kind==PropKind::Citizen) {
        Person(s,2,zoom*.92f,false,(float)o.variant);
        if(world.quest==0 && o.variant==0) Marker(P(o.p,85+std::sin(world.time*2)*4),Gold,6*zoom);
    } else if(o.kind==PropKind::Memory && !world.found[o.variant]) {
        Vec2 p=P(o.p,19+std::sin(world.time*2+o.variant)*4);
        canvas.Glow(p,22*zoom,26*zoom,Gold.Fade(.3f));
        Marker(p,Gold,4*zoom);
        canvas.Ellipse(s,13*zoom,6*zoom,Gold.Fade(.35f),false);
    }
    objectAlpha=1;
}
void Game::WorldScene() {
    Ground();
    struct Item {float depth; int index;}; std::vector<Item> items;
    for(int i=0;i<(int)world.props.size();++i) items.push_back({world.props[i].p.x+world.props[i].p.y,i});
    items.push_back({world.player.x+world.player.y,-1}); items.push_back({world.captain.x+world.captain.y,-2});
    std::stable_sort(items.begin(),items.end(),[](const Item&a,const Item&b){return a.depth<b.depth;});
    for(auto item:items) {
        if(item.index>=0) DrawProp(world.props[item.index]);
        else if(item.index==-1) Person(P(world.player),0,zoom,world.walking);
        else Person(P(world.captain),1,zoom,world.captainWalking,1.2f);
    }
    canvas.Ellipse(P(world.player),20*zoom,9*zoom,Teal.Fade(.7f),false,1.1f*zoom);
    // Floating ambient dust is deterministic and does not change the playable map.
    for(int i=0;i<64;++i) {
        float x=std::fmod(i*197.3f+world.time*(3+i%4),1500.f)-30;
        float y=std::fmod(i*137.7f-world.time*(6+i%3)+10000,960.f)-30;
        float alpha=.15f+.22f*(.5f+.5f*std::sin(world.time+i));
        canvas.Ellipse({x,y},i%3==0?1.8f:1.f,i%3==0?1.8f:1.f,Color(i%4==0?0xe5bc80:0x90c6bf,alpha));
    }
    if(world.scanWave>=0) {
        float radius=world.scanWave*230*zoom;
        canvas.Ellipse(P(world.scanOrigin),radius,radius*.5f,Teal.Fade(Clamp(1-world.scanWave/2.3f,0,1)*.8f),false,2*zoom);
        for(int i=0;i<3;++i) if(!world.found[i]) canvas.Glow(P(world.memories[i],20),30,40,Gold.Fade(.3f));
    }
}
void Game::Panel(float x,float y,float w,float h,float alpha) {
    canvas.Rect(x,y,w,h,Ink.Fade(alpha));
    canvas.Line({x,y},{x+w,y},Color(0x91b4b5,.25f));
    canvas.Line({x,y+h},{x+w,y+h},Color(0x91b4b5,.15f));
    canvas.Line({x,y},{x,y+12},Gold.Fade(.7f)); canvas.Line({x,y},{x+12,y},Gold.Fade(.7f));
    canvas.Line({x+w,y+h},{x+w,y+h-12},Gold.Fade(.4f));
}
void Game::Marker(Vec2 p,Color c,float size) {
    canvas.Quad(p+Vec2{0,-size},p+Vec2{size,0},p+Vec2{0,size},p+Vec2{-size,0},c);
    canvas.Quad(p+Vec2{0,-size*.48f},p+Vec2{size*.48f,0},p+Vec2{0,size*.48f},p+Vec2{-size*.48f,0},Ink);
}
void Game::CenterText(float x,float y,const std::wstring& text,int size,Color c,bool bold) { canvas.Text(x-canvas.Measure(text,size,bold)/2,y,text,size,c,bold); }
void Game::Keycap(float x,float y,const std::wstring& key,bool active) {
    canvas.Rect(x,y,28,27,Color(active?0x365b5f:0x1b3039,.95f));
    canvas.Line({x,y+26},{x+28,y+26},active?Teal:Muted.Fade(.6f));
    CenterText(x+14,y+3,key,14,White);
}
void Game::Paragraph(float x,float y,const std::wstring& text,float width,int size,Color color,float lineHeight) {
    // Wrap at word boundaries; explicit newlines are honored for journal paragraphs.
    std::wstring line,word;
    auto append=[&]() {
        if(word.empty())return;
        if(!line.empty() && canvas.Measure(line+word,size)>width) { canvas.Text(x,y,line,size,color);y+=lineHeight;line.clear(); }
        line+=word;word.clear();
    };
    for(wchar_t ch:text) {
        if(ch==L'\n') {append();canvas.Text(x,y,line,size,color);y+=lineHeight;line.clear();}
        else {word+=ch;if(ch==L' ')append();}
    }
    append(); if(!line.empty())canvas.Text(x,y,line,size,color);
}
void Game::Map(float x,float y,float w,float h,bool full) {
    auto point=[&](Vec2 g){Vec2 p=GameWorld::Project(g);return Vec2{x+w*.31f+p.x*w/2450.f,y+h*.40f+p.y*h/1050.f};};
    for(int gx=-10;gx<=20;++gx)for(int gy=-17;gy<=5;++gy) {
        int tile=world.Tile(gx,gy);if(tile<0)continue;
        Vec2 p={(float)gx,(float)gy};
        canvas.Quad(point(p+Vec2{-.45f,-.45f}),point(p+Vec2{.45f,-.45f}),point(p+Vec2{.45f,.45f}),point(p+Vec2{-.45f,.45f}),Color(tile==2?0x46665b:tile==5?0x677571:0x42565e,.9f));
    }
    for(int i=0;i<3;++i) Marker(point(world.relays[i]),world.restored[i]?Teal:Gold,full?7.f:3.f);
    for(int i=0;i<3;++i) if(!world.found[i]) canvas.Ellipse(point(world.memories[i]),full?3.f:1.5f,full?3.f:1.5f,White.Fade(.6f));
    Marker(point({15.5f,-13.2f}),world.quest>=3?Gold:Teal,full?9.f:4.f);
    canvas.Ellipse(point(world.captain),full?4.f:2.f,full?4.f:2.f,Gold);
    Vec2 p=point(world.player);
    canvas.Glow(p,full?21.f:9.f,full?21.f:9.f,Teal.Fade(.5f));
    canvas.Triangle(p+Vec2{0,-6},p+Vec2{4,4},p+Vec2{-4,4},White);
    if(full) {
        struct Label{Vec2 p;const wchar_t* text;};
        for(const auto& label:std::vector<Label>{{{-7,4},L"01  노마드 착륙장"},{{3,6},L"02  제7 주거구역"},{{2,-17},L"03  기억의 정원"},{{17,5},L"04  기록보관소"},{{14,-20},L"05  중앙 첨탑"}}) {
            Vec2 v=point(label.p);CenterText(v.x,v.y,label.text,13,White);
        }
    }
}
void Game::HUD() {
    // Sparse typography at the edges leaves the center of the world unobstructed.
    canvas.Text(42,31,L"S T A R   D R I F T E R",15,White,true);
    canvas.Text(42,61,L"CHAPTER 01   /   아직 꺼지지 않은 별",12,Gold);
    canvas.Line({42,88},{292,88},Gold.Fade(.32f));
    canvas.Text(41,100,L"ELYSIUM",34,White,true);
    canvas.Text(43,146,world.Region()+L"  ·  통제된 행성",14,Muted);
    Panel(42,193,306,107,.8f);
    Marker({61,216},Gold,4);
    canvas.Text(75,205,L"현재의 여정",12,Gold);
    Paragraph(60,232,world.ObjectiveText(),270,16,White,26);
    canvas.Text(60,274,L"J  항해 일지    ·    M  행성 지도",11,Muted);
    Panel(1190,34,207,187,.82f);
    canvas.Text(1205,43,L"S E C T O R   0 7",11,Muted);
    canvas.Text(1363,42,L"M",12,Gold);
    Map(1199,69,187,119,false);
    canvas.Line({1205,192},{1382,192},Muted.Fade(.2f));
    canvas.Text(1205,199,L"신호 연결  "+std::to_wstring(world.RestoredCount())+L" / 3",11,Teal);
    // Quest marker stays within the safe gameplay area when its destination is off screen.
    Vec2 target=P(world.Objective(),92), screen=target;
    screen.x=Clamp(screen.x,385,1135);screen.y=Clamp(screen.y,165,714);
    if(GameWorld::Distance(world.player,world.Objective())>2) {
        canvas.Glow(screen,22,22,Gold.Fade(.15f));Marker(screen,Gold,7);
        int distance=(int)(GameWorld::Distance(world.player,world.Objective())*10);
        CenterText(screen.x,screen.y+13,std::to_wstring(distance)+L" m",12,Gold);
        if(GameWorld::Distance(screen,target)>10) {
            Vec2 d=target-screen;float len=GameWorld::Distance(d,{});d=d*(1/len);
            canvas.Line(screen+d*15,screen+d*23,Gold,1.5f);
        }
    }
    // Crew cards: protagonist and one captain, matching the reduced design scope.
    Panel(42,788,246,73,.85f);
    Portrait(54,801,43,49,0);
    canvas.Text(109,800,L"개척자",17,White,true);canvas.Text(109,829,L"기억의 공명  /  동행 중",11,Teal);
    Panel(298,788,212,73,.85f);
    Portrait(310,801,43,49,1);
    canvas.Text(365,800,L"카엘",17,White,true);canvas.Text(365,829,L"노마드의 선장",11,Gold);
    Panel(584,794,268,66,.88f);
    Keycap(601,814,L"Q",world.scanCooldown<=0);
    canvas.Text(640,804,L"공명 탐지",14,White);
    canvas.Text(640,829,world.scanCooldown<=0?L"주변의 기억과 신호를 찾습니다":L"재충전 중…",11,Muted);
    canvas.Rect(600,853,233,2,Color(0x344c53));canvas.Rect(600,853,233*(1-world.scanCooldown/5),2,Teal);
    canvas.Text(1070,804,L"WASD  이동   /   SHIFT  달리기",12,White.Fade(.85f));
    canvas.Text(1070,829,L"휠  확대·축소    ESC  일시정지",11,Muted);
    if(world.nearby>=0 && world.view==View::Explore) {
        const auto label=world.NearbyText(); float width=canvas.Measure(label,15)+66;
        Panel(720-width/2,709,width,43,.92f);Keycap(731-width/2,717,L"E",true);
        canvas.Text(769-width/2,719,label,15,White);
    }
    if(world.toastTime>0 && world.view==View::Explore) {
        float a=Clamp(world.toastTime,0,1);float width=canvas.Measure(world.toast,13)+40;
        canvas.Rect(720-width/2,89,width,37,Ink.Fade(.86f*a));
        CenterText(720,98,world.toast,13,White.Fade(a));
    }
    if(arrival<4.8f && world.view==View::Explore) {
        float a=Clamp(arrival*1.5f,0,1)*Clamp(4.8f-arrival,0,1);
        CenterText(720,190,L"E L Y S I U M",27,White.Fade(a));
        CenterText(720,231,L"아직 꺼지지 않은 별",14,Gold.Fade(a));
    }
}
void Game::Dialogue() {
    canvas.Rect(0,0,1440,900,Ink.Fade(.20f));
    Panel(80,620,1280,237,.97f);
    if(world.dialogue.empty() && world.choicePending) {
        canvas.Text(115,637,L"이 도시의 내일을 여는 방법",21,White,true);
        canvas.Text(115,673,L"어떤 선택도 남은 문제를 없애지는 못합니다. 결정권은 주민에게 돌아갑니다.",13,Muted);
        for(int i=0;i<2;++i) {
            float y=699+i*66.f;canvas.Rect(114,y,1212,55,Color(i==0?0x25413f:0x273a41));
            Keycap(130,y+14,std::to_wstring(i+1),true);
            canvas.Text(176,y+15,i==0?L"지금 문을 열고, 주민 의회에 도시의 결정권을 돌려준다.":L"생명 유지 장치를 지키며, 주민 투표로 구역을 하나씩 개방한다.",18,White);
        }
        return;
    }
    if(world.dialogue.empty())return;
    const auto& line=world.dialogue.front();
    int kind=line.speaker.find(L"카엘")!=std::wstring::npos?1:(line.speaker.find(L"세라")!=std::wstring::npos || line.speaker.find(L"주민")!=std::wstring::npos)?2:
        (line.speaker.find(L"개척자")!=std::wstring::npos?0:3);
    Portrait(101,641,123,189,kind);
    canvas.Text(253,645,line.speaker,18,Gold,true);
    canvas.Line({254,678},{1315,678},Muted.Fade(.2f));
    auto visible=line.text.substr(0,(size_t)typing);
    Paragraph(253,703,visible,1045,23,White,38);
    canvas.Text(1130,818,L"E / SPACE   계속",12,Muted);
}
void Game::Journal() {
    canvas.Rect(0,0,1440,900,Ink.Fade(.75f));Panel(230,106,980,685,.97f);
    canvas.Text(275,137,L"J O U R N A L   /   항해 일지",16,Gold);
    canvas.Text(274,180,L"아직 꺼지지 않은 별",32,White,true);
    Paragraph(275,243,L"안전을 이유로 목소리를 잃은 행성. 세라는 닫힌 창문 너머의 별을 다시 보고 싶어 한다. 카엘과 함께 중계기를 연결하고, 주민들이 자신의 내일을 선택할 수 있도록 돕자.",850,18,Muted,31);
    canvas.Line({276,349},{1160,349},Muted.Fade(.25f));
    canvas.Text(275,374,L"현재 목표",13,Gold);canvas.Text(275,403,world.ObjectiveText(),21,White);
    canvas.Text(275,474,L"기억의 잔향   "+std::to_wstring(world.MemoryCount())+L" / 3",14,Gold);
    const wchar_t* notes[]={L"항해 기록 — 이름을 잊어도, 내민 손은 누군가 기억한다.",L"정원의 편지 — 기다리는 것도 내가 고른 일이다.",L"삭제된 명령 — 보호는 선택권을 대체하지 않는다."};
    for(int i=0;i<3;++i) canvas.Text(288,518+i*47.f,world.found[i]?notes[i]:L"아직 발견하지 못한 기록",17,world.found[i]?White:Muted.Fade(.55f));
    canvas.Text(275,724,L"동행  카엘  ·  노마드의 선장",14,Muted);canvas.Text(1000,730,L"J / ESC  닫기",12,Gold);
}
void Game::Title() {
    // Gradient is built from translucent bands so the illustrated sky remains visible.
    for(int i=0;i<72;++i) canvas.Rect(i*20.f,0,20,900,Ink.Fade(.78f*(1-i/72.f)));
    canvas.Text(106,95,L"A JOURNEY TOWARD OUR OWN TOMORROW",13,Gold);
    canvas.Line({108,134},{188,134},Gold,.9f);
    canvas.Text(100,263,L"STAR",84,White,true);
    canvas.Text(100,349,L"DRIFTER",84,White,true);
    canvas.Text(107,462,L"기억을 잃어도, 내일은 선택할 수 있다.",23,White);
    canvas.Text(108,510,L"CHAPTER 01    /    아직 꺼지지 않은 별",14,Gold);
    Panel(106,581,338,67,.6f);
    canvas.Text(130,600,world.quest>0?L"여정 계속":L"여정 시작",21,White);canvas.Text(358,608,L"ENTER  →",12,Gold);
    canvas.Text(108,674,L"WASD  탐험   ·   E  대화   ·   Q  공명 탐지",13,Muted);
    canvas.Text(108,701,L"M  지도   ·   J  항해 일지   ·   휠  확대·축소",13,Muted);
    canvas.Text(106,836,L"2.5D EXPLORATION RPG",11,Muted);
    canvas.Text(1127,836,L"FIRST CONTACT  /  v0.2",11,Muted);
}
void Game::Ending() {
    canvas.Rect(0,0,1440,900,Ink.Fade(.66f));
    CenterText(720,167,L"C H A P T E R   C O M P L E T E",15,Gold);
    CenterText(720,236,L"창문 너머의 별",49,White,true);
    CenterText(720,327,world.choice==0?L"도시의 문이 열리고, 첫 번째 시민 의회가 시작되었습니다.":L"주민들이 투표한 첫 구역의 문이 열렸습니다. 다음 개방도 그들이 정합니다.",19,White);
    CenterText(720,373,L"모든 문제가 사라진 것은 아닙니다. 하지만 이제, 내일을 고르는 목소리가 있습니다.",18,Muted);
    canvas.Line({570,433},{870,433},Gold.Fade(.4f));
    CenterText(720,473,L"“네가 누구였는지보다, 지금 어디로 가고 싶은지가 더 궁금해.”",21,White);
    CenterText(720,520,L"— 카엘, 노마드의 선장",14,Gold);
    CenterText(720,583,L"복구한 중계기  3 / 3       되찾은 기억  "+std::to_wstring(world.MemoryCount())+L" / 3",14,Muted);
    CenterText(720,697,L"ENTER   새로운 여정",16,Gold);
    CenterText(720,805,L"첫 행성의 이야기는 여기까지입니다. 다음 행성은 후속 개발 범위입니다.",13,Muted);
}
void Game::Render() {
    renderer.BeginSceneCapture();glDisable(GL_DEPTH_TEST);glClearColor(.025f,.045f,.06f,1);glClear(GL_COLOR_BUFFER_BIT);
    float parallaxX=std::sin(world.time*.025f)*8-camera.x*.018f;
    canvas.Texture(sky,-80+parallaxX,-65,1600,1000,Color(0xffffff,1));
    if(world.view!=View::Title && world.view!=View::Ending) {
        canvas.Rect(0,0,1440,900,Ink.Fade(.28f));
        WorldScene();
    }
    canvas.Flush();renderer.EndSceneCaptureAndComposite(.25f,.27f,0.f);
    if(world.view==View::Title) Title();
    else if(world.view==View::Ending) Ending();
    else {
        HUD();
        if(world.view==View::Dialogue) Dialogue();
        else if(world.view==View::Map) {
            canvas.Rect(0,0,1440,900,Ink.Fade(.72f));Panel(175,92,1090,715,.98f);
            canvas.Text(218,123,L"E L Y S I U M   /   행성 지도",22,White,true);
            canvas.Text(220,170,world.ObjectiveText(),16,Gold);
            Map(229,240,968,418,true);
            canvas.Line({220,707},{1215,707},Muted.Fade(.2f));
            Marker({227,744},Gold,5);canvas.Text(245,734,L"미복구 신호",13,Muted);
            Marker({401,744},Teal,5);canvas.Text(419,734,L"연결된 신호",13,Muted);
            canvas.Ellipse({585,744},3,3,White);canvas.Text(601,734,L"기억의 잔향",13,Muted);
            canvas.Text(1093,734,L"M / ESC  닫기",12,Gold);
        } else if(world.view==View::Journal) Journal();
        else if(world.view==View::Pause) {
            canvas.Rect(0,0,1440,900,Ink.Fade(.72f));Panel(488,298,464,303,.96f);
            CenterText(720,332,L"잠시, 별을 바라보며",27,White,true);
            CenterText(720,398,L"ENTER / ESC    여정 계속",17,Gold);
            CenterText(720,447,L"T    타이틀로 돌아가기",16,Muted);
            CenterText(720,528,L"지도와 일지를 열면 탐험 시간이 멈춥니다.",12,Muted);
        }
    }
    canvas.Flush();
}
