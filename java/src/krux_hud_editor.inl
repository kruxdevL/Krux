void RenderHudEditor() {
    using cold::ui::payloadHuds;using cold::ui::payloadHudFirst;
    auto& config=cold::modules::modules().config();auto& hud=cold::ui::client().hud();const auto size=ImGui::GetIO().DisplaySize;
    static int expanded=-1,dragged=-1,scrollTo=-1;static ImVec2 dragOffset{},headerOffset{};static bool moving=false;
    const float width=std::min(g_classicColumnWidth,size.x-16),headerHeight=g_classicHeaderHeight*g_textScale,rowHeight=g_moduleRowHeight*g_textScale;
    static float contentHeight=21*rowHeight;bool folded=config.getBool("interface","payloadEditorCollapsed",false);
    const float height=folded?headerHeight:std::min(std::max(headerHeight+36,headerHeight+contentHeight+3),size.y-64);
    ImVec2 position{config.getNumber("interface","payloadEditorX",12),config.getNumber("interface","payloadEditorY",52)};
    if(!ImGui::IsMouseDown(0)){moving=false;dragged=-1;}
    if(moving)position=ImGui::GetIO().MousePos-headerOffset;
    position.x=std::clamp(position.x,0.f,std::max(0.f,size.x-width));position.y=std::clamp(position.y,0.f,std::max(0.f,size.y-height));
    if(moving){config.setNumber("interface","payloadEditorX",position.x,0);config.setNumber("interface","payloadEditorY",position.y,0);}
    const auto mouse=ImGui::GetIO().MousePos;const bool overInspector=mouse.x>=position.x&&mouse.x<=position.x+width&&mouse.y>=position.y&&mouse.y<=position.y+height;
    const ImVec2 done{(size.x-74)*.5f,size.y-34};const bool overDone=mouse.x>=done.x&&mouse.x<=done.x+74&&mouse.y>=done.y&&mouse.y<=done.y+24;
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(size);
    ImGui::Begin("##hud-editor",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus);
    const auto elements=hud.editableElements();
    const float canvasWidth=float(std::max(1,cold::render::renderer().width())),canvasHeight=float(std::max(1,cold::render::renderer().height()));const ImVec2 scale{size.x/canvasWidth,size.y/canvasHeight};
    const bool snapping=config.getBool("interface","hudSnapping",true);
    for(const auto& element:elements){
        const ImVec2 at{element.bounds.x*scale.x,element.bounds.y*scale.y},extent{element.bounds.width*scale.x,element.bounds.height*scale.y};
        ImGui::PushID(element.id);ImGui::SetCursorScreenPos(at);ImGui::InvisibleButton("##element",extent);
        if(!overInspector&&!overDone&&ImGui::IsItemClicked(1)){expanded=element.id==11?11:element.id>=payloadHudFirst?element.id-payloadHudFirst:-1;scrollTo=expanded;PlayMenuClick();}
        if(!overInspector&&!overDone&&ImGui::IsItemClicked(0)){dragged=element.id;dragOffset=mouse-at;}
        if(dragged==element.id&&ImGui::IsMouseDown(0)){const auto desired=mouse-dragOffset;std::vector<cold::Rect> others;for(const auto& other:elements)if(other.id!=element.id)others.push_back(other.bounds);const auto snapped=cold::ui::snapHudElement({desired.x/scale.x,desired.y/scale.y,element.bounds.width,element.bounds.height},others,canvasWidth,canvasHeight,6.f/std::max(.1f,scale.x),snapping&&!ImGui::GetIO().KeyShift);hud.moveElement(element.id,snapped.x,snapped.y,canvasWidth);}
        if(!overInspector&&!overDone&&(dragged==element.id||ImGui::IsItemHovered())){auto* draw=ImGui::GetWindowDrawList();draw->AddRect(at,at+extent,MenuAccent(200),0);draw->AddText({at.x,std::max(0.f,at.y-ImGui::GetFontSize()-2)},MenuAccent(),element.label);}
        ImGui::PopID();
    }
    if(DrawPrimordialButton("##hud-done","Done",done,{74,24})){g_menuPage=1;SetHudEditing(false);}
    ImGui::End();
    ImGui::SetNextWindowPos(position,ImGuiCond_Always);ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{1,0});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,0});
    ImGui::Begin("##hud-elements",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
    ImGui::SetWindowFontScale(g_textScale);auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(position,position+ImVec2(width,height),MenuBody());
    const auto header=cold::Color::mix(g_headerColour,cold::ui::theme().accent,.4f);
    draw->AddRectFilled(position,position+ImVec2(width,headerHeight),ImGui::ColorConvertFloat4ToU32({header.r,header.g,header.b,g_menuPanelOpacity}));
    draw->AddRect(position+ImVec2(.5f,.5f),position+ImVec2(width-.5f,height-.5f),MenuOutline(180));
    draw->AddLine(position+ImVec2(1,headerHeight),position+ImVec2(width-1,headerHeight),MenuOutline(150));
    draw->AddText(position+ImVec2(6,(headerHeight-ImGui::GetFontSize())*.5f),IM_COL32(238,234,237,255),"HUD");
    draw->AddText(position+ImVec2(width-13,(headerHeight-ImGui::GetFontSize())*.5f),IM_COL32(218,212,216,255),folded?"+":"-");
    ImGui::SetCursorScreenPos(position);ImGui::InvisibleButton("##drag-header",{width,headerHeight});
    if(ImGui::IsItemClicked(1)||(ImGui::IsItemClicked(0)&&mouse.x>=position.x+width-22)){folded=!folded;config.setBool("interface","payloadEditorCollapsed",folded);}
    else if(ImGui::IsItemClicked(0)){moving=true;headerOffset=mouse-position;}
    if(!folded){
    ImGui::SetCursorScreenPos(position+ImVec2(1,headerHeight+1));ImGui::BeginChild("##hud-elements-scroll",{width-2,height-headerHeight-2},false,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar);
    const float contentStart=ImGui::GetCursorPosY();
    auto* hudModule=cold::modules::modules().find("HUD");auto* notices=hudModule?hudModule->setting("Notifications"):nullptr;
    const auto moduleRow=[&](const char* id,const char* label,bool open,int active){
        const auto at=ImGui::GetCursorScreenPos();const float available=ImGui::GetContentRegionAvail().x;
        const bool clicked=ImGui::InvisibleButton(id,{available,rowHeight}),hovered=ImGui::IsItemHovered();auto* rowDraw=ImGui::GetWindowDrawList();
        const float highlight=std::max(open?.36f:active>0?.32f:0.f,hovered?.18f:0.f);const auto fill=cold::Color::mix(g_bodyColour,cold::ui::theme().accent,highlight);
        if(highlight>0)rowDraw->AddRectFilled(at,at+ImVec2(available,rowHeight),ImGui::ColorConvertFloat4ToU32({fill.r,fill.g,fill.b,g_menuPanelOpacity}));
        rowDraw->AddRect(at+ImVec2(1,1),at+ImVec2(available-1,rowHeight-1),hovered?MenuOutline(140):active>0?MenuOutline(80):IM_COL32(40,38,40,int(180*g_menuOutlineOpacity)));
        rowDraw->PushClipRect(at,at+ImVec2(available-27,rowHeight),true);rowDraw->AddText(at+ImVec2(5,(rowHeight-ImGui::GetFontSize())*.5f),active>0?IM_COL32(242,236,241,255):ImGui::ColorConvertFloat4ToU32({g_menuTextColour.r,g_menuTextColour.g,g_menuTextColour.b,1}),label);rowDraw->PopClipRect();
        if(active>=0)rowDraw->AddCircleFilled(at+ImVec2(available-22,rowHeight*.5f),1.8f,active?MenuAccent():IM_COL32(75,70,76,255));
        rowDraw->AddText(at+ImVec2(available-12,(rowHeight-ImGui::GetFontSize())*.5f),IM_COL32(213,205,214,255),open?"-":"+");
        const bool expand=ImGui::IsItemClicked(1)||(clicked&&(active<0||mouse.x>=at.x+available-17));if(clicked||expand)PlayMenuClick();return std::pair<bool,bool>{clicked&&!expand,expand};
    };
    auto [appearanceClicked,appearanceExpanded]=moduleRow("##appearance","Appearance",expanded==-2,-1);
    if(appearanceExpanded||appearanceClicked)expanded=expanded==-2?-1:-2;
    if(expanded==-2){
        ImGui::Dummy({0,7});ImGui::Indent(8);ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,5});
        if(hudModule){
            if(auto* setting=hudModule->setting("HUD scale")){float value=setting->number();if(DrawPrimordialValue("##hud-scale","HUD scale",value,.5f,3,2,1))setting->setNumber(value);}
            if(auto* setting=hudModule->setting("Text shadows")){bool value=setting->booleanValue();if(DrawPrimordialToggle("##hud-shadows","Text shadows",value))setting->setBoolean(value);}
        }
        float interval=config.getNumber("payload.hud","Refresh interval",.1f);if(DrawPrimordialValue("##hud-refresh","Refresh seconds",interval,.05f,1,2,.1f))config.setNumber("payload.hud","Refresh interval",interval,2);
        if(DrawPrimordialButton("##native-size","Minecraft size",ImGui::GetCursorScreenPos(),{PrimordialControlWidth(),22})){if(hudModule)if(auto* setting=hudModule->setting("HUD scale"))setting->setNumber(1);for(const auto& element:payloadHuds)config.setNumber(std::string("payload.")+element.key,"Scale",1,2);g_noticeScale=1;config.setNumber("interface","noticeScale",1,2);}
        bool snap=config.getBool("interface","hudSnapping",true);if(DrawPrimordialToggle("##hud-snapping","Snap to edges",snap))config.setBool("interface","hudSnapping",snap);
        ImGui::PopStyleVar();ImGui::Unindent(8);ImGui::Dummy({0,4});
    }
    for(int index=0;index<int(payloadHuds.size());++index){
        ImGui::PushID(index);const std::string key=payloadHuds[index].key,section="payload."+key;bool active=index==11?(notices?notices->booleanValue():true):config.getBool("payload.hud",key,index==0);
        const bool open=expanded==index;auto [toggled,expand]=moduleRow(key.c_str(),payloadHuds[index].label,open,active);
        if(toggled){if(index==11&&notices)notices->setBoolean(!active);else config.setBool("payload.hud",key,!active);if(!active)cold::modules::modules().queueEnabled("HUD",true);}
        if(expand)expanded=open?-1:index;
        if(scrollTo==index){ImGui::SetScrollHereY(0);scrollTo=-1;}
        if(expanded==index&&index==11){
            ImGui::Dummy({0,7});ImGui::Indent(8);ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,5});
            DrawPrimordialToggle("##notice-module","Module notices",g_moduleNotices);
            if(DrawPrimordialChoice("##notice-position","Position",{"Bottom right","Bottom left","Top right","Top left","Bottom center","Top center"},g_noticePosition))hud.resetNotificationPosition();
            DrawPrimordialValue("##notice-scale","Scale",g_noticeScale,.5f,3,2,1);
            DrawPrimordialValue("##notice-opacity","Opacity",g_noticeOpacity,0,1,2,.95f);
            DrawPrimordialValue("##notice-duration","Duration",g_noticeDuration,1,10,1,3);
            DrawPrimordialToggle("##notice-progress","Time indicator",g_noticeProgress);
            if(DrawPrimordialButton("##preview-notification","Preview notification",ImGui::GetCursorScreenPos(),{PrimordialControlWidth(),22}))hud.previewNotice();
            ImGui::PopStyleVar();ImGui::Unindent(8);ImGui::Dummy({0,4});
        } else if(expanded==index){
            ImGui::Dummy({0,7});ImGui::Indent(8);ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,5});ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+PrimordialControlWidth());
            float scaleValue=config.getNumber(section,"Scale",1);if(DrawPrimordialValue("##scale","Scale",scaleValue,.5f,3,2,1))config.setNumber(section,"Scale",scaleValue,2);
            int font=config.getInt(section,"Font",0);if(DrawPrimordialChoice("##font","Font",{"Default","Smooth","Minecraft","Rico pixel","Arial small","Outlined pixel"},font))config.setInt(section,"Font",font);
            int shadow=config.getInt(section,"Text shadow",0);if(DrawPrimordialChoice("##text-shadow","Text shadow",{"Default","Off","On"},shadow))config.setInt(section,"Text shadow",shadow);
            bool background=config.getBool(section,"Background",false);if(DrawPrimordialToggle("##background","Background",background))config.setBool(section,"Background",background);
            bool gradient=config.getBool(section,"Gradient",false);if(DrawPrimordialToggle("##gradient","Gradient text",gradient))config.setBool(section,"Gradient",gradient);
            float colour[]{config.getNumber(section,"Red",1),config.getNumber(section,"Green",1),config.getNumber(section,"Blue",1),config.getNumber(section,"Opacity",1)};
            if(DrawPrimordialColour("##colour","Colour",colour)){constexpr const char* names[]{"Red","Green","Blue","Opacity"};for(int c=0;c<4;++c)config.setNumber(section,names[c],colour[c],3);}
            const auto toggle=[&](const char* label,bool fallback){bool value=config.getBool(section,label,fallback);if(DrawPrimordialToggle(label,label,value))config.setBool(section,label,value);};
            if(index==0){char text[128];std::snprintf(text,sizeof(text),"%s",config.get(section,"Text","krux.lol").c_str());ImGui::SetNextItemWidth(PrimordialControlWidth());if(ImGui::InputText("##text",text,sizeof(text)))config.set(section,"Text",text);}
            if(index==3){toggle("Show label",true);toggle("Nether coordinates",true);toggle("Auto dimension",true);if(!config.getBool(section,"Auto dimension",true))toggle("Overworld to Nether",true);}
            if(index==5||index==10)toggle("km/h",false);
            if(index==6){toggle("Vertical",false);toggle("Durability",true);}
            if(index==9)for(const auto* axis:{"X","Y","Z"}){float value=config.getNumber(section,axis,std::string_view(axis)=="Y"?64:0);if(DrawPrimordialValue(axis,axis,value,-30000000,30000000,1,0))config.setNumber(section,axis,value,1);}
            if(index==7){toggle("Show label",true);toggle("Double equivalents",false);}
            if(index==8||index==9)toggle("Show label",true);
            if(index==10)for(const auto* metric:{"Speed","Vertical","FPS","Ping"})toggle(metric,true);
            if(index>=12&&index<=15)toggle("Graph",true);
            if(index==18){toggle("Real clock",false);if(config.getBool(section,"Real clock",false))toggle("Seconds",true);}
            if(DrawPrimordialButton("##reset-position","Reset position",ImGui::GetCursorScreenPos(),{PrimordialControlWidth(),22})){config.setNumber("payload.layout",key+"X",12,0);config.setNumber("payload.layout",key+"Y",38+index*12,0);config.setBool("payload.layout",key+"Right",false);}
            ImGui::PopTextWrapPos();ImGui::PopStyleVar();ImGui::Unindent(8);ImGui::Dummy({0,4});
        }
        ImGui::PopID();
    }
    contentHeight=ImGui::GetCursorPosY()-contentStart;ImGui::EndChild();}
    ImGui::End();ImGui::PopStyleVar(3);
}