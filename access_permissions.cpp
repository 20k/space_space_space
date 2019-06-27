#include "access_permissions.hpp"
#include "ship_components.hpp"
#include "ui_util.hpp"

void access_permissions::render_ui(component& parent)
{
    std::string str;

    if(access == STATE_NONE)
        str = "(N)";
    if(access == STATE_ALL)
        str = "(Y)";

    if(ImGuiX::SimpleButton(str.c_str()))
    {
        if(access == STATE_NONE)
            access = STATE_ALL;
        else if(access == STATE_ALL)
            access = STATE_NONE;
    }
}
