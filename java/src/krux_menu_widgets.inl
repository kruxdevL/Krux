#include "krux_ui_style.h"

static float PrimordialHoverMix(ImGuiID id,bool active,float speed=18.f) {
    float& value=*ImGui::GetStateStorage()->GetFloatRef(id,active?1.f:0.f);
    value+=(active?1.f:0.f-value)*(1.f-std::exp(-speed*ImGui::GetIO().DeltaTime));
    return value;
}

// Keep help legible independently of the scale of the menu that owns the hovered row.
void DrawPrimordialTooltip(const char* title,const char* body="",const char* hint="",float scale=1.f) {
    if(!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(7,5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,5));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,MenuBody());
    ImGui::PushStyleColor(ImGuiCol_Border,MenuAccent(150));
    ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32(224,219,222,255));
    ImGui::PushFont(ImGui::GetFont());
    const float wrap=std::clamp(ImGui::GetIO().DisplaySize.x-40.f,120.f,240.f);
    ImGui::SetNextWindowSizeConstraints({0,0},{wrap+14,FLT_MAX});
    if(ImGui::BeginTooltip()) {
        ImGui::SetWindowFontScale(std::clamp(scale,.85f,1.f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+wrap);
        ImGui::TextUnformatted(body&&*body?body:title);
        ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    ImGui::PopFont();ImGui::PopStyleColor(3);ImGui::PopStyleVar(4);
}

// Shared by Minecraft and native utilities so controls keep identical rendering and hit areas.
bool DrawPrimordialButton(const char* id, const char* label, const ImVec2& position,
    const ImVec2& size, bool selected = false) {
    ImGui::SetCursorScreenPos(position);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool pressed = ImGui::IsItemClicked();
    const float hover=PrimordialHoverMix(ImGui::GetItemID()^0x51C4E8A1u,hovered||selected);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const int shade=int(std::round(22.f+(selected?24.f:14.f)*hover));
    draw->AddRectFilled(position,position+size,IM_COL32(shade,shade,shade,255));
    draw->AddRect(position, position + size,
        selected ? MenuAccent(235) : hovered ? MenuAccent(150) : IM_COL32(45, 45, 45, 255));
    const ImVec2 text = ImGui::CalcTextSize(label);
    draw->AddText(position + ImVec2((size.x - text.x) * 0.5f, (size.y - text.y) * 0.5f),
        selected ? IM_COL32(239, 232, 235, 255) : IM_COL32(176, 173, 179, 255), label);
    if (pressed) PlayMenuClick();
    return pressed;
}

void DrawPrimordialDockIcon(ImDrawList* draw, int index, const ImVec2& center, ImU32 colour) {
    constexpr float radius = 8.0f;
    switch (index) {
    case 0: // Combat
        draw->AddCircle(center, radius, colour, 16, 1.5f);
        draw->AddCircleFilled(center, 2.2f, colour);
        draw->AddLine(center - ImVec2(12.0f, 0.0f), center + ImVec2(12.0f, 0.0f), colour, 1.5f);
        draw->AddLine(center - ImVec2(0.0f, 12.0f), center + ImVec2(0.0f, 12.0f), colour, 1.5f);
        break;
    case 1: // Move
        draw->AddLine(center - ImVec2(10.0f, 7.0f), center + ImVec2(10.0f, -7.0f), colour, 2.0f);
        draw->AddLine(center + ImVec2(10.0f, -7.0f), center + ImVec2(4.0f, -8.0f), colour, 2.0f);
        draw->AddLine(center + ImVec2(10.0f, -7.0f), center + ImVec2(8.0f, -1.0f), colour, 2.0f);
        draw->AddLine(center - ImVec2(10.0f, 7.0f), center - ImVec2(4.0f, 8.0f), colour, 2.0f);
        break;
    case 2: // Visuals
        draw->AddBezierQuadratic(center - ImVec2(12.0f, 0.0f), center,
            center + ImVec2(12.0f, 0.0f), colour, 1.5f, 12);
        draw->AddBezierQuadratic(center - ImVec2(12.0f, 0.0f), center + ImVec2(0.0f, 13.0f),
            center + ImVec2(12.0f, 0.0f), colour, 1.5f, 12);
        draw->AddCircleFilled(center, 3.5f, colour);
        break;
    case 6: // Cosmetics
        for(float side:{-1.f,1.f}) {
            draw->AddLine(center,center+ImVec2(side*12,-8),colour,1.5f);
            draw->AddLine(center+ImVec2(side*12,-8),center+ImVec2(side*9,5),colour,1.5f);
            draw->AddLine(center+ImVec2(side*9,5),center,colour,1.5f);
            draw->AddLine(center+ImVec2(side*5,-3),center+ImVec2(side*9,5),colour,1.f);
        }
        break;
    case 5: // DonutSMP
        draw->AddCircle(center, 10, colour, 24, 2);
        draw->AddCircle(center, 4, colour, 16, 1.5f);
        break;
    case 3: // Player
        draw->AddCircle(center - ImVec2(0.0f, 6.0f), 4.0f, colour, 12, 1.5f);
        draw->PathArcTo(center + ImVec2(0.0f, 7.0f), 8.5f, 3.4f, 5.98f, 14);
        draw->PathStroke(colour, 0, 1.5f);
        break;
    default: // Misc
        draw->AddCircle(center, 7.0f, colour, 12, 1.5f);
        for (int spoke = 0; spoke < 8; ++spoke) {
            const float angle = static_cast<float>(spoke) * 0.78539816f;
            const ImVec2 direction(std::cos(angle), std::sin(angle));
            draw->AddLine(center + direction * 9.0f, center + direction * 12.0f, colour, 1.5f);
        }
        draw->AddCircleFilled(center, 2.3f, colour);
        break;
    }
}

bool DrawPrimordialDock(const char* id, const char* label, int icon, const ImVec2& position,
                        const ImVec2& size, bool selected) {
    ImGui::SetCursorScreenPos(position);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool pressed = ImGui::IsItemClicked();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 fill = selected ? IM_COL32(43, 43, 43, 255)
                                : hovered ? IM_COL32(38, 38, 38, 255)
                                          : IM_COL32(30, 30, 30, 0);
    const ImU32 border = selected ? IM_COL32(59, 59, 59, 255) : IM_COL32(0, 0, 0, 0);
    const ImU32 text = selected ? IM_COL32(232, 230, 232, 255) : IM_COL32(144, 142, 144, 255);
    draw->AddRectFilled(position, position + size, fill, 8.0f);
    if (selected) draw->AddRect(position, position + size, border, 8.0f);
    static constexpr const char* icons[] = { "a", "b", "c", "d", "e" };
    if (g_primordialIcons && icon >= 0 && icon < static_cast<int>(std::size(icons))) {
        const ImVec2 glyph = g_primordialIcons->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, icons[icon]);
        draw->AddText(g_primordialIcons, 20.0f,
            position + ImVec2((size.x - glyph.x) * 0.5f, 6.0f), text, icons[icon]);
    } else {
        DrawPrimordialDockIcon(draw, icon, position + ImVec2(size.x * 0.5f, 16.0f), text);
    }
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    draw->AddText(position + ImVec2((size.x - textSize.x) * 0.5f, 31.0f), text, label);
    if (pressed) PlayMenuClick();
    return pressed;
}

