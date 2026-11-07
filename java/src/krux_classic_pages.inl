void RenderClassicPreferenceSettings(bool mainMenu) {
    if(mainMenu) {
        auto& config=cold::modules::modules().config();
        float mainScale=config.getNumber("interface","mainMenuScale",1);
        if(DrawPrimordialValue("##title-scale","Scale",mainScale,.65f,1.4f,2,1))config.setNumber("interface","mainMenuScale",mainScale,2);
        bool skin=config.getBool("interface","mainMenuSkin",true),animation=config.getBool("interface","mainMenuAnimation",true);
        if(DrawPrimordialToggle("##skin","Player skin",skin))config.setBool("interface","mainMenuSkin",skin);
        if(DrawPrimordialToggle("##animation","Animate skin",animation))config.setBool("interface","mainMenuAnimation",animation);
        ImGui::Spacing();ImGui::Separator();ImGui::TextUnformatted("Audio");
        DrawPrimordialToggle("##music","Play music",g_menuMusic);DrawPrimordialToggle("##loop","Loop track",g_musicLoop);
        DrawPrimordialValue("##volume","Volume",g_musicVolume,0,1,2,.22f);DrawPrimordialValue("##fade","Fade seconds",g_musicFade,0,5,1,1.5f);
    } else {
        auto& fontConfig=cold::modules::modules().config();
        const std::array<std::pair<const char*,krux::fonts::Domain>,3> fontChoices{{{"ClickGUI font",krux::fonts::Domain::ClickGui},{"HUD font",krux::fonts::Domain::Hud},{"World labels / nametags font",krux::fonts::Domain::WorldLabels}}};
        for(const auto& [label,domain]:fontChoices){int selected=krux::fonts::preference(fontConfig,domain);const std::string id=std::string("##")+krux::fonts::preferenceKey(domain);if(DrawPrimordialChoice(id.c_str(),label,{"Smooth","Minecraft","Rico pixel","Arial small","Outlined pixel"},selected)){fontConfig.setInt("interface",krux::fonts::preferenceKey(domain),selected);SaveInterface();}}
        if(DrawPrimordialValue("##text-scale","ClickGUI font scale",g_textScale,.75f,2.f,2,1))SaveInterface();
        bool shareUsage=fontConfig.getBool("interface","shareModuleUsage",false);
        if(DrawPrimordialToggle("##share-module-usage","Share module usage",shareUsage)){fontConfig.setBool("interface","shareModuleUsage",shareUsage);krux::usage::setSharing(shareUsage);SaveInterface();}
        if(ImGui::IsItemHovered())DrawPrimordialTooltip("Counts and enabled time only; no player, server or location data.");
        DrawPrimordialToggle("##clicks","Keyboard clicks",g_clickSound);
        if(ImGui::Button("Reset column positions",{PrimordialControlWidth(),24}))g_resetClassicLayout=true;
        ImGui::Dummy({0,7});ImGui::TextUnformatted("Appearance");ImGui::Separator();
        const auto colour=[](const char* id,const char* label,cold::Color& value){float rgba[]{value.r,value.g,value.b,1};if(DrawPrimordialColour(id,label,rgba,false))value={rgba[0],rgba[1],rgba[2],1};};
        auto accent=cold::ui::theme().accent;colour("##accent","Accent",accent);cold::ui::setAccent(accent);
        colour("##background","Background",g_bodyColour);colour("##header","Headers",g_headerColour);colour("##text","Text",g_menuTextColour);
        DrawPrimordialValue("##rows","Module row height",g_moduleRowHeight,18,34,0,22);
        DrawPrimordialValue("##column-width","Category width",g_classicColumnWidth,140,300,0,196);
        DrawPrimordialValue("##header-height","Category header height",g_classicHeaderHeight,18,36,0,24);
        DrawPrimordialValue("##panel-opacity","Panel opacity",g_menuPanelOpacity,.25f,1,2,1);
        DrawPrimordialValue("##outline-opacity","Panel outline opacity",g_menuOutlineOpacity,0,1,2,1);
        DrawPrimordialValue("##menu-dim","Menu background dim",g_menuBackdrop,0,.85f,2,.32f);
        DrawPrimordialToggle("##dim-panels","Dim inactive panels",g_dimInactivePanels);
        if(g_dimInactivePanels)DrawPrimordialValue("##inactive-brightness","Inactive brightness",g_classicInactiveBrightness,.15f,.8f,2,.42f);
        if(DrawPrimordialButton("##reset-menu-appearance","Reset menu appearance",ImGui::GetCursorScreenPos(),{PrimordialControlWidth(),24})) {
            g_classicColumnWidth=196;g_classicHeaderHeight=24;g_menuPanelOpacity=g_menuOutlineOpacity=1;g_moduleRowHeight=22;g_menuBackdrop=.32f;g_dimInactivePanels=true;g_classicInactiveBrightness=.42f;
            g_bodyColour=cold::Color::rgb(0x101010);g_headerColour=cold::Color::rgb(0x151515);g_menuTextColour=cold::Color::rgb(0xBABABA);cold::ui::setAccent(cold::Color::rgb(0xC19AA4));SaveInterface();
        }

    }
}

void RenderClassicUtilityPage() {
    const auto screen=ImGui::GetIO().DisplaySize;
    const float contentWidth=std::max(260.f,screen.x-132.f);
    const bool compact=screen.x<900||screen.y<500;
    const int columns=compact?1:3;
    const float width=std::min(compact?360.f:290.f,(contentWidth-32-(columns-1)*12)/columns);
    const float height=std::min(350.f,screen.y-(compact?118:80));
    const float left=std::max(12.f,(contentWidth-(width*columns+12*(columns-1)))*.5f);
    ImGui::SetNextWindowPos({0,52});ImGui::SetNextWindowSize({screen.x,screen.y-52});
    ImGui::Begin("##classic-utility",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);
    static int page=0;bool panelOpen=false;
    if(compact) {
        const char* labelsConfigs[]={"Browse","Actions","Info / share"};
        for(int index=0;index<3;++index)if(DrawPrimordialButton((std::string("##utility-tab")+std::to_string(index)).c_str(),labelsConfigs[index],{left+index*(width/3),68},{width/3-4,26},page==index))page=index;
    }
    const auto panel=[&](const char* id,const char* title,int index) {
        panelOpen=!compact||index==page;if(!panelOpen)return false;
        const ImVec2 at{left+(compact?0:index)*(width+12),compact?106.f:68.f},extent{width,height-12};
        auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(at,at+extent,MenuBody());
        draw->AddRectFilled(at,at+ImVec2(width,24),MenuHeader());draw->AddRect(at,at+extent,MenuOutline(100));draw->AddText(at+ImVec2(8,(24-ImGui::GetFontSize())*.5f),MenuAccent(),title);
        ImGui::SetCursorScreenPos(at+ImVec2(10,32));ImGui::BeginChild(id,extent-ImVec2(20,42),false,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar);
        ImGui::SetWindowFontScale(g_textScale);ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+PrimordialControlWidth());return true;
    };
    const auto end=[&] {if(panelOpen){ImGui::PopTextWrapPos();ImGui::EndChild();panelOpen=false;}};
    RenderCloudConfigContent(panel,end);
    ImGui::End();
}