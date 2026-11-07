void Hud::drawPayload(modules::Context& context,bool edit) {
    const auto& state=*context.state;auto& settings=modules::modules().config();auto& gfx=render::renderer();auto& game=game::Game::instance();
    if(settings.getInt("payload.hud","Revision",0)<1){
        constexpr const char* oldNames[]{"Watermark","Module list","Coordinates","Player info"};constexpr int ids[]{0,2,3,16};
        for(int n=0;n<4;++n)if(!settings.get("HUD",oldNames[n],"").empty())settings.setBool("payload.hud",payloadHuds[ids[n]].key,settings.getBool("HUD",oldNames[n],false));
        settings.setInt("payload.hud","Revision",1);
    }
    if(settings.getInt("payload.hud","Revision",0)<2){
        if(settings.get("payload.hud","Info","").empty()&&!settings.get("HUD","Info panel","").empty())settings.setBool("payload.hud","Info",settings.getBool("HUD","Info panel",false));
        settings.setInt("payload.hud","Revision",2);
    }
    const auto defaultFontRole=krux::fonts::preference(settings,krux::fonts::Domain::Hud)==1?render::FontRole::PixelScaled:render::FontRole::Hud;
    const double now=nowSeconds(),speed=std::sqrt(state.player.velocity.x*state.player.velocity.x+state.player.velocity.y*state.player.velocity.y+state.player.velocity.z*state.player.velocity.z)*20.;
    const auto enabled=[&](int i){return settings.getBool("payload.hud",payloadHuds[i].key,i==0);};
    const double refresh=std::clamp(double(settings.getNumber("payload.hud","Refresh interval",.1f)),.05,1.);
    if(payloadSampled_<0||now-payloadSampled_>=refresh){
        const double elapsed=payloadSampled_<0?refresh:std::max(.001,now-payloadSampled_);
        payloadAcceleration_=payloadSampled_<0?0:clamp(float(.5*(speed-payloadPreviousSpeed_)/elapsed+.5*payloadAcceleration_),-50.f,50.f);payloadPreviousSpeed_=speed;payloadSmoothedSpeed_=payloadSampled_<0?speed:.5*speed+.5*payloadSmoothedSpeed_;
        if(context.env&&enabled(3))payloadDimension_=bridge::utilitySnapshot(context.env,685,3);
        if(context.env&&(enabled(7)||enabled(8))){std::istringstream values(bridge::utilitySnapshot(context.env,685,2));payloadStorageValid_=bool(values>>payloadChests_>>payloadShulkers_);}
        else if(context.env){bridge::utilitySnapshot(context.env,685,-2);payloadStorageValid_=false;}
        if(context.env&&(enabled(12)||enabled(13))){std::istringstream values(bridge::utilitySnapshot(context.env,685,0));long long rx=0,tx=0;if(values>>rx>>tx){payloadRx_=float(std::max(0LL,rx-payloadReceived_)/elapsed);payloadTx_=float(std::max(0LL,tx-payloadSent_)/elapsed);payloadReceived_=rx;payloadSent_=tx;}}
        if(context.env&&!enabled(12)&&!enabled(13))bridge::utilitySnapshot(context.env,685,-1);
        const float samples[]{float(payloadSmoothedSpeed_),payloadAcceleration_,payloadTx_,payloadRx_};for(int n=0;n<4;++n){auto& history=payloadHistory_[n];history.push_back(samples[n]);while(history.size()>120)history.pop_front();}payloadSampled_=now;
    }
    const auto number=[](double value,const char* suffix){char text[96];std::snprintf(text,sizeof(text),"%.1f%s",value,suffix);return std::string(text);};
    for(int i=0;i<int(payloadHuds.size());++i){
        if(i==11||!enabled(i)){payloadBounds_[i]={};continue;}
        const std::string key=payloadHuds[i].key,section="payload."+key;
        const int fontChoice=std::clamp(settings.getInt(section,"Font",0),0,5),shadowChoice=std::clamp(settings.getInt(section,"Text shadow",0),0,2);
        const auto fontRole=fontChoice==1?render::FontRole::HudSmooth:fontChoice==2?render::FontRole::PixelScaled:fontChoice==3?render::FontRole::RicoPixel:fontChoice==4?render::FontRole::Arial:fontChoice==5?render::FontRole::OutlinedPixel:defaultFontRole;
        const float unit=std::max(.25f,unit_*clamp(settings.getNumber(section,"Scale",1),.5f,3.f)),font=9*unit;
        const bool vertical=settings.getBool(section,"Vertical",false),background=settings.getBool(section,"Background",false),gradient=settings.getBool(section,"Gradient",false);
        const Color colour{settings.getNumber(section,"Red",1),settings.getNumber(section,"Green",1),settings.getNumber(section,"Blue",1),settings.getNumber(section,"Opacity",1)};
        const float defaultX=i==1||i==2?std::max(0.f,float(state.windowWidth)-170*unit):12.f+float(i%4)*55*unit;
        const float defaultY=i==0?12.f:38.f+float(i/4)*34*unit;
        float x=settings.getNumber("payload.layout",key+"X",defaultX),y=settings.getNumber("payload.layout",key+"Y",defaultY);
        std::vector<std::string> lines;float width=0,height=9*unit;
        switch(i){
            case 0:lines={settings.get(section,"Text","krux.lol")};break;
            case 1:case 2:{for(const auto& module:modules::modules().all())if(module->enabled()&&module->name()!="HUD"&&module->name()!="ClickGUI"){std::string text=module->name();if(i==2&&!module->suffix().empty())text+=" ["+module->suffix()+"]";lines.push_back(text);}std::sort(lines.begin(),lines.end(),[&](const auto& a,const auto& b){return gfx.measure(fontRole,a,font)>gfx.measure(fontRole,b,font);});if(lines.empty())lines={edit?"No active modules":""};break;}
            case 3:{char value[100];std::snprintf(value,sizeof(value),settings.getBool(section,"Show label",true)?"XYZ %.1f / %.1f / %.1f":"%.1f / %.1f / %.1f",state.player.pos.x,state.player.pos.y,state.player.pos.z);lines={value};if(settings.getBool(section,"Nether coordinates",true)){const bool toNether=settings.getBool(section,"Auto dimension",true)?payloadDimension_!="minecraft:the_nether":settings.getBool(section,"Overworld to Nether",true);const double divisor=toNether?8.:.125;std::snprintf(value,sizeof(value),settings.getBool(section,"Show label",true)?"Linked %.1f / %.1f":"%.1f / %.1f",state.player.pos.x/divisor,state.player.pos.z/divisor);lines.push_back(value);}break;}
            case 4:{constexpr const char* compass[]{"South (+Z)","Southwest","West (-X)","Northwest","North (-Z)","Northeast","East (+X)","Southeast"};double angle=std::fmod(double(state.player.yaw),360.);if(angle<0)angle+=360.;const int heading=int(std::floor((angle+22.5)/45))%8;lines={compass[heading]};break;}
            case 5:lines={number(payloadSmoothedSpeed_*(settings.getBool(section,"km/h",false)?3.6:1),settings.getBool(section,"km/h",false)?" km/h":" b/s")};break;
            case 7:lines={(settings.getBool(section,"Show label",true)?settings.getBool(section,"Double equivalents",false)?std::string("Dub equivalents "):std::string("Chests "):std::string())+(payloadStorageValid_?(settings.getBool(section,"Double equivalents",false)?number(payloadChests_*.5,""):std::to_string(payloadChests_)):"n/a")};break;
            case 8:lines={(settings.getBool(section,"Show label",true)?std::string("Shulkers "):std::string())+(payloadStorageValid_?std::to_string(payloadShulkers_):"n/a")};break;
            case 9:{char text[120];std::snprintf(text,sizeof(text),settings.getBool(section,"Show label",true)?"XYZ %.1f / %.1f / %.1f":"%.1f / %.1f / %.1f",settings.getNumber(section,"X",0),settings.getNumber(section,"Y",64),settings.getNumber(section,"Z",0));lines={text};break;}
            case 10:
                if(settings.getBool(section,"Speed",true))lines.push_back("Speed "+number(std::hypot(state.player.velocity.x,state.player.velocity.z)*20*(settings.getBool(section,"km/h",false)?3.6:1),settings.getBool(section,"km/h",false)?" km/h":" b/s"));
                if(settings.getBool(section,"Vertical",true))lines.push_back("Vertical "+number(state.player.velocity.y*20," b/s"));
                if(settings.getBool(section,"FPS",true))lines.push_back("FPS "+std::to_string(state.fps));
                if(settings.getBool(section,"Ping",true))lines.push_back("Ping "+(state.ping<0?std::string("--"):std::to_string(state.ping)+" ms"));break;
            case 12:lines={number(payloadTx_," packets/s")};break;
            case 13:lines={number(payloadRx_," packets/s")};break;
            case 14:lines={number(payloadAcceleration_," b/sÂ²")};break;
            case 15:lines={number(payloadSmoothedSpeed_," b/s")};break;
            case 18:{char text[32];if(settings.getBool(section,"Real clock",false)){const std::time_t stamp=std::time(nullptr);std::tm time{};localtime_s(&time,&stamp);std::strftime(text,sizeof(text),settings.getBool(section,"Seconds",true)?"%H:%M:%S":"%H:%M",&time);}else{const long long ticks=((state.worldTime%24000)+30000)%24000;const int hours=int(ticks/1000),minutes=int(ticks%1000*60/1000);std::snprintf(text,sizeof(text),"%02d:%02d%s",hours%12?hours%12:12,minutes,hours>=12?"PM":"AM");}lines={text};break;}
            case 19:{for(const auto& module:modules::modules().all())if(module->enabled()&&module->name()!="HUD"&&module->name()!="ClickGUI")lines.push_back(module->name());std::sort(lines.begin(),lines.end());constexpr const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";for(auto& value:lines)for(std::size_t c=0;c<value.size();++c)if(value[c]!=' ')value[c]=alphabet[(std::uint64_t(now*12)+c*17)%36];if(lines.empty()&&edit)lines={"No active modules"};break;}

        }
        for(const auto& line:lines)width=std::max(width,gfx.measure(fontRole,line,font));height=std::max(9*unit,float(lines.size())*10*unit);
        if(i==6){width=vertical?37*unit:96*unit;height=vertical?104*unit:25*unit;}
        if(i>=12&&i<=15&&settings.getBool(section,"Graph",true)){width=150*unit;height=55*unit;}
        if(i==16){width=42*unit;height=82*unit;}
        if(i==17){width=68*unit;height=24*unit;if(!edit){const float travelX=std::max(1.f,state.windowWidth-width),travelY=std::max(1.f,state.windowHeight-height);const auto bounce=[](double t,float length){const float value=float(std::fmod(t,double(length)*2));return value<=length?value:2*length-value;};x=bounce(now*37,travelX);y=bounce(now*23,travelY);}}
        width=std::min(float(state.windowWidth),std::max(width,10*unit));
        const bool alignRight=settings.getBool("payload.layout",key+"Right",x+width*.5f>state.windowWidth*.5f);
        if(settings.get("payload.layout",key+"Right","").empty()){
            settings.setBool("payload.layout",key+"Right",alignRight);
            settings.setNumber("payload.layout",key+"RightGap",std::max(0.f,state.windowWidth-x-width),1);
        }
        if(alignRight&&i!=17)x=state.windowWidth-width-settings.getNumber("payload.layout",key+"RightGap",12);
        height=std::min(float(state.windowHeight),height);x=clamp(x,0.f,std::max(0.f,float(state.windowWidth)-width));y=clamp(y,0.f,std::max(0.f,float(state.windowHeight)-height));payloadBounds_[i]={x,y,width,height};
        if(background)gfx.rect({x,y,width,height},Color::rgb(0,.55f*colour.a),0);
        const auto paint=[&](const std::string& value,float tx,float ty,const Color& ink){
            if(shadowChoice==0){drawHudText(fontRole,value,tx,ty,font,ink,render::Align::Left,true,unit);return;}
            const bool previous=gfx.setTextShadow(shadowChoice==2);
            if(shadowChoice==2&&fontRole!=render::FontRole::PixelScaled)gfx.textCap(fontRole,value,tx+unit,ty+unit,font,{ink.r*.25f,ink.g*.25f,ink.b*.25f,ink.a});
            gfx.textCap(fontRole,value,tx,ty,font,ink);gfx.setTextShadow(previous);
        };
        const auto text=[&](const std::string& value,float tx,float ty){if(!gradient){paint(value,tx,ty,colour);return;}float pen=tx;for(std::size_t c=0;c<value.size();){std::size_t cursor=c;render::decodeUtf8(value,cursor);const std::string glyph=value.substr(c,cursor-c);const auto ink=Color::mix(colour,hudPalette().accent,float(c)/std::max(1.f,float(value.size()-1)));paint(glyph,pen,ty,ink);pen+=gfx.measure(fontRole,glyph,font);c=cursor;}};
        for(std::size_t n=0;n<lines.size();++n){const auto line=gfx.elide(fontRole,lines[n],font,width);text(line,x+(alignRight?width-gfx.measure(fontRole,line,font):0),y+float(n)*10*unit);}
        if(i==6&&context.env){for(int slot=0;slot<4;++slot){const auto item=game.equipment(context.env,state.player.handle,game::Game::Slot(slot));if(!item.present)continue;const float ix=x+(vertical?0:float(slot)*24*unit),iy=y+(vertical?float(slot)*26*unit:0);const auto sprite=game.itemSprite(context.env,item.id);if(sprite.valid)gfx.imageCrisp({ix,iy,16*unit,16*unit},{sprite.u0,sprite.v0,sprite.u1-sprite.u0,sprite.v1-sprite.v0},sprite.texture,colour,0);if(item.maxDamage>0&&settings.getBool(section,"Durability",true))text(std::to_string(std::max(0,item.maxDamage-item.damage)),ix,iy+16*unit);}}
        if(i>=12&&i<=15&&settings.getBool(section,"Graph",true)){const int h=i==12?2:i==13?3:i==14?1:0;const auto& values=payloadHistory_[h];float magnitude=1;for(float value:values)magnitude=std::max(magnitude,std::abs(value));if(values.size()>1)for(std::size_t n=1;n<values.size();++n){const float x0=x+width*float(n-1)/119.f,x1=x+width*float(n)/119.f;const float graphHeight=std::max(unit,height-15*unit),baseline=y+height-2*unit-(h==1?graphHeight*.5f:0),range=h==1?graphHeight*.5f:graphHeight;const float y0=baseline-(h==1?values[n-1]:std::max(0.f,values[n-1]))/magnitude*range,y1=baseline-(h==1?values[n]:std::max(0.f,values[n]))/magnitude*range;gfx.line(x0,y0,x1,y1,unit,colour);}gfx.line(x,y+height-1,x+width,y+height-1,1,colour.withAlpha(.4f));}
        if(i==16&&context.env){const auto skin=game.playerSkin(context.env,state.player.handle);if(skin.texture){render::skinModel(skin,payloadBounds_[i],180-state.player.yaw,state.player.pitch,colour.a,float(now));}else if(edit)text("Skin loading",x,y);}
        if(i==17)text("krux.lol",x,y+7*unit);
    }
}