void DrawPrimordialPanel(const char* title, const ImVec2& position, const ImVec2& size) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(position, position + size, MenuBody(), g_cornerRadius);
    draw->AddRect(position, position + size, IM_COL32(24, 24, 24, 255), g_cornerRadius);
    draw->AddText(position + ImVec2(12.0f, 10.0f), IM_COL32(220, 215, 219, 255), title);
    draw->AddLine(position + ImVec2(1.0f, 31.0f), position + ImVec2(size.x - 1.0f, 31.0f),
        MenuAccent(205));
}

static float g_primordialWidthOverride=0;
float PrimordialControlWidth() {
    if(g_primordialWidthOverride>0)return g_primordialWidthOverride;
    const float available = ImGui::GetContentRegionAvail().x;
    // Child windows own the scrollbar. Keeping this gutter explicit prevents
    // numeric controls from visually touching it at narrow panel widths.
    return (std::max)(1.0f, available - 8.0f);
}

bool DrawPrimordialToggle(const char* id, const char* label, bool& value) {
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 size(PrimordialControlWidth(), 18.0f);
    ImGui::InvisibleButton(id, size);
    const bool changed = ImGui::IsItemClicked();
    if (changed) { value = !value; PlayMenuClick(); }
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 check = position + ImVec2(0.0f, 3.0f);
    draw->AddRectFilled(check, check + ImVec2(11.0f, 11.0f), value ? MenuAccent() : IM_COL32(25, 25, 25, 255), 2.0f);
    draw->AddRect(check, check + ImVec2(11.0f, 11.0f), hovered ? MenuAccent(200) : IM_COL32(47, 47, 47, 255), 2.0f);
    if (value) {
        draw->AddLine(check + ImVec2(2.0f, 5.0f), check + ImVec2(4.5f, 8.0f), IM_COL32(40, 35, 38, 255), 1.4f);
        draw->AddLine(check + ImVec2(4.5f, 8.0f), check + ImVec2(9.0f, 2.0f), IM_COL32(40, 35, 38, 255), 1.4f);
    }
    draw->PushClipRect(position + ImVec2(18.0f, 0.0f), position + size, true);
    draw->AddText(position + ImVec2(18.0f, 1.0f), hovered ? IM_COL32(234, 230, 233, 255) : IM_COL32(180, 176, 180, 255), label);
    draw->PopClipRect();
    if (hovered) DrawPrimordialTooltip(label);
    return changed;
}

