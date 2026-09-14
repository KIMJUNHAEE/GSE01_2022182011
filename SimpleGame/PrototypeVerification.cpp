#include "stdafx.h"
#include "Game.h"
#include "Dependencies/freeglut.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <queue>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace {
void Require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void Capture(Game& game,int width,int height,const std::filesystem::path& file) {
    game.Render();glFinish();
    Require(glGetError()==GL_NO_ERROR,"OpenGL error while rendering verification frame");
    std::vector<unsigned char> pixels(width*height*4);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,width,height,GL_BGRA,GL_UNSIGNED_BYTE,pixels.data());
    Require(glGetError()==GL_NO_ERROR,"OpenGL framebuffer readback failed");
    int lit=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]+pixels[i+1]+pixels[i+2]>70)++lit;
    Require(lit>width*height/20,"Rendered framebuffer is blank or nearly black");
    BITMAPFILEHEADER header={};header.bfType=0x4d42;header.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+(DWORD)pixels.size();
    BITMAPINFOHEADER info={};info.biSize=sizeof(info);info.biWidth=width;info.biHeight=height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    std::ofstream out(file,std::ios::binary);out.write((char*)&header,sizeof(header));out.write((char*)&info,sizeof(info));out.write((char*)pixels.data(),pixels.size());
    Require(out.good(),"Could not write verification screenshot");
}
void FinishDialogue(GameWorld& w) {for(int n=0;n<20 && !w.dialogue.empty();++n)w.Advance();}
// Routes use the same collision-tested movement function as keyboard input.
void WalkTo(GameWorld& w,Vec2 target,float reach = 1.3f) {
    const int nx=129,ny=97;const float x0=-11,y0=-18,step=.25f;
    auto point=[&](int n){return Vec2{x0+(n%nx)*step,y0+(n/nx)*step};};
    auto index=[&](Vec2 p){return (int)std::round((p.y-y0)/step)*nx+(int)std::round((p.x-x0)/step);};
    std::vector<int> parent(nx*ny,-1);std::queue<int> q;
    int start=index(w.player),end=-1;Require(start>=0 && start<nx*ny,"Invalid route start");q.push(start);parent[start]=start;
    while(!q.empty()) {
        int cur=q.front();q.pop();Vec2 p=point(cur);
        if(GameWorld::Distance(p,target)<reach){end=cur;break;}
        for(int d:{1,-1,nx,-nx}) {
            int n=cur+d;if(n<0||n>=nx*ny||parent[n]!=-1)continue;
            if(std::abs((n%nx)-(cur%nx))+std::abs(n/nx-cur/nx)!=1)continue;
            if(!w.Walkable(point(n),.27f))continue;
            parent[n]=cur;q.push(n);
        }
    }
    Require(end>=0,"A quest or memory target is unreachable");
    std::vector<int> path;for(int n=end;n!=start;n=parent[n])path.push_back(n);std::reverse(path.begin(),path.end());
    for(int n:path) {
        Vec2 goal=point(n);int guard=0;
        while(GameWorld::Distance(w.player,goal)>.025f && guard++<100) {
            Vec2 d=GameWorld::Project(goal)-GameWorld::Project(w.player);
            float remaining=GameWorld::Distance(d,{});float dt=(std::min)(1/120.f,remaining/155.f);
            w.Move(d,dt,false);w.Update(dt);Require(w.Walkable(w.player),"Player penetrated a collider");
        }
        if(guard>=100) throw std::runtime_error("Movement stalled near target " + std::to_string(target.x) + "," + std::to_string(target.y) + " at " + std::to_string(w.player.x) + "," + std::to_string(w.player.y));
    }
    w.Move({},0,false);w.Update(.016f);
    for(int i=0;i<180 && GameWorld::Distance(w.player,w.captain)>1.4f;++i)w.Update(1/60.f);
    Require(GameWorld::Distance(w.player,w.captain)<2.f,"Companion failed to follow the collision-tested route");
}
}
int RunPrototypeVerification(Game& game,Renderer& renderer,int width,int height,const std::filesystem::path& output) {
    std::filesystem::create_directories(output);std::ofstream report(output/"verification.txt");
    try {
        auto& w=game.world;
        Require(w.Walkable(w.player),"Spawn intersects geometry");Require(w.Walkable(w.captain,.15f),"Companion spawn intersects geometry");
        Capture(game,width,height,output/"01-title.bmp");report<<"PASS title rendering and Unicode text\n";
        game.Key(13,true);game.Key(13,false);Require(w.view==View::Explore,"Title did not enter exploration");
        for(int i=0;i<320;++i)game.Tick(.016f);
        Capture(game,width,height,output/"02-arrival.bmp");
        game.Key('m',true);Capture(game,width,height,output/"03-map.bmp");
        float paused=w.time;for(int i=0;i<60;++i)game.Tick(.016f);Require(w.time==paused,"Map did not pause gameplay");game.Key('m',true);
        GameWorld a,b,c;a.Start();b.Start();c.Start();a.player=b.player=c.player={-8,3};
        for(int i=0;i<20;++i)a.Move({1,0},.005f,false);
        for(int i=0;i<10;++i)b.Move({1,0},.01f,false);
        for(int i=0;i<10;++i)c.Move({1,-1},.01f,false);
        Require(GameWorld::Distance(a.player,b.player)<.002f,"Movement is frame-rate dependent");
        float straight=GameWorld::Distance(GameWorld::Project(a.player),GameWorld::Project({-8,3}));
        float diagonal=GameWorld::Distance(GameWorld::Project(c.player),GameWorld::Project({-8,3}));
        Require(std::abs(straight-diagonal)<.05f,"Diagonal movement is faster than axial movement");
        Require(!w.Walkable({-7.4f,.2f}) && !w.Walkable({-11,0}),"Building or world boundary collision failed");
        report<<"PASS timestep-independent normalized movement, collisions, map pause\n";
        WalkTo(w,{2.4f,.8f});game.SnapCamera();w.Interact();Require(w.quest==1,"Resident interaction did not start the quest");
        for(int i=0;i<200;++i)game.Tick(.02f);
        Capture(game,width,height,output/"04-dialogue.bmp");FinishDialogue(w);
        for(int i=0;i<3;++i) {WalkTo(w,w.memories[i]);w.Interact();FinishDialogue(w);Require(w.found[i],"Memory interaction failed");}
        for(int i=0;i<3;++i) {
            WalkTo(w,w.relays[i]);w.Interact();Require(w.restored[i],"Relay interaction failed");FinishDialogue(w);
            if(i==0){game.SnapCamera();w.Scan();game.Tick(.03f);Capture(game,width,height,output/"05-garden.bmp");}
        }
        Require(w.quest==2 && w.RestoredCount()==3 && w.MemoryCount()==3,"Quest counters are inconsistent");
        report<<"PASS collision-tested travel and companion following to all relays and memories\n";
        w.Scan();float cooldown=w.scanCooldown;w.Scan();Require(w.scanCooldown==cooldown,"Scanner cooldown was bypassed");
        WalkTo(w,{15.5f,-13.2f});game.SnapCamera();w.Interact();FinishDialogue(w);
        Require(w.choicePending && w.view==View::Dialogue,"Core choice did not open");Capture(game,width,height,output/"06-choice.bmp");
        GameWorld alternate=w;alternate.Choose(1);FinishDialogue(alternate);Require(alternate.quest==3 && alternate.choice==1,"Gradual-opening branch failed");
        w.Choose(0);FinishDialogue(w);Require(w.quest==3 && w.choice==0,"Immediate-opening branch failed");
        w.Scan();game.Tick(.03f);Capture(game,width,height,output/"07-restored.bmp");
        game.Key('j',true);Capture(game,width,height,output/"08-journal.bmp");game.Key('j',true);
        WalkTo(w,{-7.4f,.2f},2.1f);w.Interact();Require(w.view==View::Ending && w.quest==4,"Returning to the ship did not complete the chapter");
        Capture(game,width,height,output/"09-ending.bmp");
        report<<"PASS both narrative choices, journal and completed chapter\n";
        game.Key(13,true);Require(w.view==View::Title && w.quest==0,"Restart did not reset quest");
        glutReshapeWindow(1000,740);glutMainLoopEvent();renderer.SetOutputSize(1000,740);
        Capture(game,1000,740,output/"10-resize.bmp");
        report<<"PASS clean restart and non-16:10 resize\nALL CHECKS PASSED\n";
        report.flush();return 0;
    } catch(const std::exception& e) {report<<"FAIL: "<<e.what()<<"\n";report.flush();throw;}
}
