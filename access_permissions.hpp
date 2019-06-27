#ifndef ACCESS_PERMISSIONS_HPP_INCLUDED
#define ACCESS_PERMISSIONS_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>

struct component;

struct access_permissions : serialisable, free_function
{
    enum state
    {
        STATE_NONE = 0,
        STATE_ALL,
        STATE_FRIENDLY, ///unimplemented
        STATE_SPECIFIED, ///unimplemented, specified friendlies by id?
    };

    state access = state::STATE_NONE;

    bool allowed(size_t ship_id)
    {
        return access == state::STATE_ALL;
    }

    void render_ui(component& parent);
};

#endif // ACCESS_PERMISSIONS_HPP_INCLUDED