static bool g_sliderWheelConsumed=false;

bool DrawPrimordialValue(const char* id, const char* label, float& value, float minimum,
    float maximum, int decimals, float defaultValue) {
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float width = PrimordialControlWidth();
    ImGui::InvisibleButton(id, ImVec2(width, 33.0f));
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    bool changed = false;
    if(hovered && ImGui::GetIO().KeyAlt) {
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
        if(ImGui::GetIO().MouseWheel!=0) {
            g_sliderWheelConsumed=true;
            float& remainder=*ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID(id)^0x67A3B129u);
            remainder+=ImGui::GetIO().MouseWheel;
            const int steps=int(remainder);
            remainder-=float(steps);
            const float quantum=std::pow(10.f,-float(std::clamp(decimals,0,6)));
            const float requested=std::clamp(value+steps*quantum,minimum,maximum);
            if(requested!=value){value=requested;changed=true;}
        }
    }
    if(hovered&&ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {value=std::clamp(defaultValue,minimum,maximum);changed=true;PlayMenuClick();}
    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float fraction = std::clamp((ImGui::GetIO().MousePos.x - position.x) / width, 0.0f, 1.0f);
        const float requested = minimum + (maximum - minimum) * fraction;
        if (requested != value) { value = requested; changed = true; }
    }
    char formatted[32]{};
    std::snprintf(formatted, sizeof(formatted), "%.*f", decimals, value);
    const ImVec2 valueSize = ImGui::CalcTextSize(formatted);
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(position, position + ImVec2(width - valueSize.x - 9.0f, 16.0f), true);
    draw->AddText(position, IM_COL32(187, 183, 187, 255), label);
    draw->PopClipRect();
    draw->AddText(position + ImVec2(width - valueSize.x, 0), IM_COL32(139, 135, 140, 255), formatted);
    const ImVec2 track = position + ImVec2(0, 19);
    draw->AddRectFilled(track, track + ImVec2(width, 8), IM_COL32(27, 27, 27, 255), 3.0f);
    draw->AddRect(track, track + ImVec2(width, 8), IM_COL32(45, 45, 45, 255), 3.0f);
    const float fraction = maximum > minimum ? std::clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f) : 0.0f;
    const float thumb = track.x + 2.0f + (width - 4.0f) * fraction;
    if (thumb > track.x + 2.0f) draw->AddRectFilled(track + ImVec2(1, 1), ImVec2(thumb, track.y + 7), MenuAccent(100), 2.0f);
    draw->AddRectFilled(ImVec2(thumb - 1.5f, track.y), ImVec2(thumb + 1.5f, track.y + 8),
        active || hovered ? MenuAccent() : IM_COL32(181, 174, 179, 255), 1.0f);
    if (hovered) DrawPrimordialTooltip(label,formatted,"Alt + scroll to adjust; right click to reset");
    return changed;
}
bool DrawPrimordialChoice(const char* id, const char* label, const std::vector<std::string>& choices, int& value) {
    const auto start=ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(label);
    ImGui::SetCursorScreenPos({start.x,start.y+ImGui::GetTextLineHeight()+4.f});
    const float width=PrimordialControlWidth();
    ImGui::SetNextItemWidth(width);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{6,4});
    const float controlHeight=ImGui::GetFrameHeight();
    const char* preview = value >= 0 && value < static_cast<int>(choices.size()) ? choices[value].c_str() : "none";
    bool changed = false;
    const float fontScale=ImGui::GetFontSize()/ImGui::GetFont()->FontSize;
    PushKruxPopupStyle();
    ImGui::SetNextWindowSizeConstraints({width,0},{width,std::min(260.f,std::max(40.f,ImGui::GetIO().DisplaySize.y-16))});
    if (ImGui::BeginCombo(id, preview)) {
        ImGui::SetWindowFontScale(fontScale);
        for (int index = 0; index < static_cast<int>(choices.size()); ++index) {
            ImGui::PushID(index);const auto at=ImGui::GetCursorScreenPos();const float row=std::max(18.f,ImGui::GetTextLineHeight()+6);
            const bool selected=ImGui::Selectable("##choice",value==index,0,{0,row});
            const bool hovered=ImGui::IsItemHovered();const float hover=PrimordialHoverMix(ImGui::GetItemID()^0x6432B7D1u,hovered||value==index);
            if(hover>0.002f){const auto tint=cold::Color::mix(g_bodyColour,cold::ui::theme().accent,(value==index?.28f:.16f)*hover);ImGui::GetWindowDrawList()->AddRectFilled(at,at+ImVec2(ImGui::GetContentRegionAvail().x,row),ImGui::ColorConvertFloat4ToU32({tint.r,tint.g,tint.b,g_menuPanelOpacity}));}
            if(value==index)ImGui::GetWindowDrawList()->AddRectFilled(at,at+ImVec2(2,row),MenuAccent(220));
            ImGui::GetWindowDrawList()->AddText({at.x,KruxCenteredTextY(at.y,row,choices[index].c_str())},ImGui::GetColorU32(ImGuiCol_Text),choices[index].c_str());
            if (selected) {
                value = index;
                changed = true;
                PlayMenuClick();
            }
            if(value==index)ImGui::SetItemDefaultFocus();ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    PopKruxPopupStyle();
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos({start.x,std::max(ImGui::GetCursorScreenPos().y,start.y+ImGui::GetTextLineHeight()+4.f+controlHeight+7.f)});
    return changed;
}

bool DrawPrimordialColour(const char* id, const char* label, float value[4], bool alpha = true, bool compact = false, float compactWidth = 20.f) {
    ImGui::PushID(id);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float swatchWidth=compact?compactWidth:20.f;
    const float width = compact?swatchWidth:PrimordialControlWidth();
    const bool pressed = ImGui::InvisibleButton("##colour", ImVec2(width, 18.0f));
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    if(!compact){
        draw->PushClipRect(position, position + ImVec2(width - 27.0f, 18.0f), true);
        draw->AddText(position + ImVec2(0, 1), ImGui::GetColorU32(ImGuiCol_Text), label);
        draw->PopClipRect();
    }
    const ImVec2 swatch = position + ImVec2(width - swatchWidth, 5.0f);
    draw->AddRectFilled(swatch - ImVec2(1, 1), swatch + ImVec2(swatchWidth+1, 9), IM_COL32(17, 17, 17, 255), 2.0f);
    const int firstVertex = draw->VtxBuffer.Size;
    draw->AddRectFilled(swatch, swatch + ImVec2(swatchWidth, 8), IM_COL32_WHITE, 1.0f);
    const float opacity = alpha ? std::clamp(value[3], 0.0f, 1.0f) : 1.0f;
    const auto shade = [&](float brightness, float lift) {
        return ImGui::GetColorU32(ImVec4(
            std::clamp((value[0] * brightness + lift) * opacity + 0.12f * (1.0f - opacity), 0.0f, 1.0f),
            std::clamp((value[1] * brightness + lift) * opacity + 0.12f * (1.0f - opacity), 0.0f, 1.0f),
            std::clamp((value[2] * brightness + lift) * opacity + 0.12f * (1.0f - opacity), 0.0f, 1.0f), 1.0f));
    };
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, firstVertex, draw->VtxBuffer.Size,
        swatch, swatch + ImVec2(0, 8), shade(0.85f, 0.15f), shade(0.68f, 0.0f));
    if (hovered) draw->AddRect(swatch - ImVec2(1, 1), swatch + ImVec2(swatchWidth+1, 9), MenuAccent(180), 2.0f);
    if (pressed) {
        PlayMenuClick();
        ImGui::OpenPopup("##picker");
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        // The menu is a separate native window, so popups must fit its client area.
        ImGui::SetNextWindowPos(ImVec2(std::clamp(swatch.x - 192.0f, 8.0f, (std::max)(8.0f, display.x - 224.0f)),
            std::clamp(swatch.y + 16.0f, 8.0f, (std::max)(8.0f, display.y - 242.0f))));
    }
    bool changed = false;
    const float fontScale=ImGui::GetFontSize()/ImGui::GetFont()->FontSize;
    PushKruxPopupStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));
    if (ImGui::BeginPopup("##picker")) {
        ImGui::SetWindowFontScale(fontScale);
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(196.0f);
        const ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoSidePreview
            | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_PickerHueBar
            | (alpha ? ImGuiColorEditFlags_AlphaBar : ImGuiColorEditFlags_NoAlpha);
        changed = ImGui::ColorPicker4("##value", value, flags);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    PopKruxPopupStyle();
    ImGui::PopID();
    return changed;
}

