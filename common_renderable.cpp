#include "common_renderable.hpp"
#include <networking/serialisable.hpp>

void common_renderable::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(type);
    DO_SERIALISE(r);
    DO_SERIALISE(velocity);
    DO_SERIALISE(is_unknown);
}