bool DrawPrimordialRailItem(const char* id, const char* label, const ImVec2& size, bool selected, int enabled = -1, const char* binding = nullptr) {
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const float hover=PrimordialHoverMix(ImGui::GetItemID()^0x31D6A51Fu,hovered||selected);
    auto* draw = ImGui::GetWindowDrawList();
    if (hover>0.002f) {
        const int leftShade=int(std::round(34.f+(selected?12.f:5.f)*hover));
        const ImU32 left=IM_COL32(leftShade,leftShade,leftShade,255);
        const ImU32 right=IM_COL32(34,34,34,255);
        draw->AddRectFilledMultiColor(position, position + size, left, right, right, left);
    }
    if (selected) draw->AddRectFilled(position, position + ImVec2(2, size.y), MenuAccent());
    if (enabled >= 0) draw->AddCircleFilled(position + ImVec2(size.x - 8, size.y * 0.5f), 2.5f,
        enabled ? MenuAccent() : IM_COL32(76,73,75,255));
    const float textSize = ImGui::GetFont()->RasterSize(ImGui::GetFontSize() * 0.75f);
    const float keyWidth = binding ? (std::min)(size.x * 0.42f, ImGui::GetFont()->CalcTextSizeA(textSize, FLT_MAX, 0, binding).x) : 0;
    const float right = position.x + size.x - (enabled >= 0 ? 17.0f : 5.0f);
    const float keyX = right - keyWidth;
    draw->PushClipRect(position + ImVec2(11, 0), ImVec2(binding ? keyX - 6 : right, position.y + size.y), true);
    draw->AddText(position + ImVec2(11, (size.y - ImGui::GetFontSize()) * 0.5f),
        selected ? IM_COL32(227, 224, 226, 255) : IM_COL32(144, 144, 144, 255), label);
    draw->PopClipRect();
    if (binding) {
        draw->PushClipRect(ImVec2(keyX, position.y), ImVec2(right, position.y + size.y), true);
        draw->AddText(ImGui::GetFont(), textSize, ImVec2(keyX, position.y + (size.y - textSize) * 0.5f),
            IM_COL32(160, 149, 153, 255), binding);
        draw->PopClipRect();
    }
    if (pressed) PlayMenuClick();
    return pressed;
}